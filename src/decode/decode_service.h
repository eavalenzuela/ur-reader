#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QThreadPool>
#include <QSet>
#include "decode/page_cache.h"

namespace ur {

class Book;

// Asynchronous page decode + caching. Pages are read from the archive and
// decoded on a QThreadPool; results are delivered via pageReady on the
// caller's thread. Holds the PageCache and a prefetch window.
//
// MODULE: decode/cache service.
class DecodeService : public QObject {
    Q_OBJECT
public:
    DecodeService(Book* book, qint64 cacheBudgetBytes, QObject* parent = nullptr);
    ~DecodeService() override;

    // Requests a page. A cache hit emits pageReady promptly; a miss schedules
    // a background decode.
    void request(int pageIndex);

    // Sets the page the user is on; the service prefetches a window of
    // neighbouring pages in reading order and lets the cache evict the rest.
    void setFocus(int pageIndex, int aheadCount, int behindCount);

signals:
    void pageReady(int pageIndex, QImage image);
    void pageFailed(int pageIndex, QString error);

private:
    // Schedules a background decode for a page not already cached/in-flight.
    void enqueueDecode(int pageIndex);

    // Runs on the service thread (queued from a worker) when a decode ends:
    // updates the cache, feeds the size back to the Book, emits the result.
    void deliver(int pageIndex, QImage image, QString error);

    Book*       m_book;
    PageCache   m_cache;        // touched only on the service thread
    QThreadPool m_pool;         // background decode workers
    QSet<int>   m_inFlight;     // pages currently being decoded
};

} // namespace ur
