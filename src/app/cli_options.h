#pragma once

#include <QString>
#include <QStringList>
#include <optional>
#include "core/types.h"

namespace ur {

// Parsed command line. See DESIGN.md "Launch (Library -> Reader)":
//
//   ur-reader <archive-path> [--page N] [--mode paged|scroll]
//             [--direction rtl|ltr] [--session-id <uuid>]
//
// MODULE: app shell.
struct CliOptions {
    QString archivePath;                          // required positional

    std::optional<int>              page;         // --page
    std::optional<ReadingMode>      mode;         // --mode
    std::optional<ReadingDirection> direction;    // --direction
    QString                         sessionId;    // --session-id (correlation)

    // Parses argv. On error sets `error` and returns std::nullopt.
    static std::optional<CliOptions> parse(const QStringList& args, QString* error);
};

} // namespace ur
