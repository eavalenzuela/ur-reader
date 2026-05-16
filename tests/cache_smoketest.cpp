// Smoke test for PageCache: verifies budget accounting, LRU eviction order,
// and clear(). Uses synthetic QImages, so it needs no image files.

#include <QImage>
#include <QTextStream>

#include "decode/page_cache.h"

int main()
{
    QTextStream out(stdout);

    // Each 1000x1000 ARGB32 image is 4,000,000 bytes. A 9 MB budget holds two.
    ur::PageCache cache(9LL * 1000 * 1000);
    const auto makeImage = []() { return QImage(1000, 1000, QImage::Format_ARGB32); };

    cache.put(0, makeImage());
    cache.put(1, makeImage());
    out << "put 0,1            -> 0:" << cache.contains(0)
        << " 1:" << cache.contains(1) << "  (expect 1 1)\n";

    cache.get(0);                       // touch 0 -> page 1 becomes LRU
    cache.put(2, makeImage());          // over budget -> evict LRU (page 1)
    out << "touch 0, put 2     -> 0:" << cache.contains(0)
        << " 1:" << cache.contains(1)
        << " 2:" << cache.contains(2) << "  (expect 1 0 1)\n";

    cache.put(3, makeImage());          // evict next LRU (page 0)
    out << "put 3              -> 0:" << cache.contains(0)
        << " 2:" << cache.contains(2)
        << " 3:" << cache.contains(3) << "  (expect 0 1 1)\n";

    cache.clear();
    out << "clear              -> 3:" << cache.contains(3)
        << "  (expect 0)\n";

    return 0;
}
