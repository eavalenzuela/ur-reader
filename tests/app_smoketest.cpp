// Smoke test for the app module. Runs headless via Qt's offscreen platform:
// constructs the MainWindow, grabs its framebuffer, and checks the page
// renders and that the mode toggle works. Grabs are saved to test_data/.
//
//   app_smoketest <archive-path>

#include <QApplication>
#include <QEventLoop>
#include <QImage>
#include <QTextStream>
#include <QTimer>
#include <cstdlib>

#include "app/hover_scrubber.h"
#include "app/main_window.h"
#include "archive/comic_archive.h"
#include "model/book.h"
#include "session/app_config.h"
#include "session/progress_file.h"

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
        err << "usage: app_smoketest <archive-path>\n";
        return 2;
    }

    QString error;
    auto archive = ur::ComicArchive::open(args.at(1), &error);
    if (!archive) {
        err << "open failed: " << error << "\n";
        return 1;
    }
    ur::Book* book = new ur::Book(archive.get());

    ur::ProgressData progress;
    progress.book.id = QStringLiteral("p1:apptest");
    progress.book.path = args.at(1);
    progress.book.pageCount = book->pageCount();
    progress.view.mode = ur::ReadingMode::Paged;
    progress.view.direction = ur::ReadingDirection::RightToLeft;
    progress.view.fit = ur::FitMode::Whole;
    progress.progress.page = 0;

    int passed = 0;
    int failed = 0;
    const auto check = [&](const char* what, bool ok) {
        out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
        ok ? ++passed : ++failed;
    };

    ur::MainWindow window(std::move(archive), book, progress, ur::AppConfig{});
    window.resize(900, 1100);
    window.show();
    pump(1000);

    {
        const QImage img = window.grab().toImage();
        img.save(QStringLiteral("test_data/_app_paged.png"));
        check("window grab is non-null", !img.isNull());
        const QRgb c = img.pixel(img.width() / 2, img.height() * 2 / 3);
        check("paged view shows page 0 cover (blue)", nearColour(c, 40, 60, 120));
    }

    window.setMode(ur::ReadingMode::Scroll);
    pump(600);
    {
        const QImage img = window.grab().toImage();
        img.save(QStringLiteral("test_data/_app_scroll.png"));
        const QRgb c = img.pixel(img.width() / 2, img.height() * 2 / 3);
        check("setMode(Scroll) keeps page 0 cover (blue)",
              nearColour(c, 40, 60, 120));
    }

    // --- HoverScrubber pageAtX ----------------------------------------------
    // Set up a 5-page slider 400 px wide; verify the endpoints and a couple
    // of midpoints, and confirm RTL inversion flips the mapping.
    {
        ur::HoverScrubber s;
        s.resize(400, 24);
        s.setRange(0, 4);

        s.setInvertedAppearance(false);
        check("scrubber: LTR x=0 -> page 0", s.pageAtX(0) == 0);
        check("scrubber: LTR x=400 -> page 4", s.pageAtX(400) == 4);
        const int midLtr = s.pageAtX(200);
        check("scrubber: LTR mid is page 2", midLtr == 2);

        s.setInvertedAppearance(true);
        check("scrubber: RTL x=0 -> page 4", s.pageAtX(0) == 4);
        check("scrubber: RTL x=400 -> page 0", s.pageAtX(400) == 0);
    }

    out << "app smoke test: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
