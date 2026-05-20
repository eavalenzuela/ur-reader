// Smoke test for the view module. Runs headless via Qt's offscreen platform:
// builds the views, drives them through the decode pipeline, grabs their
// framebuffers, and checks that the expected pages render. Grabs are also
// saved to test_data/_view_*.png for eyeballing.
//
// Expects the bundled test_data/sample.cbz, whose pages are solid colours:
//   page 0 cover  blue   (40,60,120)
//   page 1 page1  grey   (200,200,200)
//   page 2 page2  red    (180,40,40)   -- 1600x1200, a wide double-spread
//   page 3 page10 green  (40,160,60)
//   page 4 page11 orange (235,140,30)
//
//   view_smoketest <archive-path>

#include <QApplication>
#include <QEventLoop>
#include <QImage>
#include <QScrollBar>
#include <QTextStream>
#include <QTimer>
#include <cstdlib>

#include "archive/comic_archive.h"
#include "decode/decode_service.h"
#include "model/book.h"
#include "view/paged_view.h"
#include "view/scroll_view.h"

namespace {

void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

bool nearColour(QRgb c, int r, int g, int b, int tol = 40)
{
    return std::abs(qRed(c) - r) < tol
        && std::abs(qGreen(c) - g) < tol
        && std::abs(qBlue(c) - b) < tol;
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        err << "usage: view_smoketest <archive-path>\n";
        return 2;
    }

    QString error;
    auto archive = ur::ComicArchive::open(args.at(1), &error);
    if (!archive) {
        err << "open failed: " << error << "\n";
        return 1;
    }
    ur::Book book(archive.get());
    ur::DecodeService decoder(&book, 256LL * 1024 * 1024);

    int passed = 0;
    int failed = 0;
    const auto check = [&](const char* what, bool ok) {
        out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
        ok ? ++passed : ++failed;
    };

    // --- PagedView -----------------------------------------------------------
    ur::PagedView paged;
    paged.resize(800, 1000);
    paged.setFitMode(ur::FitMode::Whole);
    paged.setDirection(ur::ReadingDirection::RightToLeft);
    paged.setBook(&book, &decoder);
    paged.show();
    pump(800);

    {
        const QImage img = paged.grab().toImage();
        img.save(QStringLiteral("test_data/_view_paged_p0.png"));
        const QRgb c = img.pixel(img.width() / 2, img.height() / 2);
        check("paged: page 0 shows cover (blue)", nearColour(c, 40, 60, 120));
        check("paged: currentPage() == 0", paged.currentPage() == 0);
    }

    paged.goToPage(3);
    pump(400);
    {
        const QImage img = paged.grab().toImage();
        const QRgb c = img.pixel(img.width() / 2, img.height() / 2);
        check("paged: goToPage(3) -> currentPage() == 3", paged.currentPage() == 3);
        check("paged: page 3 shows page10 (green)", nearColour(c, 40, 160, 60));
    }

    // Double-page mode. Page 2 is a wide double-spread, so detection keeps it
    // a unit of its own — goToPage(2) lands on page 2 alone, not a [1,2] pair.
    paged.setDoublePage(true);
    paged.goToPage(2);
    pump(400);
    {
        const QImage img = paged.grab().toImage();
        img.save(QStringLiteral("test_data/_view_paged_wide.png"));
        const QRgb c = img.pixel(img.width() / 2, img.height() / 2);
        check("paged: wide page 2 stands alone -> currentPage() == 2",
              paged.currentPage() == 2);
        check("paged: page 2 shows page2 (red)", nearColour(c, 180, 40, 40));
    }

    // Pages 3 and 4 are both portrait -> they pair into one spread. In RTL
    // the later page (4 = orange) sits on the left, page 3 (green) on the right.
    paged.goToPage(3);
    pump(400);
    {
        const QImage img = paged.grab().toImage();
        img.save(QStringLiteral("test_data/_view_paged_spread.png"));
        const int y = img.height() / 2;
        const QRgb left  = img.pixel(img.width() / 4, y);
        const QRgb right = img.pixel(img.width() * 3 / 4, y);
        check("paged: spread lead page is 3", paged.currentPage() == 3);
        check("paged: RTL spread puts page 4 (orange) on the left",
              nearColour(left, 235, 140, 30));
        check("paged: RTL spread puts page 3 (green) on the right",
              nearColour(right, 40, 160, 60));
    }

    // --- ScrollView ----------------------------------------------------------
    ur::ScrollView scroll;
    scroll.resize(800, 1000);
    scroll.setBook(&book, &decoder);
    scroll.show();
    pump(800);

    {
        const QImage img = scroll.grab().toImage();
        img.save(QStringLiteral("test_data/_view_scroll_top.png"));
        const QRgb c = img.pixel(img.width() / 2, img.height() / 2);
        check("scroll: top of book shows cover (blue)", nearColour(c, 40, 60, 120));
        check("scroll: currentPage() == 0 at top", scroll.currentPage() == 0);
    }

    scroll.goToPage(2);
    pump(400);
    {
        const QImage img = scroll.grab().toImage();
        img.save(QStringLiteral("test_data/_view_scroll_p2.png"));
        const QRgb c = img.pixel(img.width() / 2, img.height() / 2);
        check("scroll: goToPage(2) -> currentPage() == 2", scroll.currentPage() == 2);
        check("scroll: page 2 shows page2 (red)", nearColour(c, 180, 40, 40));
    }

    // --- snap-to-page (scroll mode) -----------------------------------------
    // Free-scroll baseline: nudge the viewport off a boundary and confirm it
    // stays put (no snap should fire when snap is off).
    scroll.goToPage(1);
    pump(200);
    {
        QScrollBar* bar = scroll.verticalScrollBar();
        const int baseline = bar->value();
        bar->setValue(baseline + 50);
        pump(250);
        check("scroll: free-scroll keeps off-boundary offset",
              std::abs(bar->value() - (baseline + 50)) < 5);
    }

    // Snap on: same nudge should settle back to the nearest page top.
    scroll.setSnap(true);
    scroll.goToPage(1);
    pump(250);                                  // let the settle timer fire
    {
        QScrollBar* bar = scroll.verticalScrollBar();
        const int boundary = bar->value();
        bar->setValue(boundary + 60);           // small nudge off the boundary
        pump(250);
        check("scroll: snap pulls small nudge back to page boundary",
              std::abs(bar->value() - boundary) < 5);
    }
    // A larger nudge past the midpoint should snap forward to the next page,
    // not backward — verifies we pick *nearest*, not always *previous*.
    {
        QScrollBar* bar = scroll.verticalScrollBar();
        const int before = bar->value();
        bar->setValue(before + 700);            // ~most of a page down (pages are 1200px tall)
        pump(250);
        check("scroll: snap goes forward when nudge crosses the midpoint",
              bar->value() > before + 300);
    }
    scroll.setSnap(false);

    out << "view smoke test: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
