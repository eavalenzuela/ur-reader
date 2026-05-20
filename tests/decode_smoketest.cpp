// Smoke test for the decode pipeline. Builds a Book, runs every page through
// DecodeService (threaded decode + cache), then verifies a re-request is a
// cache hit and that the Book learned real page sizes from the decoder.
//
//   decode_smoketest <archive-path>

#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QTextStream>
#include <QTimer>

#include "archive/comic_archive.h"
#include "model/book.h"
#include "decode/auto_trim.h"
#include "decode/decode_service.h"
#include "decode/thumbnail_service.h"

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

    // --- auto_trim unit checks ----------------------------------------------
    // 1. Page with a clear ~10% white border on every side should crop down
    //    to just the inner content rectangle.
    {
        QImage bordered(400, 600, QImage::Format_ARGB32);
        bordered.fill(QColor(255, 255, 255));
        QPainter p(&bordered);
        p.fillRect(40, 60, 320, 480, QColor(20, 20, 20));   // 10% border, dark inside
        p.end();
        const QImage trimmed = ur::autoTrimBorders(bordered);
        const bool ok = trimmed.width() == 320 && trimmed.height() == 480;
        out << "  " << (ok ? "PASS" : "FAIL")
            << "  auto_trim: 10% white border cropped to "
            << trimmed.width() << "x" << trimmed.height()
            << " (expected 320x480)\n";
        if (!ok) ++failed;
    }
    // 2. A solid-colour image has no detectable border (every side hits the
    //    cap), so we must NOT trim it — protects real content.
    {
        QImage solid(400, 600, QImage::Format_ARGB32);
        solid.fill(QColor(128, 128, 128));
        const QImage trimmed = ur::autoTrimBorders(solid);
        const bool ok = trimmed.width() == 400 && trimmed.height() == 600;
        out << "  " << (ok ? "PASS" : "FAIL")
            << "  auto_trim: uniform page left untouched ("
            << trimmed.width() << "x" << trimmed.height() << ")\n";
        if (!ok) ++failed;
    }

    // --- ThumbnailService ---------------------------------------------------
    // Decode at ~160px wide; verify size & aspect are roughly preserved, then
    // re-request and confirm the cached path fires synchronously.
    {
        constexpr int kThumbW = 160;
        ur::ThumbnailService thumbs(&book, kThumbW, 4LL * 1024 * 1024);

        QImage got;
        int    gotPage = -1;
        QObject::connect(&thumbs, &ur::ThumbnailService::thumbnailReady, &app,
            [&](int idx, const QImage& img) {
                gotPage = idx;
                got = img;
                app.quit();
            });
        thumbs.request(0);
        QTimer::singleShot(5000, &app, [&]() {
            err << "timeout waiting for thumbnail\n";
            app.quit();
        });
        app.exec();

        const bool delivered = gotPage == 0 && !got.isNull();
        out << "  " << (delivered ? "PASS" : "FAIL")
            << "  thumbs: page 0 decoded ("
            << got.width() << "x" << got.height() << ")\n";
        if (!delivered) ++failed;

        // The bundled cover.png is 800x1200 — at width 160 we expect ~240px tall.
        const bool sizedRight = delivered && got.width() == kThumbW
                              && got.height() >= 200 && got.height() <= 280;
        out << "  " << (sizedRight ? "PASS" : "FAIL")
            << "  thumbs: scaled to target width with preserved aspect\n";
        if (!sizedRight) ++failed;

        // Re-request: cache hit, the slot fires synchronously inside request().
        bool cacheHit = false;
        QObject::connect(&thumbs, &ur::ThumbnailService::thumbnailReady, &app,
            [&](int idx, const QImage&) { if (idx == 0) cacheHit = true; });
        thumbs.request(0);
        out << "  " << (cacheHit ? "PASS" : "FAIL")
            << "  thumbs: cached re-request fires synchronously\n";
        if (!cacheHit) ++failed;
    }

    return failed == 0 ? 0 : 1;
}
