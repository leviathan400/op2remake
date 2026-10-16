#include "buildingcatalog.h"

#include <QFile>
#include <QStringList>
#include <QTextStream>

bool BuildingCatalog::load(const QString& tsvPath) {
    QFile f(tsvPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream ts(&f);
    bool headerSkipped = false;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (!headerSkipped) { headerSkipped = true; continue; } // skip column header
        if (line.trimmed().isEmpty()) continue;

        // map_id name footprint initVA  e[0..7]  p[0..7]   (20 columns)
        const QStringList c = line.split('\t');
        if (c.size() < 20) continue;

        BuildingDef d;
        d.mapId = c[0].toInt(nullptr, 16);
        d.name = c[1];
        const QStringList fp = c[2].split('x');
        if (fp.size() == 2) {
            d.fpX = qMax(1, fp[0].toInt());
            d.fpY = qMax(1, fp[1].toInt());
        }
        for (int i = 0; i < 8; ++i) d.eden[i] = c[4 + i].toInt();
        for (int i = 0; i < 8; ++i) d.plym[i] = c[12 + i].toInt();
        buildings_.push_back(std::move(d));
    }
    return !buildings_.empty();
}
