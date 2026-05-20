#include "archive/comic_archive.h"

#include <archive.h>
#include <archive_entry.h>

#include <QFileInfo>
#include <QHash>
#include <QLatin1String>
#include <QMutex>
#include <QMutexLocker>
#include <algorithm>

#include "core/natural_sort.h"

namespace ur {

namespace {

constexpr int kBlockSize = 64 * 1024;

bool hasImageExtension(const QString& name)
{
    static const char* const kExts[] = {
        ".jpg", ".jpeg", ".png", ".gif", ".webp",
        ".bmp", ".avif", ".jxl", ".tif", ".tiff",
    };
    const QString lower = name.toLower();
    for (const char* ext : kExts) {
        if (lower.endsWith(QLatin1String(ext)))
            return true;
    }
    return false;
}

bool isComicInfo(const QString& name)
{
    return QFileInfo(name).fileName().compare(
               QStringLiteral("ComicInfo.xml"), Qt::CaseInsensitive) == 0;
}

// Entry name as UTF-8, preferring libarchive's UTF-8 accessor.
QString entryName(struct archive_entry* entry)
{
    if (const char* utf8 = archive_entry_pathname_utf8(entry))
        return QString::fromUtf8(utf8);
    if (const char* raw = archive_entry_pathname(entry))
        return QString::fromUtf8(raw);
    return {};
}

// Opens a libarchive read handle for a comic archive. Caller frees it with
// archive_read_free. Format is detected from content, not the extension —
// any cbz/cbr/cb7/cbt that libarchive can identify will open.
struct archive* openHandle(const QString& path)
{
    struct archive* a = archive_read_new();
    if (!a)
        return nullptr;
    archive_read_support_format_zip(a);    // cbz
    archive_read_support_format_rar(a);    // cbr (RAR4)
    archive_read_support_format_rar5(a);   // cbr (RAR5)
    archive_read_support_format_7zip(a);   // cb7
    archive_read_support_format_tar(a);    // cbt
    archive_read_support_filter_all(a);
    if (archive_read_open_filename(
            a, path.toLocal8Bit().constData(), kBlockSize) != ARCHIVE_OK) {
        archive_read_free(a);
        return nullptr;
    }
    return a;
}

// Reads the data of the archive's current entry into a QByteArray.
QByteArray readCurrentData(struct archive* a, qint64 sizeHint)
{
    QByteArray out;
    if (sizeHint > 0 && sizeHint < (1LL << 31))
        out.reserve(static_cast<int>(sizeHint));

    const void* buf = nullptr;
    size_t len = 0;
    la_int64_t offset = 0;
    for (;;) {
        const int r = archive_read_data_block(a, &buf, &len, &offset);
        if (r == ARCHIVE_EOF)
            break;
        if (r < ARCHIVE_WARN)   // ARCHIVE_FAILED / ARCHIVE_FATAL
            break;
        out.append(static_cast<const char*>(buf), static_cast<qsizetype>(len));
    }
    return out;
}

} // namespace

struct ComicArchive::Impl {
    QString               path;
    QVector<ArchiveEntry> entries;     // natural-sorted images
    QByteArray            comicInfo;   // raw ComicInfo.xml, if present
    bool                  hasInfo = false;

    // Forward-advancing cursor over the archive in its stored (physical)
    // order. Holds the libarchive read handle between calls so an ascending
    // sequence of readEntry() calls walks through the archive once instead
    // of re-opening each time. All access serialised by `cursorMutex`.
    QMutex                cursorMutex;
    struct archive*       cursor        = nullptr;
    int                   cursorPhysical = -1;   // -1 = positioned before any kept entry
    QHash<QString, int>   physicalByName;        // entry name -> physical rank

    void closeCursor()
    {
        if (cursor) {
            archive_read_free(cursor);
            cursor = nullptr;
        }
        cursorPhysical = -1;
    }
};

ComicArchive::ComicArchive() : d(std::make_unique<Impl>()) {}
ComicArchive::~ComicArchive()
{
    d->closeCursor();
}

std::unique_ptr<ComicArchive> ComicArchive::open(const QString& path, QString* error)
{
    const auto fail = [error](const QString& msg) -> std::unique_ptr<ComicArchive> {
        if (error)
            *error = msg;
        return nullptr;
    };

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile())
        return fail(QStringLiteral("File not found: %1").arg(path));

