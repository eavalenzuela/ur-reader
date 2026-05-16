#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>
#include <memory>

namespace ur {

// One file inside the archive.
struct ArchiveEntry {
    int     index = -1;   // position in the filtered, natural-sorted list
    QString name;         // path within the archive
    qint64  size = 0;     // uncompressed size in bytes
};

// Read-only access to a comic archive. v1 accepts cbz (Zip) and cbr (RAR4 /
// RAR5); cb7 (7z) and cbt (Tar) are deferred post-v1 — libarchive handles
// them through the same API, so adding them is an extension allow-list change.
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

    // Extracts the raw decompressed bytes of one entry. Concurrent-safe: each
    // call uses an independent archive handle and reads only immutable state,
    // so callers may decode pages from multiple threads without locking.
    QByteArray readEntry(int index, QString* error);

    ~ComicArchive();

private:
    ComicArchive();
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ur
