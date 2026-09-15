#pragma once

// M6 - the PAJ7620U2 replaces the BOOT button as the instrument's only input.
//
// Mapping:
//   swipe left / right   previous / next mode (SKY <-> NEO)
//   swipe up / down      backlight brighter / dimmer
//   wave                 wake the screen from its dimmed idle
//
// Rotation and forward/backward are deliberately left unbound. They are the
// least reliable gestures on this sensor and there is nothing yet that needs
// them; binding them "because they exist" only creates accidental triggers.
namespace gestureinput {

// Returns false when the sensor does not answer on I2C, which is not fatal -
// the instrument stays fully usable from the web UI, and this is exactly what
// a miswired or absent sensor looks like.
bool begin();

bool present();

// Call from loop(). Cheap: reads one register and returns unless a gesture
// actually fired.
void loop();

// Swaps the sense of left/right and up/down. Which way round the sensor reads
// depends on how it ends up physically mounted, and that is not knowable until
// it is in an enclosure.
void setInverted(bool inverted);

}  // namespace gestureinput
