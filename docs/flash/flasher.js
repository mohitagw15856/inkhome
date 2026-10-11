// Inkhome web flasher: installs Inkhome beside the stock firmware and switches
// between the two, over WebSerial, with nothing to download or build.
//
// Adapted from crosspoint-tools (MIT, (c) 2025 SoFriendly), src/lib/flasher.js:
// partition-table and otadata parsing, the otadata CRC, image validation and
// the connect/disconnect sequence. Inkhome's change: the target slot is chosen
// by identifying stock firmware, never by "whichever slot is not running", so
// installing or updating twice cannot overwrite stock.

import { ESPLoader, Transport } from './esptool.bundle.js';

const OTA_SECTOR = 0x1000;
const OTADATA_BYTES = 2 * OTA_SECTOR;
const OTA_STATE_NEW = 0;
const OTA_STATE_INVALID = 3;
const OTA_STATE_ABORTED = 4;
const IMAGE_MAGIC = 0xe9;
const APP_DESC_MAGIC = 0xabcd5432;
const XTEINK_USB = [{ usbVendorId: 0x303a, usbProductId: 0x1001 }];

// ---------------------------------------------------------------- bytes and CRC

const CRC_TABLE = new Uint32Array(256);
for (let i = 0; i < 256; i++) {
  let c = i;
  for (let j = 0; j < 8; j++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  CRC_TABLE[i] = c >>> 0;
}

// Equals esp_rom_crc32_le(UINT32_MAX, data, len), as ESP-IDF stores in otadata.
function crc32Le(data) {
  let crc = 0;
  for (const b of data) crc = CRC_TABLE[(crc ^ b) & 0xff] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}

const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
const u32Bytes = (v) => new Uint8Array([v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff]);
const cString = (b, o, n) => new TextDecoder().decode(b.subarray(o, o + n)).split('\0')[0];
const seqCrc = (seq) => crc32Le(u32Bytes(seq));

// ---------------------------------------------------------------- partition table and otadata

export function parsePartitionTable(data) {
  let otadata = null;
  const apps = [];
  for (let o = 0; o + 32 <= data.length; o += 32) {
    const row = data.subarray(o, o + 32);
    if (row.every((b) => b === 0xff)) break;
    if (row[0] === 0xeb && row[1] === 0xeb) continue; // MD5 row
    const type = row[2];
    const subtype = row[3];
    const p = { offset: u32(row, 4), size: u32(row, 8), label: cString(row, 12, 16) };
    if (type === 0x01 && subtype === 0x00) otadata = p;
    if (type === 0x00 && (subtype === 0x10 || subtype === 0x11)) apps[subtype - 0x10] = p;
  }
  if (!otadata || !apps[0] || !apps[1]) throw new Error('This device has no two-slot partition table, so stock cannot be kept beside Inkhome.');
  for (const p of [otadata, apps[0], apps[1]]) {
    const end = (p.offset + p.size) >>> 0;
    if (p.offset < 0x9000 || end < p.offset || end > 0x1000000) throw new Error(`Partition ${p.label} is outside the safe flash range.`);
  }
  return { otadata, apps };
}

export function parseOtadata(data) {
  const entries = [0, OTA_SECTOR].map((off) => {
    const seq = u32(data, off);
    const state = u32(data, off + 0x18);
    const crcOk = u32(data, off + 0x1c) === seqCrc(seq);
    return { seq, eligible: seq !== 0xffffffff && crcOk && state !== OTA_STATE_INVALID && state !== OTA_STATE_ABORTED };
  });
  let activeSector = -1;
  let activeSeq = 0;
  entries.forEach((e, i) => {
    if (e.eligible && (activeSector < 0 || e.seq > activeSeq)) {
      activeSector = i;
      activeSeq = e.seq;
    }
  });
  // Bootloader rule: no valid entry boots app0; otherwise app (seq - 1) % 2.
  const activeApp = activeSector < 0 ? 0 : (activeSeq - 1) % 2;
  return { activeSector, activeSeq, activeApp };
}

// New 4 KB sector selecting `targetApp`. Written to the sector that is not
// active, so a power cut leaves the old entry as a valid fallback.
export function buildOtadataSector(otaRaw, ota, targetApp) {
  let seq = ota.activeSeq + 1;
  while ((seq - 1) % 2 !== targetApp) seq++;
  const sector = ota.activeSector < 0 ? 0 : 1 - ota.activeSector;
  const data = new Uint8Array(otaRaw.subarray(sector * OTA_SECTOR, (sector + 1) * OTA_SECTOR));
  data.set(u32Bytes(seq), 0);
  data.set(u32Bytes(OTA_STATE_NEW), 0x18);
  data.set(u32Bytes(seqCrc(seq)), 0x1c);
  return { sector, seq, data };
}

// ---------------------------------------------------------------- image checks

// esp_app_desc_t sits at image offset 0x20: magic, then version at +0x10,
// project name at +0x30, build time/date at +0x50/+0x60, IDF version at +0x70.
export function identifyImage(head) {
  if (head[0] !== IMAGE_MAGIC) return { kind: 'empty', label: 'Empty' };
  if (u32(head, 0x20) !== APP_DESC_MAGIC) return { kind: 'other', label: 'Unknown firmware' };
  const version = cString(head, 0x30, 32);
  const project = cString(head, 0x50, 32);
  const date = cString(head, 0x80, 16);
  if (project === 'inkhome') return { kind: 'inkhome', label: `Inkhome ${version}`, version, project, date };
  if (/crosspoint/i.test(project)) return { kind: 'crosspoint', label: `CrossPoint ${version}`, version, project, date };
  // Xteink's stock builds are produced by Espressif's arduino-lib-builder.
  if (project === 'arduino-lib-builder') return { kind: 'stock', label: `Stock firmware (built ${date})`, version, project, date };
  return { kind: 'other', label: `${project} ${version}`.trim(), version, project, date };
}

export async function validateImage(data) {
  if (data.length < 24 || data[0] !== IMAGE_MAGIC) throw new Error('The firmware file is not an ESP32 app image.');
  const hashAppended = (data[23] & 1) === 1;
  let xor = 0xef;
  let pos = 24;
  for (let i = 0; i < data[1]; i++) {
    if (data.length - pos < 8) throw new Error('Firmware file is truncated (segment header).');
    const len = u32(data, pos + 4);
    pos += 8;
    if (len > data.length - pos) throw new Error('Firmware file is truncated (segment data).');
    for (let j = pos; j < pos + len; j++) xor ^= data[j];
    pos += len;
  }
  const padEnd = (pos + 16) & ~15;
  if (padEnd + (hashAppended ? 32 : 0) !== data.length) throw new Error('Firmware file size does not match its header.');
  if ((xor & 0xff) !== data[padEnd - 1]) throw new Error('Firmware checksum mismatch: the download is corrupt.');
  if (hashAppended) {
    const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', data.subarray(0, data.length - 32)));
    if (!digest.every((b, i) => b === data[data.length - 32 + i])) throw new Error('Firmware SHA-256 mismatch: the download is corrupt.');
  }
}

// Which slot an install or update writes. Never a slot holding stock (or any
// firmware we cannot identify) while another choice exists.
export function planInstall(device) {
  const [a, b] = device.slots;
  const replaceable = (s) => s.kind === 'empty' || s.kind === 'inkhome' || s.kind === 'crosspoint';
  const protectedKind = (s) => s.kind === 'stock' || s.kind === 'other';
  const inkhomeSlot = device.slots.findIndex((s) => s.kind === 'inkhome');
  if (inkhomeSlot >= 0 && protectedKind(device.slots[1 - inkhomeSlot])) {
    return { target: inkhomeSlot, note: `Updates Inkhome in ${device.apps[inkhomeSlot].label}. The stock firmware in ${device.apps[1 - inkhomeSlot].label} is kept.` };
  }
  if (replaceable(a) !== replaceable(b)) {
    const target = replaceable(a) ? 0 : 1;
    return { target, note: `Installs into ${device.apps[target].label}. ${device.slots[1 - target].label} in ${device.apps[1 - target].label} is kept.` };
  }
  if (protectedKind(a) && protectedKind(b)) {
    // Both slots hold stock (an older copy plus the running one): replace the copy that is not running.
    const target = 1 - device.activeApp;
    return {
      target,
      warn: `Both slots hold stock firmware. Inkhome replaces the copy in ${device.apps[target].label}; the stock firmware you are running now (${device.apps[device.activeApp].label}) is kept.`,
    };
  }
  // Neither slot holds stock: nothing to keep, so update or fill the slot that is not running.
  const target = inkhomeSlot >= 0 ? inkhomeSlot : 1 - device.activeApp;
  return { target, warn: 'No stock firmware was found on this device, so switching back to stock will not be possible.' };
}

// ---------------------------------------------------------------- device session

export class Device {
  constructor(log) {
    this.log = log;
    this.loader = null;
  }

  static supported() {
    return 'serial' in navigator;
  }

  // Call straight from a click handler: the port picker needs the user gesture.
  async connect() {
    const port = await navigator.serial.requestPort({ filters: XTEINK_USB }).catch(() => navigator.serial.requestPort());
    const transport = new Transport(port, false);
    this.loader = new ESPLoader({ transport, baudrate: 921600, romBaudrate: 115200, enableTracing: false });
    await this.loader.main();
    const chip = this.loader.chip?.CHIP_NAME || '';
    if (!/ESP32-C3/i.test(chip)) {
      await this.disconnect();
      throw new Error(`This is an ${chip || 'unknown chip'}, not an Xteink X3 or X4 (ESP32-C3). Nothing was written.`);
    }
    this.log(`Connected to ${chip}.`);
  }

  async inspect() {
    const table = parsePartitionTable(await this.loader.readFlash(0x8000, 0x1000));
    const otaRaw = await this.loader.readFlash(table.otadata.offset, OTADATA_BYTES);
    const ota = parseOtadata(otaRaw);
    const slots = [];
    for (const app of table.apps) slots.push(identifyImage(await this.loader.readFlash(app.offset, 0x100)));
    this.state = { ...table, otaRaw, ota, slots, activeApp: ota.activeApp };
    return this.state;
  }

  async writeImage(appIndex, image, onProgress) {
    const slot = this.state.apps[appIndex];
    if (image.length > slot.size) throw new Error(`Inkhome (${image.length} bytes) does not fit ${slot.label} (${slot.size} bytes).`);
    await this.loader.writeFlash({
      fileArray: [{ data: this.loader.ui8ToBstr(image), address: slot.offset }],
      flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep', eraseAll: false, compress: true,
      reportProgress: (_, written, total) => onProgress?.(written / total),
    });
    const check = identifyImage(await this.loader.readFlash(slot.offset, 0x100));
    if (check.kind !== 'inkhome') throw new Error('Read-back after writing did not find Inkhome. The boot selection was not changed.');
  }

  // Selects which slot boots next. No-op when it already boots.
  async bootApp(appIndex) {
    const { otadata, otaRaw, ota } = this.state;
    if (ota.activeSector >= 0 && ota.activeApp === appIndex) return false;
    const next = buildOtadataSector(otaRaw, ota, appIndex);
    await this.loader.writeFlash({
      fileArray: [{ data: this.loader.ui8ToBstr(next.data), address: otadata.offset + next.sector * OTA_SECTOR }],
      flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep', eraseAll: false, compress: true,
    });
    const verify = parseOtadata(await this.loader.readFlash(otadata.offset, OTADATA_BYTES));
    if (verify.activeApp !== appIndex || verify.activeSeq !== next.seq) throw new Error('The boot selection did not verify after writing.');
    return true;
  }

  // Leaves the chip without a reset pulse: the X3's stock firmware powers off
  // after a serial reset, so the user restarts it by hand instead.
  async disconnect() {
    if (!this.loader) return;
    try {
      await this.loader.transport.setDTR(false);
      await this.loader.after('no_reset_stub');
      await this.loader.transport.setRTS(false);
    } catch {}
    try {
      await this.loader.transport.disconnect();
    } catch {}
    this.loader = null;
  }
}
