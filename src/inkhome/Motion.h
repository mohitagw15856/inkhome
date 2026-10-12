#pragma once

// Motion gestures from the X3's IMU (accelerometer and gyro), both off by
// default and marked experimental until tuned on hardware:
//   Shake to bookmark: three firm jolts within a second while reading.
//   Flip to sleep: lay the reader face down and still for two seconds.
// "Face down" is learnt rather than assumed: while the owner presses buttons
// the reader is facing them, so the sign of gravity on the screen axis at
// those moments is "face up". Nothing happens until that has been learnt.
// No IMU (X4): everything here is a no-op.

namespace inkhome::motion {

struct Reading {
  float ax = 0, ay = 0, az = 0;  // g
  float gx = 0, gy = 0, gz = 0;  // degrees per second
  bool valid = false;
};

void tick(bool readerActive, bool userActivity);
// Latest sample, for the sensor test screen.
Reading latest();
// -1 face up is negative Z, +1 positive Z, 0 not learnt yet.
int learntFaceUpSign();
// Keeps the IMU awake while the sensor test screen is open.
void setTestScreenOpen(bool open);

}  // namespace inkhome::motion
