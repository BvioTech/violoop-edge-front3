#include "ActionCatalog.h"

#include <QJsonArray>
#include <QJsonValue>

namespace {

// The strings of a JSON array (or a lone string) that name available
// clips, in the order given.
QStringList clips(const QJsonValue &value, const QStringList &available)
{
    QStringList result;
    const QJsonArray array = value.isArray() ? value.toArray() : QJsonArray { value };
    for (const QJsonValue &entry : array) {
        const QString name = entry.toString();
        if (available.contains(name) && !result.contains(name))
            result.append(name);
    }
    return result;
}

} // namespace

ActionCatalog ActionCatalog::fromJson(const QJsonObject &manifest, const QStringList &available)
{
    ActionCatalog catalog;
    if (available.isEmpty())
        return catalog;

    const QJsonObject idle = manifest.value(QLatin1String("idle")).toObject();
    const QString base = idle.value(QLatin1String("base")).toString();
    catalog.idleBase = available.contains(base) ? base : available.first();
    catalog.idleInterludes = clips(idle.value(QLatin1String("interludes")), available);
    catalog.idleInterludes.removeAll(catalog.idleBase);

    catalog.workClips = clips(manifest.value(QLatin1String("work")), available);
    const QStringList once = clips(manifest.value(QLatin1String("once")), available);
    catalog.once = QSet<QString>(once.begin(), once.end());

    const QJsonObject conditions = manifest.value(QLatin1String("conditions")).toObject();
    for (auto it = conditions.begin(); it != conditions.end(); ++it) {
        const QStringList found = clips(it.value(), available);
        if (!found.isEmpty())
            catalog.conditionClips.insert(it.key(), found.first());
    }
    const QJsonObject cues = manifest.value(QLatin1String("cues")).toObject();
    for (auto it = cues.begin(); it != cues.end(); ++it) {
        const QStringList found = clips(it.value(), available);
        if (!found.isEmpty())
            catalog.cueClips.insert(it.key(), found);
    }
    return catalog;
}
