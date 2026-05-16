#include "decode/page_cache.h"

namespace ur {

PageCache::PageCache(qint64 budgetBytes)
    : m_budget(budgetBytes)
{
}

bool PageCache::contains(int pageIndex) const
{
    return m_images.contains(pageIndex);
}

const QImage* PageCache::get(int pageIndex)
{
    auto it = m_images.constFind(pageIndex);
    if (it == m_images.constEnd())
        return nullptr;

    // Mark as most recently used.
    m_lru.removeOne(pageIndex);
    m_lru.prepend(pageIndex);
    return &it.value();
}

void PageCache::put(int pageIndex, const QImage& image)
{
    auto existing = m_images.find(pageIndex);
    if (existing != m_images.end()) {
        // Replace: adjust the byte total by the difference.
        m_used += image.sizeInBytes() - existing.value().sizeInBytes();
        existing.value() = image;
        m_lru.removeOne(pageIndex);
        m_lru.prepend(pageIndex);
    } else {
        m_images.insert(pageIndex, image);
        m_used += image.sizeInBytes();
        m_lru.prepend(pageIndex);
    }
    evictToBudget();
}

void PageCache::setBudget(qint64 budgetBytes)
{
    m_budget = budgetBytes;
    evictToBudget();
}

void PageCache::clear()
{
    m_images.clear();
    m_lru.clear();
    m_used = 0;
}

void PageCache::evictToBudget()
{
    // Drop least-recently-used entries until within budget. Keep at least one
    // entry so a single oversized image is still usable.
    while (m_used > m_budget && m_lru.size() > 1) {
        const int victim = m_lru.takeLast();
        auto it = m_images.find(victim);
        if (it != m_images.end()) {
            m_used -= it.value().sizeInBytes();
            m_images.erase(it);
        }
    }
}

} // namespace ur
