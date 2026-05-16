// Smoke test for the decode pipeline. Builds a Book, runs every page through
// DecodeService (threaded decode + cache), then verifies a re-request is a
// cache hit and that the Book learned real page sizes from the decoder.
//
//   decode_smoketest <archive-path>

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

#include "archive/comic_archive.h"
#include "model/book.h"
#include "decode/decode_service.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        err << "usage: decode_smoketest <archive-path>\n";
        return 2;
    }

    QString error;
    auto archive = ur::ComicArchive::open(args.at(1), &error);
    if (!archive) {
        err << "open failed: " << error << "\n";
        return 1;
    }

    ur::Book book(archive.get());
    const int pageCount = book.pageCount();
    out << "pages: " << pageCount << "\n";

    ur::DecodeService decoder(&book, 256LL * 1024 * 1024);

    int received = 0;
    int failed = 0;
    QObject::connect(&decoder, &ur::DecodeService::pageReady, &app,
        [&](int idx, const QImage& img) {
            out << "  ready  page " << idx << "  "
                << img.width() << "x" << img.height() << "\n";
            if (++received + failed == pageCount)
                app.quit();
        });
    QObject::connect(&decoder, &ur::DecodeService::pageFailed, &app,
        [&](int idx, const QString& e) {
            out << "  FAILED page " << idx << ": " << e << "\n";
            if (received + ++failed == pageCount)
                app.quit();
        });

    // Decode the whole book via a forward prefetch window from page 0.
    decoder.setFocus(0, pageCount, 0);

    QTimer::singleShot(10000, &app, [&]() {
        err << "timeout waiting for decodes\n";
        app.quit();
    });
    app.exec();
    out << "decoded " << received << " / " << pageCount
        << " (" << failed << " failed)\n";

    // Re-request page 0: pageReady fires synchronously on a cache hit.
    bool cacheHit = false;
    QObject::connect(&decoder, &ur::DecodeService::pageReady, &app,
        [&](int idx, const QImage&) { if (idx == 0) cacheHit = true; });
    decoder.request(0);
    out << "re-request page 0 is cache hit: " << (cacheHit ? "yes" : "no") << "\n";

    if (pageCount > 0) {
        const ur::Page& p0 = book.page(0);
        out << "book learned page[0] size: sizeKnown=" << p0.sizeKnown
            << " " << p0.pixelSize.width() << "x" << p0.pixelSize.height() << "\n";
    }

    return failed == 0 ? 0 : 1;
}
