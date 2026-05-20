#pragma once

#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>

#include "decode/page_cache.h"

namespace ur {

class Book;

// Produces low-resolution page thumbnails for the scrubber preview. Lives
// alongside DecodeService but is intentionally separate: its cache budget
// is tiny (room for many thumbs, not a few full pages), its worker pool
// is single-threaded so it never starves the full-page decoder, and it
// uses QImageReader's setScaledSize to avoid decoding pixels we're going
// to throw away.
//
// MODULE: decode/cache service.
class ThumbnailService : public QObject {
    Q_OBJECT
public:
    ThumbnailService(Book*   book,
                     int     thumbWidth,        // longest-edge target, in px
                     qint64  cacheBudgetBytes,
                     QObject* parent = nullptr);
    ~ThumbnailService() override;

    // Synchronous cache-hit fires thumbnailReady immediately; cache miss
    // schedules a background decode.
    void request(int pageIndex);

    int thumbWidth() const { return m_thumbWidth; }

signals:
    void thumbnailReady(int pageIndex, QImage image);

private:
    void enqueueDecode(int pageIndex);
    void deliver(int pageIndex, QImage image);

    Book*       m_book;
    int         m_thumbWidth;
    PageCache   m_cache;       // touched only on the service thread
    QThreadPool m_pool;        // 1 worker; thumbs are background polish
    QSet<int>   m_inFlight;
};

} // namespace ur
