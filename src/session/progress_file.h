#pragma once

#include <QString>
#include <QHash>
#include <QDateTime>
#include <optional>
#include "core/types.h"

namespace ur {

// In-memory mirror of the progress-file JSON (schema 1). See DESIGN.md
// "Progress file" for the field reference.
struct ProgressData {
    int schema = 1;

    struct {
        QString id;        // partial-hash key
        QString path;      // last known path (display / relaunch only)
        QString title;
        int     pageCount = 0;
    } book;

    struct {
        int       page = 0;          // absolute page index
        bool      completed = false;
        int       openCount = 0;     // plain incrementer, +1 per launch
        QDateTime firstOpened;
        QDateTime lastRead;
    } progress;

    struct {
        ReadingMode      mode = ReadingMode::Paged;
        ReadingDirection direction = ReadingDirection::RightToLeft;
        FitMode          fit = FitMode::Smart;
        bool doublePage = false;
        bool coverAlone = true;
        int  pairOffset = 0;
        bool autoTrim = false;
        bool snap = false;
    } view;

    QHash<int, SpreadOverride> spreadOverrides;   // sparse: only overridden pages
};

// Loads / stores ProgressData as JSON. The reader is the sole writer; the
// library is strictly read-only.
//
// MODULE: session/progress.
class ProgressFile {
public:
    // Resolves to QStandardPaths::AppDataLocation + "/progress/<id>.json".
    static QString pathForId(const QString& bookId);

    static std::optional<ProgressData> load(const QString& bookId);

    // Atomic write: temp file + rename. Callers should debounce.
    static bool save(const ProgressData& data);
};

} // namespace ur
