#pragma once

// Bluetooth page turner: with the setting on, Bluetooth starts while a book
// is open and a paired BLE remote or keyboard turns pages (PageDown, Right,
// Down, Enter or Space forward; PageUp, Left, Up or Backspace back). Pair it
// from InkWriter's Keyboard row. Bluetooth stops when the book closes. It is
// skipped when free memory is low, and InkWriter's own use takes priority.

namespace inkhome::pageturner {

void tick(bool readerActive);
bool takeNext();
bool takePrev();
// A remote key arrived in the last minute (keeps the reader awake).
bool recentlyUsed();
// InkWriter owns Bluetooth while it is open.
void setAppOwnsBluetooth(bool owned);

}  // namespace inkhome::pageturner
