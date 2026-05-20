#include "decode/thumbnail_service.h"

#include <QBuffer>
#include <QImageReader>
#include <QRunnable>
#include <algorithm>

#include "archive/comic_archive.h"
#include "model/book.h"

namespace ur {

ThumbnailService::ThumbnailService(Book*   book,
                                   int     thumbWidth,
                                   qint64  cacheBudgetBytes,
                                   QObject* parent)
    : QObject(parent)
    , m_book(book)
    , m_thumbWidth(std::max(32, thumbWidth))
    , m_cache(cacheBudgetBytes)
{
    // Thumbnails are a polish layer — keep the worker pool to one thread so
    // they can never starve the main DecodeService threadpool.
    m_pool.setMaxThreadCount(1);
}

ThumbnailService::~ThumbnailService()
{
    m_pool.clear();
    m_pool.waitForDone();
}

void ThumbnailService::request(int pageIndex)
{
    if (!m_book || pageIndex < 0 || pageIndex >= m_book->pageCount())
        return;

    if (const QImage* cached = m_cache.get(pageIndex)) {
        emit thumbnailReady(pageIndex, *cached);
        return;
    }
    if (m_inFlight.contains(pageIndex))
        return;
    enqueueDecode(pageIndex);
}

void ThumbnailService::enqueueDecode(int pageIndex)
{
    m_inFlight.insert(pageIndex);
    const int targetWidth = m_thumbWidth;

    m_pool.start(QRunnable::create([this, pageIndex, targetWidth]() {
        QImage image;
        QByteArray bytes = m_book->archive()->readEntry(pageIndex, nullptr);
        if (!bytes.isEmpty()) {
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            reader.setAutoTransform(true);
            // Ask the codec to scale during decode where possible (JPEG and
            // PNG support this in Qt). Falls back to a full decode + scale
            // for codecs that don't, which is still cheap at this size.
            const QSize full = reader.size();
            if (full.isValid() && full.width() > 0) {
                const int w = std::min(targetWidth, full.width());
                const int h = std::max(1, full.height() * w / full.width());
                reader.setScaledSize(QSize(w, h));
            }
            image = reader.read();
        }

        QMetaObject::invokeMethod(this, [this, pageIndex, image]() {
            deliver(pageIndex, image);
        }, Qt::QueuedConnection);
    }));
}

void ThumbnailService::deliver(int pageIndex, QImage image)
{
    m_inFlight.remove(pageIndex);
    if (image.isNull())
        return;                          // best-effort; silently skip failures
    m_cache.put(pageIndex, image);
    emit thumbnailReady(pageIndex, image);
}

} // namespace ur
