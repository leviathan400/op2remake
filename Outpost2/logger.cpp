#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

namespace {
QFile g_file;
}

void Logger::init(const QString& appDir) {
    QDir().mkpath(appDir + QStringLiteral("/logs"));
    const QString path = appDir + QStringLiteral("/logs/op2remake-") +
                         QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") +
                         QStringLiteral(".txt");
    g_file.setFileName(path);
    g_file.open(QIODevice::WriteOnly | QIODevice::Text);
    Logger::log(QStringLiteral("=== Outpost 2: Divided Destiny - debug log ==="));
}

void Logger::log(const QString& line) {
    if (!g_file.isOpen()) return;
    QTextStream ts(&g_file);
    ts << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << "  " << line << "\n";
    ts.flush();
}
