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
    return 0;
}
