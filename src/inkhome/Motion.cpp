#include "Motion.h"

#include <HalTiltSensor.h>
#include <Logging.h>

#include <algorithm>
#include <cmath>

#include "CrossPointSettings.h"
#include "HomeButtonSettings.h"
#include "MappedInputManager.h"

extern MappedInputManager mappedInputManager;  // main.cpp
void enterDeepSleep(bool fromTimeout);         // main.cpp: the same path as auto-sleep

namespace inkhome::motion {

namespace {

constexpr unsigned long POLL_MS = 50;
// Shake: |a| above this many g, this many times, within the window.
constexpr float SHAKE_G = 1.8f;
constexpr int SHAKE_PEAKS = 3;
constexpr unsigned long SHAKE_WINDOW_MS = 1000;
constexpr unsigned long SHAKE_COOLDOWN_MS = 3000;
// Flip: gravity mostly on the screen axis, nearly still, for this long.
constexpr float FLAT_G = 0.85f;
constexpr float STILL_DPS = 20.0f;
constexpr unsigned long FLIP_HOLD_MS = 2000;
// Face-up learning: sign of Z while the owner presses buttons.
constexpr int LEARN_SAMPLES = 5;

Reading last;
unsigned long lastPollMs = 0;
bool testOpen = false;

unsigned long peakTimes[SHAKE_PEAKS] = {};
int peakCount = 0;
bool aboveShake = false;
unsigned long lastShakeMs = 0;

int learnVotes = 0;  // positive: +Z faces the owner
int learnSamples = 0;
unsigned long faceDownSinceMs = 0;

bool anyGestureOn() { return SETTINGS.motionShakeBookmark || SETTINGS.motionFlipSleep; }

// The simulator's IMU stand-in has no raw readings: report a reader lying still, face up.
void keepImuAwake(const bool awake) {
#ifdef SIMULATOR
  (void)awake;
#else
  halTiltSensor.setKeepAwake(awake);
#endif
}

bool readImu(Reading& r) {
#ifdef SIMULATOR
  r.ax = 0.02f;
  r.ay = -0.03f;
  r.az = 0.99f;
  r.gx = r.gy = r.gz = 0.5f;
  return true;
#else
  return halTiltSensor.readMotion(r.ax, r.ay, r.az, r.gx, r.gy, r.gz);
#endif
}

void detectShake(const unsigned long now) {
  const float magnitude = std::sqrt(last.ax * last.ax + last.ay * last.ay + last.az * last.az);
  const bool above = magnitude > SHAKE_G;
  if (above && !aboveShake) {
    peakTimes[peakCount % SHAKE_PEAKS] = now;
    ++peakCount;
    const unsigned long oldest = peakTimes[peakCount % SHAKE_PEAKS];
    if (peakCount >= SHAKE_PEAKS && now - oldest <= SHAKE_WINDOW_MS && now - lastShakeMs > SHAKE_COOLDOWN_MS) {
      lastShakeMs = now;
      peakCount = 0;
      const bool lookUp = SETTINGS.motionShakeBookmark == 2;
      LOG_INF("MOTION", "Shake: %s", lookUp ? "look up" : "bookmark");
      mappedInputManager.injectHomeButtonAction(lookUp ? HomeButtonAction::Dictionary : HomeButtonAction::Bookmark);
    }
  }
  aboveShake = above;
}

void detectFlip(const unsigned long now) {
  const int faceUp = learntFaceUpSign();
  if (faceUp == 0) return;
  const bool still = std::fabs(last.gx) < STILL_DPS && std::fabs(last.gy) < STILL_DPS && std::fabs(last.gz) < STILL_DPS;
  const bool faceDown = still && (faceUp > 0 ? last.az < -FLAT_G : last.az > FLAT_G);
  if (!faceDown) {
    faceDownSinceMs = 0;
    return;
  }
  if (faceDownSinceMs == 0) faceDownSinceMs = now;
  if (now - faceDownSinceMs >= FLIP_HOLD_MS) {
    faceDownSinceMs = 0;
    LOG_INF("MOTION", "Face down: sleep");
    enterDeepSleep(false);
  }
}

}  // namespace

Reading latest() { return last; }

int learntFaceUpSign() {
  if (learnSamples < LEARN_SAMPLES) return 0;
  return learnVotes > 0 ? 1 : learnVotes < 0 ? -1 : 0;
}

void setTestScreenOpen(const bool open) { testOpen = open; }

void tick(const bool readerActive, const bool userActivity) {
  if (!halTiltSensor.isAvailable()) return;
  const bool wanted = anyGestureOn() || testOpen;
  keepImuAwake(wanted);
  if (!wanted) return;

  const unsigned long now = millis();
  if (now - lastPollMs < POLL_MS) return;
  lastPollMs = now;
  last.valid = readImu(last);
  if (!last.valid) return;

  // A button press means the screen is facing the owner: learn which Z sign that is.
  if (userActivity && std::fabs(last.az) > 0.3f) {
    learnVotes = std::max(-50, std::min(50, learnVotes + (last.az > 0 ? 1 : -1)));
    if (learnSamples < 1000) ++learnSamples;
  }

  if (SETTINGS.motionShakeBookmark && readerActive) detectShake(now);
  if (SETTINGS.motionFlipSleep) detectFlip(now);
}

}  // namespace inkhome::motion
