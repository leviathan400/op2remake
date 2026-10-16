#include "unitcatalog.h"

#include <QFile>
#include <QStringList>
#include <QTextStream>

bool UnitCatalog::load(const QString& tsvPath) {
    QFile f(tsvPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream ts(&f);
    bool headerSkipped = false;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (!headerSkipped) { headerSkipped = true; continue; } // column header
        if (line.trimmed().isEmpty()) continue;

        // kind name faction dirs g0..g15  (20 columns)
        const QStringList c = line.split('\t');
        if (c.size() < 20) continue;

        UnitDef d;
        d.kind = c[0];
        d.name = c[1];
        d.faction = c[2];
        d.dirs = c[3].toInt();
        for (int i = 0; i < 16; ++i) d.groups[i] = c[4 + i].toInt();
        units_.push_back(std::move(d));
    }
    return !units_.empty();
}

const UnitDef* UnitCatalog::turretForChassis(const QString& chassisName) const {
    for (const UnitDef& u : units_) {
        if (u.kind == QStringLiteral("turret") &&
            u.name.startsWith(chassisName, Qt::CaseInsensitive)) {
            return &u;
        }
    }
    // Turrets are named "<Weapon>_<Chassis>" (e.g. Laser_Lynx); also match suffix.
    for (const UnitDef& u : units_) {
        if (u.kind == QStringLiteral("turret") &&
            u.name.contains(chassisName, Qt::CaseInsensitive)) {
            return &u;
        }
    }
    return nullptr;
}
