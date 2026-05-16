#include "view/paged_view.h"

#include <QGraphicsPixmapItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <algorithm>

#include "decode/decode_service.h"
#include "model/book.h"

namespace ur {

PagedView::PagedView(QWidget* parent)
    : ReaderView(parent)
{
    setScene(&m_scene);
}

void PagedView::setBook(Book* book, DecodeService* decoder)
{
    m_book = book;
    m_decoder = decoder;
    if (m_decoder) {
        connect(m_decoder, &DecodeService::pageReady,
                this, &PagedView::onPageReady, Qt::UniqueConnection);
    }
    rebuildUnits();
    m_currentUnit = 0;
    showCurrentUnit();
}

void PagedView::rebuildUnits()
{
    m_units.clear();
    if (!m_book)
        return;
    const int n = m_book->pageCount();

    if (!m_doublePage) {
        for (int p = 0; p < n; ++p)
            m_units.append(QVector<int>{p});
        return;
    }

    // Lead singletons: the cover (if cover-alone) plus any manual pair-offset.
    int singletons = std::max(0, (m_coverAlone ? 1 : 0) + m_pairOffset);
    int p = 0;
    while (p < n) {
        if (singletons > 0) {
            m_units.append(QVector<int>{p});
            ++p;
            --singletons;
        } else if (m_book->page(p).treatAsSpread()) {
            m_units.append(QVector<int>{p});            // wide page stands alone
            ++p;
        } else if (p + 1 < n && !m_book->page(p + 1).treatAsSpread()) {
            m_units.append(QVector<int>{p, p + 1});     // a normal pair
            p += 2;
        } else {
            m_units.append(QVector<int>{p});
            ++p;
        }
    }
}

int PagedView::unitIndexForPage(int pageIndex) const
{
    for (int i = 0; i < m_units.size(); ++i) {
        if (m_units[i].contains(pageIndex))
            return i;
    }
    return 0;
}

void PagedView::showCurrentUnit()
{
    m_scene.clear();
    m_items.clear();
    if (!m_book || m_units.isEmpty())
        return;
    m_currentUnit = std::clamp(m_currentUnit, 0, static_cast<int>(m_units.size()) - 1);

    const QVector<int>& unit = m_units[m_currentUnit];
    for (int page : unit)
        m_items.insert(page, m_scene.addPixmap(QPixmap()));

    layoutItems();
    applyFit();

    if (m_decoder) {
        for (int page : unit)
            m_decoder->request(page);
        m_decoder->setFocus(unit.first(), 4, 2);
    }
    emit pageChanged(unit.first());
}

void PagedView::onPageReady(int pageIndex, const QImage& image)
{
    auto it = m_items.find(pageIndex);
    if (it == m_items.end())
        return;   // page is not part of the spread currently on screen
    it.value()->setPixmap(QPixmap::fromImage(image));
    layoutItems();
    applyFit();
}

void PagedView::layoutItems()
{
    if (m_units.isEmpty() || m_items.isEmpty())
        return;
    QVector<int> order = m_units[m_currentUnit];

    // Place pages left-to-right on screen; RTL puts the later page on the left.
    if (m_direction == ReadingDirection::RightToLeft && order.size() == 2)
        std::swap(order[0], order[1]);

    qreal maxHeight = 0;
    for (int page : order) {
        if (auto* item = m_items.value(page))
            maxHeight = std::max(maxHeight, qreal(item->pixmap().height()));
    }

    qreal x = 0;
    for (int page : order) {
        auto* item = m_items.value(page);
        if (!item)
            continue;
        const qreal h = item->pixmap().height();
        item->setPos(x, (maxHeight - h) / 2.0);   // vertically centred
        x += item->pixmap().width() + m_pageGap;
    }
    m_scene.setSceneRect(m_scene.itemsBoundingRect());
}

void PagedView::applyFit()
{
    const QRectF content = m_scene.itemsBoundingRect();
    if (content.isEmpty())
        return;
    m_scene.setSceneRect(content);
    resetTransform();

    const qreal vw = viewport()->width();
    const qreal vh = viewport()->height();
    if (vw <= 0 || vh <= 0)
        return;

    const qreal sWidth  = vw / content.width();
    const qreal sHeight = vh / content.height();
    const qreal sWhole  = std::min(sWidth, sHeight);

    qreal s = 1.0;
    switch (m_fit) {
    case FitMode::Original: s = 1.0;     break;
    case FitMode::Width:    s = sWidth;  break;
    case FitMode::Height:   s = sHeight; break;
    case FitMode::Whole:    s = sWhole;  break;
    case FitMode::Smart:
        // Whole, unless the spread is much taller than the viewport aspect.
        s = (content.height() / content.width() > 1.5 * (vh / vw)) ? sWidth
                                                                   : sWhole;
        break;
    }
    scale(s, s);
    centerOn(content.center());
}

void PagedView::step(int delta)
{
    if (m_units.isEmpty())
        return;
    const int target = m_currentUnit + delta;
    if (target >= m_units.size()) {
        emit reachedEnd();
        return;
    }
    if (target < 0)
        return;
    m_currentUnit = target;
    showCurrentUnit();
}

void PagedView::goToPage(int pageIndex)
{
    m_currentUnit = unitIndexForPage(pageIndex);
    showCurrentUnit();
}

int PagedView::currentPage() const
{
    if (m_units.isEmpty())
        return 0;
    const int idx = std::clamp(m_currentUnit, 0, static_cast<int>(m_units.size()) - 1);
    return m_units[idx].first();
}

void PagedView::setDirection(ReadingDirection dir)
{
    ReaderView::setDirection(dir);
    layoutItems();
    applyFit();
}

void PagedView::setFitMode(FitMode fit)
{
    ReaderView::setFitMode(fit);
    applyFit();
}

void PagedView::setDoublePage(bool on)
{
    if (m_doublePage == on)
        return;
    const int lead = currentPage();
    m_doublePage = on;
    rebuildUnits();
    m_currentUnit = unitIndexForPage(lead);
    showCurrentUnit();
}

void PagedView::setCoverAlone(bool on)
{
    if (m_coverAlone == on)
        return;
    const int lead = currentPage();
    m_coverAlone = on;
    rebuildUnits();
    m_currentUnit = unitIndexForPage(lead);
    showCurrentUnit();
}

void PagedView::setPairOffset(int offset)
{
    if (m_pairOffset == offset)
        return;
    const int lead = currentPage();
    m_pairOffset = offset;
    rebuildUnits();
    m_currentUnit = unitIndexForPage(lead);
    showCurrentUnit();
}

void PagedView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        step(clickStep(event->pos()));
        event->accept();
        return;
    }
    ReaderView::mousePressEvent(event);
}

void PagedView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_PageDown:
        step(+1);
        return;
    case Qt::Key_Backspace:
    case Qt::Key_PageUp:
        step(-1);
        return;
    case Qt::Key_Right:
        step(m_direction == ReadingDirection::RightToLeft ? -1 : +1);
        return;
    case Qt::Key_Left:
        step(m_direction == ReadingDirection::RightToLeft ? +1 : -1);
        return;
    case Qt::Key_Home:
        m_currentUnit = 0;
        showCurrentUnit();
        return;
    case Qt::Key_End:
        m_currentUnit = static_cast<int>(m_units.size()) - 1;
        showCurrentUnit();
        return;
    default:
        ReaderView::keyPressEvent(event);
    }
}

void PagedView::resizeEvent(QResizeEvent* event)
{
    ReaderView::resizeEvent(event);
    applyFit();
}

} // namespace ur
