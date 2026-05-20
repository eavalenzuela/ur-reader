// Smoke test for the archive layer. Opens a cbz/cbr, prints the page list,
// reports ComicInfo.xml detection, and reads the first and last pages.
//
//   archive_smoketest <archive-path>

#include <QCoreApplication>
#include <QTextStream>

#include "archive/comic_archive.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        err << "usage: archive_smoketest <archive-path>\n";
        return 2;
    }

    QString error;
    auto archive = ur::ComicArchive::open(args.at(1), &error);
    if (!archive) {
        err << "open failed: " << error << "\n";
        return 1;
    }

    const auto& entries = archive->entries();
    out << "source       : " << archive->sourcePath() << "\n";
    out << "pages        : " << entries.size() << "\n";
    out << "ComicInfo.xml: "
        << (archive->hasComicInfo()
                ? QStringLiteral("yes (%1 bytes)").arg(archive->comicInfoXml().size())
                : QStringLiteral("no"))
        << "\n";

    out << "entry order (natural-sorted):\n";
    for (const auto& e : entries)
        out << "  [" << e.index << "] " << e.name << "  size=" << e.size << "\n";

    if (!entries.isEmpty()) {
        for (int idx : {0, static_cast<int>(entries.size()) - 1}) {
            QString readError;
            const QByteArray data = archive->readEntry(idx, &readError);
            if (data.isEmpty())
                out << "readEntry(" << idx << ") FAILED: " << readError << "\n";
            else
                out << "readEntry(" << idx << ") -> " << data.size() << " bytes\n";
        }
    }

    // --- cursor correctness --------------------------------------------------
    // Sequential reads, then a backward jump (forces re-open), then random
    // access — every read must return the same payload for the same entry.
    int failed = 0;
    const auto check = [&](const char* what, bool ok) {
        out << "  " << (ok ? "PASS" : "FAIL") << "  " << what << "\n";
        if (!ok) ++failed;
    };

    QVector<QByteArray> forward(entries.size());
    for (int i = 0; i < entries.size(); ++i)
        forward[i] = archive->readEntry(i, nullptr);

    // Backward sweep (each step forces a re-open). Bytes must match.
    bool backwardMatches = true;
    for (int i = entries.size() - 1; i >= 0; --i) {
        if (archive->readEntry(i, nullptr) != forward[i]) {
            backwardMatches = false;
            break;
        }
    }
    check("cursor: backward sweep returns identical bytes", backwardMatches);

    // Random-access pattern interleaving forward jumps and backward jumps.
    bool randomMatches = true;
    const int pattern[] = {2, 0, 4, 1, 3, 0, 4, 2};
    for (int idx : pattern) {
        if (idx >= entries.size()) continue;
        if (archive->readEntry(idx, nullptr) != forward[idx]) {
            randomMatches = false;
            break;
        }
    }
    check("cursor: random-access pattern returns identical bytes",
          randomMatches);

    // Resuming ascending after a backward jump must still work (the cursor
    // re-opens cleanly).
    check("cursor: ascending resume after rewind",
          archive->readEntry(0, nullptr) == forward[0]
       && archive->readEntry(static_cast<int>(entries.size()) - 1, nullptr)
              == forward.last());

    out << "archive smoke test: "
        << (failed == 0 ? "all cursor checks passed" : "FAILURES") << "\n";
    return failed == 0 ? 0 : 1;
}
