#include "model/page.h"

namespace ur {

bool Page::isWideAuto() const
{
    return sizeKnown
        && pixelSize.width() > 0
        && pixelSize.height() > 0
        && pixelSize.width() > pixelSize.height();
}

bool Page::treatAsSpread() const
{
    switch (override) {
    case SpreadOverride::ForceSingle:
        return false;
    case SpreadOverride::ForcePair:
        return true;
    case SpreadOverride::Auto:
        return isWideAuto();
    }
    return false;
}

} // namespace ur
