#pragma once

// M5 - the BOOT button as the device's one physical control.
//
// The user asked whether switching modes needs anything special. It does not:
// on the device a short press of BOOT flips between SKY and NEO, and in the
// browser the arrow keys do the same. There is no other input on this board,
// and adding one is out of scope until the display arrives.
//
// GPIO0 is safe to read at runtime. Its strapping role only applies while the
// chip is coming out of reset - holding it down then enters download mode,
// which is exactly how firmware gets flashed, and is unaffected by polling it
// afterwards.
namespace modebutton {

void begin();

// Call from loop(). Debounces internally and fires on release, so holding the
// button down cannot rattle through modes.
void update();

}  // namespace modebutton
