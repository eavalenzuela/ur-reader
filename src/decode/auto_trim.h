#pragma once

#include <QImage>

namespace ur {

// Crops uniform border whitespace (or any uniform border colour, sampled
// from the top-left pixel) off a decoded page. Conservative: each side is
// only trimmed if non-border content is found within ~20% of that side's
// extent; otherwise that side is left untouched. Returns the input image
// untouched when nothing can be trimmed.
//
// MODULE: decode/cache service.
QImage autoTrimBorders(const QImage& src);

} // namespace ur
