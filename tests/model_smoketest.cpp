// Smoke test for the model layer. Builds a Book from an archive, prints the
// page list, and exercises spread detection, per-page overrides, and the
// reading-mode heuristic by feeding in synthetic page sizes.
//
//   model_smoketest <archive-path>

#include <QCoreApplication>
#include <QTextStream>

#include "archive/comic_archive.h"
#include "core/stratified_sample.h"
#include "model/book.h"
#include "model/comic_info.h"

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

    // --- ComicInfo.xml parsing ----------------------------------------------
    int failed = 0;
    const auto check = [&](const char* what, bool ok) {
        out << "  " << (ok ? "PASS" : "FAIL") << "  " << what << "\n";
        if (!ok) ++failed;
    };

    // 1. The bundled sample carries "<Manga>YesAndRightToLeft</Manga>".
    if (archive->hasComicInfo()) {
        const auto hints = ur::ComicInfoHints::parse(archive->comicInfoXml());
        check("comicinfo: bundled sample parses as YesAndRTL",
              hints.manga == ur::MangaHint::YesAndRTL);
        const auto dir = hints.readingDirection();
        check("comicinfo: YesAndRTL implies RightToLeft direction",
              dir && *dir == ur::ReadingDirection::RightToLeft);
    }

    // 2. Synthetic XML with a couple of <Page> entries (DoublePage + Type).
    const QByteArray xml = R"(<?xml version="1.0"?>
<ComicInfo>
  <Manga>No</Manga>
  <Pages>
    <Page Image="0" Type="FrontCover"/>
    <Page Image="3" DoublePage="true" Type="Story"/>
    <Page Image="7" DoublePage="false"/>
  </Pages>
</ComicInfo>)";
    const auto hints = ur::ComicInfoHints::parse(xml);
    check("comicinfo: Manga=No parsed", hints.manga == ur::MangaHint::No);
    check("comicinfo: No implies LeftToRight",
          hints.readingDirection() == ur::ReadingDirection::LeftToRight);
    check("comicinfo: page 3 marked DoublePage",
          hints.pages.value(3).doublePage);
    check("comicinfo: page 7 explicitly not DoublePage",
          hints.pages.contains(7) && !hints.pages.value(7).doublePage);
    check("comicinfo: page 0 type captured as FrontCover",
          hints.pages.value(0).type == QStringLiteral("FrontCover"));

    // 3. Empty / garbage input must not crash and must report empty.
    check("comicinfo: empty input -> empty hints",
          ur::ComicInfoHints::parse(QByteArray()).empty());
    check("comicinfo: malformed XML -> empty hints",
          ur::ComicInfoHints::parse(QByteArrayLiteral("<<not xml>>")).empty());

    // --- stratified sampler -------------------------------------------------
    // Edge cases first.
    check("stratified: total=0 -> empty",
          ur::stratifiedSample(0, 5).isEmpty());
    check("stratified: sampleCount=0 -> empty",
          ur::stratifiedSample(10, 0).isEmpty());
    check("stratified: total=1 -> {0}",
          ur::stratifiedSample(1, 4) == QVector<int>{0});
    // Typical: sampleCount > total -> dedup collapses to {0..total-1}.
    check("stratified: oversample collapses to full range",
          ur::stratifiedSample(5, 20) == (QVector<int>{0, 1, 2, 3, 4}));
    // The endpoints must always be included.
    {
        const auto s = ur::stratifiedSample(100, 5);
        check("stratified: 5 samples across 100 -> first==0 and last==99",
              !s.isEmpty() && s.first() == 0 && s.last() == 99);
        check("stratified: 5 samples across 100 -> evenly spread",
              s == (QVector<int>{0, 24, 49, 74, 99}));
    }

    out << "model smoke test extras: "
        << (failed == 0 ? "all passed" : "FAILURES") << "\n";
    return failed == 0 ? 0 : 1;
}
