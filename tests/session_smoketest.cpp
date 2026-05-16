// Smoke test for the session module: book-id hashing, progress-file JSON
// round-trip, and AppConfig round-trip. Uses QStandardPaths test mode so it
// never touches the real user config/data directories.
//
//   session_smoketest <archive-path>

#include <QCoreApplication>
#include <QDateTime>
#include <QStandardPaths>
#include <QTextStream>

#include "session/app_config.h"
#include "session/book_identity.h"
#include "session/progress_file.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);

    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        err << "usage: session_smoketest <archive-path>\n";
        return 2;
    }

    const QString id = ur::computeBookId(args.at(1));
    out << "book id      : " << id << "\n";
    if (!id.startsWith(QLatin1String("p1:"))) {
        err << "unexpected id format\n";
        return 1;
    }
    out << "id stable    : "
        << (ur::computeBookId(args.at(1)) == id ? "yes" : "no") << "\n";

    // --- progress round-trip ---
    ur::ProgressData d;
    d.book.id = id;
    d.book.path = args.at(1);
    d.book.title = QStringLiteral("sample");
    d.book.pageCount = 5;
    d.progress.page = 3;
    d.progress.openCount = 7;
    d.progress.firstOpened = QDateTime::currentDateTimeUtc().addDays(-2);
    d.progress.lastRead = QDateTime::currentDateTimeUtc();
    d.view.mode = ur::ReadingMode::Scroll;
    d.view.doublePage = true;
    d.view.pairOffset = 1;
    d.spreadOverrides.insert(2, ur::SpreadOverride::ForcePair);
    d.spreadOverrides.insert(4, ur::SpreadOverride::ForceSingle);

    if (!ur::ProgressFile::save(d)) {
        err << "progress save failed\n";
        return 1;
    }
    out << "saved to     : " << ur::ProgressFile::pathForId(id) << "\n";

    const auto loaded = ur::ProgressFile::load(id);
    if (!loaded) {
        err << "progress load failed\n";
        return 1;
    }
    const bool progressOk =
        loaded->book.id == d.book.id &&
        loaded->book.pageCount == d.book.pageCount &&
        loaded->progress.page == d.progress.page &&
        loaded->progress.openCount == d.progress.openCount &&
        loaded->view.mode == ur::ReadingMode::Scroll &&
        loaded->view.doublePage &&
        loaded->view.pairOffset == 1 &&
        loaded->spreadOverrides.value(2) == ur::SpreadOverride::ForcePair &&
        loaded->spreadOverrides.value(4) == ur::SpreadOverride::ForceSingle;
    out << "progress R/T : page=" << loaded->progress.page
        << " openCount=" << loaded->progress.openCount
        << " overrides=" << loaded->spreadOverrides.size()
        << "  -> " << (progressOk ? "OK" : "MISMATCH") << "\n";

    out << "load(unknown): "
        << (ur::ProgressFile::load(QStringLiteral("p1:deadbeef")).has_value()
                ? "returned data (BAD)"
                : "empty (OK)")
        << "\n";

    // --- config round-trip ---
    ur::AppConfig cfg;
    cfg.pageGap = 12;
    cfg.defaultMode = ur::ReadingMode::Scroll;
    if (!cfg.save()) {
        err << "config save failed\n";
        return 1;
    }
    const ur::AppConfig back = ur::AppConfig::load();
    const bool configOk =
        back.pageGap == 12 && back.defaultMode == ur::ReadingMode::Scroll;
    out << "config R/T   : pageGap=" << back.pageGap
        << "  -> " << (configOk ? "OK" : "MISMATCH") << "\n";

    return (progressOk && configOk) ? 0 : 1;
}
