#include "decode/auto_trim.h"

#include <algorithm>
#include <cstdlib>

namespace ur {

namespace {

// Tuned for badly-scanned pages with uniform white or black borders.
// `kTrimMaxFractionPerSide` is a soft cap: if the matching run reaches the
// cap we treat the page as having no clear border on that side and skip it.
constexpr int   kTrimSampleStep         = 8;     // every Nth pixel along the edge
constexpr int   kTrimColorTolerance     = 12;    // per channel
constexpr qreal kTrimMaxFractionPerSide = 0.20;

bool channelsClose(QRgb a, QRgb b, int tol)
{
    return std::abs(qRed(a)   - qRed(b))   <= tol
        && std::abs(qGreen(a) - qGreen(b)) <= tol
        && std::abs(qBlue(a)  - qBlue(b))  <= tol;
}

bool rowMatches(const QImage& img, int y, QRgb sample, int tol)
{
    const auto* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
    for (int x = 0; x < img.width(); x += kTrimSampleStep) {
        if (!channelsClose(line[x], sample, tol))
            return false;
    }
    return true;
}

bool columnMatches(const QImage& img, int x, QRgb sample, int tol)
{
    for (int y = 0; y < img.height(); y += kTrimSampleStep) {
        const auto* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        if (!channelsClose(line[x], sample, tol))
            return false;
    }
    return true;
}

} // namespace

QImage autoTrimBorders(const QImage& src)
{
    if (src.width() < 32 || src.height() < 32)
        return src;

    QImage img = src;
    if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_RGB32)
        img = img.convertToFormat(QImage::Format_ARGB32);

    const QRgb sample = reinterpret_cast<const QRgb*>(img.constScanLine(0))[0];

    const int vLimit = static_cast<int>(img.height() * kTrimMaxFractionPerSide);
    const int hLimit = static_cast<int>(img.width()  * kTrimMaxFractionPerSide);

    int top = 0;
    while (top < vLimit && rowMatches(img, top, sample, kTrimColorTolerance))
        ++top;
    if (top == vLimit) top = 0;

    const int bottomMin = img.height() - 1 - vLimit;
    int bottom = img.height() - 1;
    while (bottom > bottomMin
           && rowMatches(img, bottom, sample, kTrimColorTolerance))
        --bottom;
    if (bottom == bottomMin) bottom = img.height() - 1;

    int left = 0;
    while (left < hLimit && columnMatches(img, left, sample, kTrimColorTolerance))
        ++left;
    if (left == hLimit) left = 0;

    const int rightMin = img.width() - 1 - hLimit;
    int right = img.width() - 1;
    while (right > rightMin
           && columnMatches(img, right, sample, kTrimColorTolerance))
        --right;
    if (right == rightMin) right = img.width() - 1;

    if (top == 0 && left == 0
        && bottom == img.height() - 1 && right == img.width() - 1)
        return src;

    return img.copy(left, top, right - left + 1, bottom - top + 1);
}

} // namespace ur
