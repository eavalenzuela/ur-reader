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
#include "model/book.h"
#include "session/app_config.h"
#include "session/book_identity.h"
#include "session/progress_file.h"

namespace {

// Probes image headers (cheap — no full decode) so reading-mode
// auto-detection has page sizes to work with at startup.
void probePageSizes(ur::ComicArchive* archive, ur::Book* book, int limit)
{
    const int count = std::min(limit, book->pageCount());
    for (int i = 0; i < count; ++i) {
        QByteArray bytes = archive->readEntry(i, nullptr);
        if (bytes.isEmpty())
            continue;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const QSize size = reader.size();
        if (size.isValid())
            book->setPageSize(i, size);
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

    const QString bookId = ur::computeBookId(cli->archivePath);
    const auto saved = ur::ProgressFile::load(bookId);

    // Auto-detection only matters when neither CLI nor saved state decides it.
    if (!cli->mode && !saved)
        probePageSizes(archive.get(), book.get(), 10);

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
    if (cli->direction)
        progress.view.direction = *cli->direction;

    int startPage = cli->page.value_or(saved ? saved->progress.page : 0);
    startPage = std::clamp(startPage, 0, book->pageCount() - 1);
    progress.progress.page = startPage;

    // Ownership of the Book passes to MainWindow.
    ur::Book* bookPtr = book.release();
    ur::MainWindow window(std::move(archive), bookPtr, progress, config);
    window.show();
    return app.exec();
}
