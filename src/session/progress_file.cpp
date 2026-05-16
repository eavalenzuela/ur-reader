#include "session/progress_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace ur {

namespace {

QString progressDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
         + QStringLiteral("/progress");
}

// book ids look like "p1:<hex>"; ':' is replaced so the id is a safe filename.
QString sanitizeId(const QString& bookId)
{
    QString s = bookId;
    s.replace(QLatin1Char(':'), QLatin1Char('_'));
    s.replace(QLatin1Char('/'), QLatin1Char('_'));
    return s;
}

QString modeToString(ReadingMode m)
{
    return m == ReadingMode::Scroll ? QStringLiteral("scroll")
                                    : QStringLiteral("paged");
}
ReadingMode modeFromString(const QString& s)
{
    return s == QLatin1String("scroll") ? ReadingMode::Scroll
                                        : ReadingMode::Paged;
}

QString directionToString(ReadingDirection d)
{
    return d == ReadingDirection::LeftToRight ? QStringLiteral("ltr")
                                              : QStringLiteral("rtl");
}
ReadingDirection directionFromString(const QString& s)
{
    return s == QLatin1String("ltr") ? ReadingDirection::LeftToRight
                                     : ReadingDirection::RightToLeft;
}

QString fitToString(FitMode f)
{
    switch (f) {
    case FitMode::Width:    return QStringLiteral("width");
    case FitMode::Height:   return QStringLiteral("height");
    case FitMode::Whole:    return QStringLiteral("whole");
    case FitMode::Original: return QStringLiteral("original");
    case FitMode::Smart:    return QStringLiteral("smart");
    }
    return QStringLiteral("smart");
}
FitMode fitFromString(const QString& s)
{
    if (s == QLatin1String("width"))    return FitMode::Width;
    if (s == QLatin1String("height"))   return FitMode::Height;
    if (s == QLatin1String("whole"))    return FitMode::Whole;
    if (s == QLatin1String("original")) return FitMode::Original;
    return FitMode::Smart;
}

QString spreadToString(SpreadOverride s)
{
    switch (s) {
    case SpreadOverride::ForceSingle: return QStringLiteral("single");
    case SpreadOverride::ForcePair:   return QStringLiteral("pair");
    case SpreadOverride::Auto:        return QStringLiteral("auto");
    }
    return QStringLiteral("auto");
}
SpreadOverride spreadFromString(const QString& s)
{
    if (s == QLatin1String("single")) return SpreadOverride::ForceSingle;
    if (s == QLatin1String("pair"))   return SpreadOverride::ForcePair;
    return SpreadOverride::Auto;
}

} // namespace

QString ProgressFile::pathForId(const QString& bookId)
{
    return progressDir() + QLatin1Char('/') + sanitizeId(bookId)
         + QStringLiteral(".json");
}

std::optional<ProgressData> ProgressFile::load(const QString& bookId)
{
    QFile file(pathForId(bookId));
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.isEmpty())
        return std::nullopt;

    ProgressData d;
    d.schema = root.value("schema").toInt(1);

    const QJsonObject book = root.value("book").toObject();
    d.book.id = book.value("id").toString();
    d.book.path = book.value("path").toString();
    d.book.title = book.value("title").toString();
    d.book.pageCount = book.value("page_count").toInt();

    const QJsonObject pr = root.value("progress").toObject();
    d.progress.page = pr.value("page").toInt();
    d.progress.completed = pr.value("completed").toBool();
    d.progress.openCount = pr.value("open_count").toInt();
    d.progress.firstOpened =
        QDateTime::fromString(pr.value("first_opened").toString(), Qt::ISODate);
    d.progress.lastRead =
        QDateTime::fromString(pr.value("last_read").toString(), Qt::ISODate);

    const QJsonObject vw = root.value("view").toObject();
    d.view.mode = modeFromString(vw.value("mode").toString());
    d.view.direction = directionFromString(vw.value("direction").toString());
    d.view.fit = fitFromString(vw.value("fit").toString());
    d.view.doublePage = vw.value("double_page").toBool();
    d.view.coverAlone = vw.value("cover_alone").toBool(true);
    d.view.pairOffset = vw.value("pair_offset").toInt();
    d.view.autoTrim = vw.value("auto_trim").toBool();
    d.view.snap = vw.value("snap").toBool();

    const QJsonObject overrides = root.value("spread_overrides").toObject();
    for (auto it = overrides.begin(); it != overrides.end(); ++it)
        d.spreadOverrides.insert(it.key().toInt(),
                                 spreadFromString(it.value().toString()));

    return d;
}

bool ProgressFile::save(const ProgressData& data)
{
    const QString path = pathForId(data.book.id);
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject book;
    book["id"] = data.book.id;
    book["path"] = data.book.path;
    book["title"] = data.book.title;
    book["page_count"] = data.book.pageCount;

    QJsonObject pr;
    pr["page"] = data.progress.page;
    pr["completed"] = data.progress.completed;
    pr["open_count"] = data.progress.openCount;
    pr["first_opened"] = data.progress.firstOpened.toUTC().toString(Qt::ISODate);
    pr["last_read"] = data.progress.lastRead.toUTC().toString(Qt::ISODate);

    QJsonObject vw;
    vw["mode"] = modeToString(data.view.mode);
    vw["direction"] = directionToString(data.view.direction);
    vw["fit"] = fitToString(data.view.fit);
    vw["double_page"] = data.view.doublePage;
    vw["cover_alone"] = data.view.coverAlone;
    vw["pair_offset"] = data.view.pairOffset;
    vw["auto_trim"] = data.view.autoTrim;
    vw["snap"] = data.view.snap;

    QJsonObject overrides;
    for (auto it = data.spreadOverrides.begin();
         it != data.spreadOverrides.end(); ++it) {
        overrides[QString::number(it.key())] = spreadToString(it.value());
    }

    QJsonObject root;
    root["schema"] = data.schema;
    root["book"] = book;
    root["progress"] = pr;
    root["view"] = vw;
    root["spread_overrides"] = overrides;

    // QSaveFile gives the atomic temp-file + rename the design calls for.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

} // namespace ur
