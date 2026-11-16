#pragma once

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>

// What a model's animation clips are for: which one is the idle pose,
// which ones stand for which state, which ones play once. Read from the
// manifest that goes with the model (assets/away/actions.json for the
// built-in one) and narrowed down to the clips the loaded model really
// has, so that whoever uses it (MascotDirector) never has to ask whether
// a clip exists.
//
// The manifest:
//
//   {
//     "once": ["welcome", "done", ...],          clips that do not loop
//     "idle": { "base": "idle.default",          the resting clip
//               "interludes": ["idle.music", ...] },  small things in between
//     "work": ["working", "idle.reading", ...],  one of these per goal
//     "conditions": { "offline": "offline", "error": "error",
//                     "waiting_user": "waiting_user" },
//     "cues": { "welcome": ["welcome"], "done": ["done"],
//               "touch_left": [...], "touch_middle": [...],   alternatives,
//               "touch_right": [...] }                         used in turn
//   }
//
// It carries no clip lengths on purpose: those come from the loaded model
// (ClipPlayer::clipDurations), so that the two cannot disagree.
struct ActionCatalog
{
    // Empty when the model has no clips at all.
    QString idleBase;
    QStringList idleInterludes;
    QStringList workClips;
    QSet<QString> once;
    // By manifest key. A condition or cue whose clip the model lacks is
    // not in here.
    QHash<QString, QString> conditionClips;
    QHash<QString, QStringList> cueClips;

    // `available`: the loaded model's clips, in file order. Whatever the
    // manifest names that is not among them is dropped; a missing idle
    // base falls back on the model's first clip.
    static ActionCatalog fromJson(const QJsonObject &manifest, const QStringList &available);
};
