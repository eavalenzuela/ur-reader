// ur-reader — entry point.
//
// Startup wiring:
//   1. Parse the command line          -> CliOptions::parse
//   2. Load global settings            -> AppConfig::load
//   3. Open the archive, build Book    -> ComicArchive::open, Book
//   4. Compute the book id             -> computeBookId
//   5. Load saved progress (if any)    -> ProgressFile::load
//   6. Resolve reading mode + page     -> ModeResolver::resolve
//   7. Refresh book metadata, bump openCount
//   8. Construct and show MainWindow
//
// On a fatal startup error (bad args, unreadable archive) print to stderr and
// exit non-zero so a launching library app can react.

#include <QApplication>
#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QImageReader>
#include <algorithm>
#include <optional>

#include "app/cli_options.h"
#include "app/main_window.h"
#include "app/mode_resolver.h"
#include "archive/comic_archive.h"
#include "core/stratified_sample.h"
#include "model/book.h"
#include "model/comic_info.h"
#include "session/app_config.h"
#include "session/book_identity.h"
#include "session/progress_file.h"

namespace {

// Probes image headers (cheap — no full decode) so reading-mode
// auto-detection has page sizes to work with at startup. We stratify the
// sample across the whole book so a book that opens on a wide cover or a
// run of front-matter spreads isn't mistaken for a webtoon based on its
// first few pages alone.
void probePageSizes(ur::ComicArchive* archive, ur::Book* book, int sampleCount)
{
    // Stratified across the whole book and read in ascending order — that
    // keeps the archive cursor on its fast path and avoids being fooled by
    // a wide cover or a run of front-matter spreads.
    for (int idx : ur::stratifiedSample(book->pageCount(), sampleCount)) {
        QByteArray bytes = archive->readEntry(idx, nullptr);
        if (bytes.isEmpty())
            continue;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const QSize size = reader.size();
        if (size.isValid())
            book->setPageSize(idx, size);
    }
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ur-reader"));
    app.setOrganizationName(QStringLiteral("ur-reader"));

    QString error;
    const auto cli = ur::CliOptions::parse(app.arguments(), &error);
    if (!cli) {
        qCritical().noquote() << error;
        return 2;
    }

    const ur::AppConfig config = ur::AppConfig::load();

    auto archive = ur::ComicArchive::open(cli->archivePath, &error);
    if (!archive) {
        qCritical().noquote() << error;
        return 1;
    }
    auto book = std::make_unique<ur::Book>(archive.get());
    if (book->pageCount() == 0) {
        qCritical().noquote() << "archive contains no pages";
        return 1;
    }

    ur::ComicInfoHints comicInfo;
    if (archive->hasComicInfo())
        comicInfo = ur::ComicInfoHints::parse(archive->comicInfoXml());

    const QString bookId = ur::computeBookId(cli->archivePath);
    const auto saved = ur::ProgressFile::load(bookId);

    // Auto-detection only matters when neither CLI nor saved state decides it.
    if (!cli->mode && !saved)
        probePageSizes(archive.get(), book.get(), 20);

    // ComicInfo per-page DoublePage hints become ForcePair overrides. Saved
    // user overrides take precedence — MainWindow re-applies them on top of
    // whatever we set here.
    for (auto it = comicInfo.pages.constBegin();
         it != comicInfo.pages.constEnd(); ++it) {
        const int idx = it.key();
        if (idx < 0 || idx >= book->pageCount())
            continue;
        if (saved && saved->spreadOverrides.contains(idx))
            continue;
        if (it.value().doublePage)
            book->page(idx).override = ur::SpreadOverride::ForcePair;
    }

    ur::ProgressData progress = saved.value_or(ur::ProgressData{});
    progress.schema = 1;
    progress.book.id = bookId;
    progress.book.path = cli->archivePath;
    progress.book.title = book->title();
    progress.book.pageCount = book->pageCount();

    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!progress.progress.firstOpened.isValid())
        progress.progress.firstOpened = now;
    progress.progress.lastRead = now;
    progress.progress.openCount += 1;

    progress.view.mode = ur::ModeResolver::resolve(
        cli->mode,
        saved ? std::optional<ur::ReadingMode>(saved->view.mode) : std::nullopt,
        *book, config.defaultMode);

    // Direction precedence: CLI > saved progress > ComicInfo Manga hint >
    // built-in default (RTL). saved overrides the struct default by virtue
    // of being applied above via value_or(saved->...), so we only consult
    // ComicInfo when no saved progress exists.
    if (cli->direction) {
        progress.view.direction = *cli->direction;
    } else if (!saved) {
        if (const auto dir = comicInfo.readingDirection())
            progress.view.direction = *dir;
    }

    int startPage = cli->page.value_or(saved ? saved->progress.page : 0);
    startPage = std::clamp(startPage, 0, book->pageCount() - 1);
    progress.progress.page = startPage;

    // Ownership of the Book passes to MainWindow.
    ur::Book* bookPtr = book.release();
    ur::MainWindow window(std::move(archive), bookPtr, progress, config);
    window.show();
    return app.exec();
}
