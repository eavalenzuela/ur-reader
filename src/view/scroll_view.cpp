#include "view/scroll_view.h"

#include <QGraphicsPixmapItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <algorithm>

#include "decode/decode_service.h"
#include "model/book.h"

namespace ur {

namespace {
constexpr qreal kEstimateAspect = 1.45;   // assumed height/width before decode
constexpr qreal kPageGap = 8.0;
}

ScrollView::ScrollView(QWidget* parent)
    : ReaderView(parent)
{
    setScene(&m_scene);
    setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    // Scroll mode keeps the identity transform and scales the page items
    // themselves, so scene coordinates equal on-screen pixels.

    m_snapTimer.setSingleShot(true);
    m_snapTimer.setInterval(120);   // idle-settle window after the last scroll
    connect(&m_snapTimer, &QTimer::timeout, this, &ScrollView::settleSnap);
}

void ScrollView::setSnap(bool on)
{
    if (m_snap == on)
        return;
    m_snap = on;
    if (m_snap)
        m_snapTimer.start();        // settle to the nearest page right away
    else
        m_snapTimer.stop();
}

qreal ScrollView::displayWidth() const
{
    const int w = viewport()->width();
    return w > 0 ? qreal(w) : 800.0;
}

void ScrollView::setBook(Book* book, DecodeService* decoder)
{
    m_book = book;
    m_decoder = decoder;
    m_scene.clear();
    m_items.clear();
    m_top.clear();
    if (!m_book)
        return;
    if (m_decoder) {
        connect(m_decoder, &DecodeService::pageReady,
                this, &ScrollView::onPageReady, Qt::UniqueConnection);
    }

    const int n = m_book->pageCount();
    m_items.reserve(n);
    m_top.resize(n);
    for (int i = 0; i < n; ++i)
        m_items.append(m_scene.addPixmap(QPixmap()));

    rebuildLayout();
    refreshVisibleWindow();
}

void ScrollView::rebuildLayout()
{
    const qreal dw = displayWidth();
    qreal y = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        QGraphicsPixmapItem* item = m_items[i];
        const QPixmap pm = item->pixmap();
        qreal height;
        if (!pm.isNull() && pm.width() > 0) {
            const qreal scale = dw / pm.width();
            item->setScale(scale);
            height = pm.height() * scale;
        } else {
            item->setScale(1.0);
            height = dw * kEstimateAspect;   // estimate until the page decodes
        }
        item->setPos(0, y);
        m_top[i] = y;
        y += height + kPageGap;
    }
    m_scene.setSceneRect(0, 0, dw, std::max<qreal>(y, 1.0));
}

void ScrollView::onPageReady(int pageIndex, const QImage& image)
{
    if (pageIndex < 0 || pageIndex >= m_items.size())
        return;
    m_items[pageIndex]->setPixmap(QPixmap::fromImage(image));
    rebuildLayout();
}

void ScrollView::refreshVisibleWindow()
{
    if (m_decoder && !m_items.isEmpty())
        m_decoder->setFocus(currentPage(), 4, 1);
}

int ScrollView::currentPage() const
{
    if (m_top.isEmpty())
        return 0;
    const qreal topY = mapToScene(0, 0).y();
    int page = 0;
    for (int i = 0; i < m_top.size(); ++i) {
        if (m_top[i] <= topY)
            page = i;
        else
            break;
    }
    return page;
}

void ScrollView::goToPage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_top.size())
        return;
    centerOn(displayWidth() / 2.0,
             m_top[pageIndex] + viewport()->height() / 2.0);
    refreshVisibleWindow();
}

void ScrollView::scrollContentsBy(int dx, int dy)
{
    ReaderView::scrollContentsBy(dx, dy);
    emit pageChanged(currentPage());
    // Restart the settle timer on every scroll so we only snap when the user
    // (or a programmatic jump) actually stops moving the viewport.
    if (m_snap && !m_suppressSnap)
        m_snapTimer.start();
}

void ScrollView::settleSnap()
{
    if (!m_snap || m_top.isEmpty())
        return;
    const qreal topY = mapToScene(0, 0).y();
    int   best = 0;
    qreal bestDist = std::abs(m_top[0] - topY);
    for (int i = 1; i < m_top.size(); ++i) {
        const qreal d = std::abs(m_top[i] - topY);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    if (bestDist < 1.0)
        return;                       // already aligned, nothing to do
    m_suppressSnap = true;            // don't re-arm from our own scroll
    centerOn(displayWidth() / 2.0,
             m_top[best] + viewport()->height() / 2.0);
    m_suppressSnap = false;
}

void ScrollView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Strict 2-zone, mapped to reading order: forward scrolls down.
        const int dir = clickStep(event->pos());
        const int delta = static_cast<int>(dir * 0.9 * viewport()->height());
        verticalScrollBar()->setValue(verticalScrollBar()->value() + delta);
        refreshVisibleWindow();
        event->accept();
        return;
    }
    ReaderView::mousePressEvent(event);
}

void ScrollView::keyPressEvent(QKeyEvent* event)
{
    const int jump = static_cast<int>(0.9 * viewport()->height());
    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_PageDown:
        verticalScrollBar()->setValue(verticalScrollBar()->value() + jump);
        refreshVisibleWindow();
        return;
    case Qt::Key_PageUp:
        verticalScrollBar()->setValue(verticalScrollBar()->value() - jump);
        refreshVisibleWindow();
        return;
    case Qt::Key_Home:
        goToPage(0);
        return;
    case Qt::Key_End:
        goToPage(static_cast<int>(m_top.size()) - 1);
        return;
    default:
        ReaderView::keyPressEvent(event);
    }
}

void ScrollView::resizeEvent(QResizeEvent* event)
{
    const int keep = currentPage();
    ReaderView::resizeEvent(event);
    rebuildLayout();
    goToPage(keep);
}

} // namespace ur
