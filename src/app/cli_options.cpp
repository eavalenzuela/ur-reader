#include "app/cli_options.h"

namespace ur {

std::optional<CliOptions> CliOptions::parse(const QStringList& args, QString* error)
{
    const auto fail = [error](const QString& msg) -> std::optional<CliOptions> {
        if (error)
            *error = msg;
        return std::nullopt;
    };

    CliOptions opt;

    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args.at(i);
        const QString key = arg.section(QLatin1Char('='), 0, 0);

        // Resolves the value for an option, supporting "--opt=value" and
        // "--opt value". Advances `i` when it consumes the next argument.
        const auto valueFor = [&]() -> QString {
            if (arg.contains(QLatin1Char('=')))
                return arg.section(QLatin1Char('='), 1);
            if (i + 1 < args.size())
                return args.at(++i);
            return QString();
        };

        if (key == QLatin1String("--page")) {
            bool ok = false;
            const int page = valueFor().toInt(&ok);
            if (!ok)
                return fail(QStringLiteral("--page requires an integer"));
            opt.page = page;
        } else if (key == QLatin1String("--mode")) {
            const QString v = valueFor();
            if (v == QLatin1String("paged"))
                opt.mode = ReadingMode::Paged;
            else if (v == QLatin1String("scroll"))
                opt.mode = ReadingMode::Scroll;
            else
                return fail(QStringLiteral("--mode must be 'paged' or 'scroll'"));
        } else if (key == QLatin1String("--direction")) {
            const QString v = valueFor();
            if (v == QLatin1String("rtl"))
                opt.direction = ReadingDirection::RightToLeft;
            else if (v == QLatin1String("ltr"))
                opt.direction = ReadingDirection::LeftToRight;
            else
                return fail(QStringLiteral("--direction must be 'rtl' or 'ltr'"));
        } else if (key == QLatin1String("--session-id")) {
            opt.sessionId = valueFor();
        } else if (arg.startsWith(QLatin1String("--"))) {
            return fail(QStringLiteral("unknown option: %1").arg(arg));
        } else if (opt.archivePath.isEmpty()) {
            opt.archivePath = arg;
        } else {
            return fail(QStringLiteral("unexpected extra argument: %1").arg(arg));
        }
    }

    if (opt.archivePath.isEmpty()) {
        return fail(QStringLiteral(
            "usage: ur-reader <archive> [--page N] [--mode paged|scroll] "
            "[--direction rtl|ltr] [--session-id ID]"));
    }
    return opt;
}

} // namespace ur
