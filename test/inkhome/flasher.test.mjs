// Node tests for the web flasher's pure logic: node --test test/inkhome/flasher.test.mjs
// Optional real-device fixtures: INKHOME_FULL_DUMP=<16 MB backup.bin> INKHOME_FW=<firmware.bin>
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, existsSync } from 'node:fs';
import { buildOtadataSector, identifyImage, parseOtadata, parsePartitionTable, planInstall, validateImage } from '../../docs/flash/flasher.js';

const u32le = (v) => [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff];

function head(project, version = '1.0') {
  const b = new Uint8Array(0x100);
  b[0] = 0xe9;
  b.set(u32le(0xabcd5432), 0x20);
  b.set(new TextEncoder().encode(version), 0x30);
  b.set(new TextEncoder().encode(project), 0x50);
  return b;
}

const apps = [{ label: 'app0' }, { label: 'app1' }];
const dev = (k0, k1, activeApp) => ({ apps, activeApp, slots: [identifyImage(k0), identifyImage(k1)] });
const EMPTY = new Uint8Array(0x100).fill(0xff);

test('identifies stock, Inkhome, CrossPoint and empty slots', () => {
  assert.equal(identifyImage(head('arduino-lib-builder')).kind, 'stock');
  assert.equal(identifyImage(head('inkhome', '1.6.5')).kind, 'inkhome');
  assert.equal(identifyImage(head('crosspoint-reader')).kind, 'crosspoint');
  assert.equal(identifyImage(EMPTY).kind, 'empty');
});

test('first install never touches the running stock firmware', () => {
  assert.equal(planInstall(dev(EMPTY, head('arduino-lib-builder'), 1)).target, 0);
  assert.equal(planInstall(dev(head('arduino-lib-builder'), EMPTY, 0)).target, 1);
});

test('two stock copies: replaces the one not running', () => {
  const plan = planInstall(dev(head('arduino-lib-builder'), head('arduino-lib-builder'), 1));
  assert.equal(plan.target, 0);
  assert.ok(plan.warn);
});

test('update rewrites Inkhome in place even while Inkhome is running', () => {
  // The "not running" rule would pick app1 here and wipe stock.
  assert.equal(planInstall(dev(head('inkhome'), head('arduino-lib-builder'), 0)).target, 0);
  assert.equal(planInstall(dev(head('arduino-lib-builder'), head('inkhome'), 1)).target, 1);
});

test('CrossPoint beside stock is replaced, stock kept', () => {
  assert.equal(planInstall(dev(head('crosspoint-reader'), head('arduino-lib-builder'), 0)).target, 0);
});

test('otadata switch selects the requested slot with a valid CRC', () => {
  const raw = new Uint8Array(0x2000).fill(0xff);
  const fresh = parseOtadata(raw);
  assert.equal(fresh.activeApp, 0);
  for (const target of [0, 1]) {
    let state = raw.slice();
    for (let round = 0; round < 3; round++) {
      const ota = parseOtadata(state);
      const next = buildOtadataSector(state, ota, target);
      state.set(next.data, next.sector * 0x1000);
      const after = parseOtadata(state);
      assert.equal(after.activeApp, target);
      assert.equal(after.activeSeq, next.seq);
      state = state.slice();
    }
  }
});

test('rejects a corrupt image', async () => {
  await assert.rejects(validateImage(new Uint8Array([0xe9, 1, 0, 0])));
  await assert.rejects(validateImage(new TextEncoder().encode('<html>not found</html>')));
});

const dump = process.env.INKHOME_FULL_DUMP;
test('real device dump: table, CRC of stock otadata, both stock slots', { skip: !dump || !existsSync(dump) }, () => {
  const full = new Uint8Array(readFileSync(dump));
  const table = parsePartitionTable(full.subarray(0x8000, 0x9000));
  assert.equal(table.otadata.offset, 0xe000);
  assert.deepEqual(table.apps.map((a) => a.offset), [0x10000, 0x780000]);
  const ota = parseOtadata(full.subarray(0xe000, 0x10000));
  assert.ok(ota.activeSector >= 0, 'stock otadata entry must pass our CRC (matches esp_rom_crc32_le)');
  const slots = table.apps.map((a) => identifyImage(full.subarray(a.offset, a.offset + 0x100)));
  assert.deepEqual(slots.map((s) => s.kind), ['stock', 'stock']);
  console.log(`  active app${ota.activeApp}; plan:`, planInstall({ apps: table.apps, slots, activeApp: ota.activeApp }));
});

const fw = process.env.INKHOME_FW;
test('real Inkhome build validates and identifies', { skip: !fw || !existsSync(fw) }, async () => {
  const image = new Uint8Array(readFileSync(fw));
  await validateImage(image);
  assert.equal(identifyImage(image.subarray(0, 0x100)).kind, 'inkhome');
});
