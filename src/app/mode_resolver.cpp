#include "app/mode_resolver.h"

#include "model/book.h"

namespace ur {

ReadingMode ModeResolver::resolve(std::optional<ReadingMode> cli,
                                  std::optional<ReadingMode> saved,
                                  const Book&                book,
                                  ReadingMode                configDefault)
{
    if (cli)
        return *cli;                       // 1. explicit CLI override
    if (saved)
        return *saved;                     // 2. per-book remembered mode
    if (const auto detected = book.detectMode())
        return *detected;                  // 3. aspect-ratio auto-detection
    return configDefault;                  // 4. global default
}

} // namespace ur
