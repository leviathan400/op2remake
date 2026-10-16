#include "gameclock.h"

GameClock::GameClock(QObject* parent) : QObject(parent) {
    // Fixed-cadence frame: generate a whole number of ticks from the accumulator
    // (so the tick rate scales with game speed) and report how many advanced this
    // frame so the renderer can advance animation by exactly that much.
    //
    // Why the fractional accumulator: at most speeds a frame is worth a NON-WHOLE
    // number of ticks (e.g. speed 5 → ~0.458 ticks/frame), but the sim clock only
    // advances in whole ticks. So each frame adds its fractional worth, emits the
    // integer part (floor), and CARRIES the remainder — e.g. 0.458, 0.916, 1.374→1
    // carry 0.374, … — yielding a deterministic 0/1 (or n/n+1) pattern whose
    // long-run average is exactly ticksPerFrame. No tick is ever lost or invented,
    // which is what keeps animation speed (and later the sim) true at every speed.
    // gameSpeed_ is read at timeout time, so a mid-frame speed change simply
    // applies from the next frame — the carry is preserved, no drift.
    connect(&timer_, &QTimer::timeout, this, [this] {
        tickAccumulator_ += ticksPerFrame(gameSpeed_);
        int n = static_cast<int>(tickAccumulator_);
        tickAccumulator_ -= n;
        for (int i = 0; i < n; ++i) clock_.advance();
        ticksThisFrame_ = n;
        emit ticked();
    });
}

QString GameClock::elapsed() const {
    const qint64 ms = wall_.isValid() ? wall_.elapsed() : 0;
    const qint64 totalSec = ms / 1000;
    const qint64 h = totalSec / 3600;
    const qint64 m = (totalSec % 3600) / 60;
    const qint64 s = totalSec % 60;
    if (h > 0) {
        return QString("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    }
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

void GameClock::setGameSpeed(int speed) {
    if (speed < kMinSpeed) speed = kMinSpeed;
    if (speed > kMaxSpeed) speed = kMaxSpeed;
    if (speed == gameSpeed_) return;
    gameSpeed_ = speed;
    // The timer interval is fixed (kFrameIntervalMs); speed changes only the number of
    // ticks generated per frame via the accumulator — no interval change needed.
    emit speedChanged();
}

void GameClock::start() {
    if (!wall_.isValid()) {
        wall_.start();
    }
    timer_.start(kFrameIntervalMs);
    emit ticked();
}

void GameClock::stop() {
    timer_.stop();
}

void GameClock::reset() {
    clock_.reset();
    wall_.restart();
    emit ticked();
}
