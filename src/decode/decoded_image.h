#pragma once

#include <QImage>

namespace ur {

// A decoded page image tagged with its page index.
struct DecodedImage {
    int    pageIndex = -1;
    QImage image;

    bool   valid() const { return pageIndex >= 0 && !image.isNull(); }
    qint64 byteCount() const { return image.sizeInBytes(); }
};

} // namespace ur