    struct archive* a = openHandle(path);
    if (!a)
        return fail(QStringLiteral("Unsupported or unreadable archive: %1").arg(path));

    auto self = std::unique_ptr<ComicArchive>(new ComicArchive());
    self->d->path = path;

    QVector<ArchiveEntry> images;
    int physicalCounter = 0;          // rank among image entries in archive order
    struct archive_entry* entry = nullptr;
    for (;;) {
        const int r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r < ARCHIVE_WARN) {
            const QString msg = QString::fromUtf8(archive_error_string(a));
            archive_read_free(a);
            return fail(QStringLiteral("Corrupt archive: %1").arg(msg));
        }
        if (archive_entry_filetype(entry) != AE_IFREG)
            continue;   // skip directories and other special members

        const QString name = entryName(entry);
        if (isComicInfo(name)) {
            self->d->comicInfo = readCurrentData(a, archive_entry_size(entry));
            self->d->hasInfo = true;
        } else if (hasImageExtension(name)) {
            ArchiveEntry e;
            e.name = name;
            e.size = archive_entry_size_is_set(entry) ? archive_entry_size(entry) : 0;
            e.physical = physicalCounter++;
            images.append(e);
        }
        // Any other member type is ignored.
    }
    archive_read_free(a);

    if (images.isEmpty())
        return fail(QStringLiteral("No image pages found in archive: %1").arg(path));

    // Build the name -> physical-rank lookup before sorting, while we still
    // have one entry per name.
    self->d->physicalByName.reserve(images.size());
    for (const auto& e : images)
        self->d->physicalByName.insert(e.name, e.physical);

    std::sort(images.begin(), images.end(),
              [](const ArchiveEntry& x, const ArchiveEntry& y) {
                  return naturalLess(x.name, y.name);
              });
    for (int i = 0; i < images.size(); ++i)
        images[i].index = i;

    self->d->entries = std::move(images);
    return self;
}

const QString& ComicArchive::sourcePath() const
{
    return d->path;
}

const QVector<ArchiveEntry>& ComicArchive::entries() const
{
    return d->entries;
}

bool ComicArchive::hasComicInfo() const
{
    return d->hasInfo;
}

QByteArray ComicArchive::comicInfoXml() const
{
    return d->comicInfo;
}

QByteArray ComicArchive::readEntry(int index, QString* error)
{
    const auto fail = [error](const QString& msg) -> QByteArray {
        if (error)
            *error = msg;
        return {};
    };

    if (index < 0 || index >= d->entries.size())
        return fail(QStringLiteral("Page index out of range: %1").arg(index));

    const ArchiveEntry want = d->entries.at(index);

    QMutexLocker locker(&d->cursorMutex);

    // If the target is at or behind where the cursor is currently parked, we
    // can't seek backward in a libarchive stream — re-open. Otherwise advance
    // the existing cursor forward, which is what makes ascending prefetch
    // O(n) total instead of O(n^2).
    if (!d->cursor || d->cursorPhysical >= want.physical) {
        d->closeCursor();
        d->cursor = openHandle(d->path);
        if (!d->cursor)
            return fail(QStringLiteral("Could not reopen archive: %1").arg(d->path));
    }

    struct archive_entry* entry = nullptr;
    for (;;) {
        const int r = archive_read_next_header(d->cursor, &entry);
        if (r == ARCHIVE_EOF)
            break;
        if (r < ARCHIVE_WARN) {
            const QString msg = QString::fromUtf8(archive_error_string(d->cursor));
            d->closeCursor();
            return fail(QStringLiteral("Read failed: %1").arg(msg));
        }
        if (archive_entry_filetype(entry) != AE_IFREG)
            continue;

        const QString name = entryName(entry);
        const auto it = d->physicalByName.constFind(name);
        if (it == d->physicalByName.constEnd())
            continue;                 // not one of our image entries
        d->cursorPhysical = it.value();
        if (name == want.name) {
            QByteArray data = readCurrentData(d->cursor, archive_entry_size(entry));
            // Cursor stays parked on this entry; the next ascending request
            // will resume from archive_read_next_header.
            return data;
        }
        // Otherwise let archive_read_next_header skip past unread data.
    }

    // Walked off the end without finding it — the archive may be malformed.
    d->closeCursor();
    return fail(QStringLiteral("Entry not found in archive: %1").arg(want.name));
}

} // namespace ur
