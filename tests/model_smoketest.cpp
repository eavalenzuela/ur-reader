// Smoke test for the model layer. Builds a Book from an archive, prints the
// page list, and exercises spread detection, per-page overrides, and the
// reading-mode heuristic by feeding in synthetic page sizes.
//
//   model_smoketest <archive-path>

#include <QCoreApplication>
#include <QTextStream>

#include "archive/comic_archive.h"
#include "model/book.h"

namespace {

QString modeName(std::optional<ur::ReadingMode> m)
{
    if (!m)
        return QStringLiteral("undetermined");
    return *m == ur::ReadingMode::Paged ? QStringLiteral("Paged")
                                        : QStringLiteral("Scroll");
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        err << "usage: model_smoketest <archive-path>\n";
        return 2;
    }

    QString error;
    auto archive = ur::ComicArchive::open(args.at(1), &error);
    if (!archive) {
        err << "open failed: " << error << "\n";
        return 1;
    }

    ur::Book book(archive.get());
    out << "title    : " << book.title() << "\n";
    out << "pageCount: " << book.pageCount() << "\n";
    for (int i = 0; i < book.pageCount(); ++i) {
        const ur::Page& p = book.page(i);
        out << "  page[" << p.index << "] entry=" << p.entryIndex
            << " name=" << p.name << "\n";
    }

    out << "detectMode (no sizes)   : " << modeName(book.detectMode()) << "\n";

    // Portrait pages, with page index 2 made a wide double-spread.
    for (int i = 0; i < book.pageCount(); ++i)
        book.setPageSize(i, QSize(800, 1200));
    if (book.pageCount() > 2)
        book.setPageSize(2, QSize(1600, 1200));
    out << "detectMode (portrait)   : " << modeName(book.detectMode()) << "\n";

    if (book.pageCount() > 2) {
        out << "  page[2] isWideAuto             = "
            << book.page(2).isWideAuto() << "\n";
        out << "  page[2] treatAsSpread (Auto)   = "
            << book.page(2).treatAsSpread() << "\n";
        out << "  page[0] treatAsSpread (Auto)   = "
            << book.page(0).treatAsSpread() << "\n";

        book.page(2).override = ur::SpreadOverride::ForceSingle;
        out << "  page[2] treatAsSpread (+Single)= "
            << book.page(2).treatAsSpread() << "\n";
        book.page(0).override = ur::SpreadOverride::ForcePair;
        out << "  page[0] treatAsSpread (+Pair)  = "
            << book.page(0).treatAsSpread() << "\n";
    }

    // Make every page a tall strip -> should detect webtoon scroll.
    for (int i = 0; i < book.pageCount(); ++i)
        book.setPageSize(i, QSize(800, 3000));
    out << "detectMode (tall strips): " << modeName(book.detectMode()) << "\n";

    return 0;
}
