#include "view/reader_view.h"

#include <QPainter>

namespace ur {

ReaderView::ReaderView(QWidget* parent)
    : QGraphicsView(parent)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setFocusPolicy(Qt::StrongFocus);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setRenderHint(QPainter::Antialiasing, true);
}

ReaderView::~ReaderView() = default;

void ReaderView::setDirection(ReadingDirection dir)
{
    m_direction = dir;
}

void ReaderView::setFitMode(FitMode fit)
{
    m_fit = fit;
}

int ReaderView::clickStep(const QPoint& pos) const
{
    const bool leftHalf = pos.x() < viewport()->width() / 2;

    // Zones map to *reading order*, not screen sides: in RTL the left side
    // advances. -1 = previous, +1 = next.
    if (m_direction == ReadingDirection::RightToLeft)
        return leftHalf ? +1 : -1;
    return leftHalf ? -1 : +1;
}

} // namespace ur
