#pragma once

#include <QHash>
#include <QList>
#include <QImage>

namespace ur {

// Memory-budgeted LRU cache of decoded page images. Explicit byte accounting
// (we deliberately avoid QPixmapCache to control the budget ourselves).
//
// Single-threaded: owned and accessed only by DecodeService.
//
// MODULE: decode/cache service.
class PageCache {
public:
    explicit PageCache(qint64 budgetBytes);

    bool          contains(int pageIndex) const;
    const QImage* get(int pageIndex);                 // touches LRU order
    void          put(int pageIndex, const QImage& image);

    void          setBudget(qint64 budgetBytes);
    void          clear();

private:
    void evictToBudget();

    qint64             m_budget;
    qint64             m_used = 0;
    QHash<int, QImage> m_images;
    QList<int>         m_lru;   // front = most recently used
};

} // namespace ur
