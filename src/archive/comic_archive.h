#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>
#include <memory>

namespace ur {

// One file inside the archive.
struct ArchiveEntry {
    int     index = -1;      // position in the filtered, natural-sorted list
    int     physical = -1;   // rank among kept entries in archive's stored order
    QString name;            // path within the archive
    qint64  size = 0;        // uncompressed size in bytes
};

// Read-only access to a comic archive. Accepts cbz (Zip), cbr (RAR4/RAR5),
// cb7 (7z), and cbt (Tar). Format is detected by libarchive from file
// content, not the filename, so a mis-extensioned archive still opens.
// Wraps libarchive. Pure C++ — no Qt GUI, not a QObject.
//
// NOTE: cbr (RAR) is a sequential-access format; extracting entry N may
// require re-scanning from the start. Callers should request entries in
// roughly ascending order — the decode prefetcher already does.
//
// MODULE: archive layer (independently testable, no GUI).
class ComicArchive {
public:
    // Opens `path` and builds the image-entry list, natural-sorted. Non-image
    // members are excluded from entries(). A ComicInfo.xml member, if present,
    // is detected and its raw bytes stashed (see comicInfoXml) — v1 does not
    // parse or act on it. Returns nullptr on failure; `error` gets a reason.
    static std::unique_ptr<ComicArchive> open(const QString& path, QString* error);

    const QString&               sourcePath() const;
    const QVector<ArchiveEntry>& entries() const;   // natural-sorted images

    // ComicInfo.xml passthrough: detected and stashed but unused in v1, so a
    // future library/metadata consumer can read it without reader changes.
    bool       hasComicInfo() const;
    QByteArray comicInfoXml() const;   // raw, unparsed; empty if absent

    // Extracts the raw decompressed bytes of one entry. Internally keeps a
    // single forward-advancing libarchive cursor protected by a mutex: an
    // ascending sequence of reads only opens the archive once and walks
    // through it in order, which is the fast path for RAR (a sequential-
    // access format). A request behind the cursor re-opens the archive and
    // walks from the start. Concurrent calls are serialised; decode workers
    // can still run the image-decode step in parallel.
    QByteArray readEntry(int index, QString* error);

    ~ComicArchive();

private:
    ComicArchive();
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ur
