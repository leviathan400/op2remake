#pragma once
#include <QString>

// =============================================================================
// Logger  —  minimal timestamped file log
// =============================================================================
//
// Writes to <appDir>/logs/op2remake-YYYYMMDD-HHMMSS.txt. Call Logger::init()
// once at startup, then Logger::log() from anywhere. Cheap and flushes each
// line so the log survives a crash.

namespace Logger {
void init(const QString& appDir);
void log(const QString& line);
} // namespace Logger
