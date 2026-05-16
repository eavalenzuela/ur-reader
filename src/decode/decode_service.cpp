#include "decode/decode_service.h"

#include <QBuffer>
#include <QImageReader>
#include <QRunnable>
#include <QThread>
#include <algorithm>

#include "archive/comic_archive.h"
#include "model/book.h"

namespace ur {

DecodeService::DecodeService(Book* book, qint64 cacheBudgetBytes, QObject* parent)
    : QObject(parent)
    , m_book(book)
    , m_cache(cacheBudgetBytes)
{
    m_pool.setMaxThreadCount(std::max(2, QThread::idealThreadCount()));
}

DecodeService::~DecodeService()
{
    // Drop queued work and let running decodes finish before teardown. Any
    // queued deliver() events targeting this object are cleared by ~QObject.
    m_pool.clear();
    m_pool.waitForDone();
}

void DecodeService::request(int pageIndex)
{
    if (!m_book || pageIndex < 0 || pageIndex >= m_book->pageCount())
        return;

    if (const QImage* cached = m_cache.get(pageIndex)) {
        emit pageReady(pageIndex, *cached);
        return;
    }
    if (m_inFlight.contains(pageIndex))
        return;

    enqueueDecode(pageIndex);
}

void DecodeService::setFocus(int pageIndex, int aheadCount, int behindCount)
{
    if (!m_book)
        return;

    const int count = m_book->pageCount();
    const int first = std::max(0, pageIndex - behindCount);
    const int last  = std::min(count - 1, pageIndex + aheadCount);

    // Focused page first, then fan outward so the nearest neighbours decode
    // soonest. Requests in reading order keep sequential archives happy.
    request(pageIndex);
    const int reach = std::max(aheadCount, behindCount);
    for (int d = 1; d <= reach; ++d) {
        if (pageIndex + d <= last)
            request(pageIndex + d);
        if (pageIndex - d >= first)
            request(pageIndex - d);
    }
}

void DecodeService::enqueueDecode(int pageIndex)
{
    m_inFlight.insert(pageIndex);

    m_pool.start(QRunnable::create([this, pageIndex]() {
        QString error;
        QImage  image;

        QByteArray bytes = m_book->archive()->readEntry(pageIndex, &error);
        if (bytes.isEmpty()) {
            if (error.isEmpty())
                error = QStringLiteral("no page data");
        } else {
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            reader.setAutoTransform(true);
            image = reader.read();
            if (image.isNull())
                error = QStringLiteral("image decode failed: %1")
                            .arg(reader.errorString());
        }

        // Hop back to the service thread to touch the cache and emit.
        QMetaObject::invokeMethod(this, [this, pageIndex, image, error]() {
            deliver(pageIndex, image, error);
        }, Qt::QueuedConnection);
    }));
}

void DecodeService::deliver(int pageIndex, QImage image, QString error)
{
    m_inFlight.remove(pageIndex);

    if (image.isNull()) {
        emit pageFailed(pageIndex, error);
        return;
    }

    m_cache.put(pageIndex, image);
    if (m_book)
        m_book->setPageSize(pageIndex, image.size());   // model learns real size
    emit pageReady(pageIndex, image);
}

} // namespace ur
