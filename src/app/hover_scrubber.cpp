#include "app/hover_scrubber.h"

#include <QMouseEvent>
#include <QStyle>

namespace ur {

HoverScrubber::HoverScrubber(QWidget* parent)
    : QSlider(Qt::Horizontal, parent)
{
    setMouseTracking(true);              // we want move events without a button held
}

int HoverScrubber::pageAtX(int x) const
{
    // QStyle handles inverted appearance (RTL) and the handle's half-width
    // padding at the ends, so the slider's own value math matches what the
    // user perceives as "the page under the cursor".
    return QStyle::sliderValueFromPosition(
        minimum(), maximum(), x, width(), invertedAppearance());
}

void HoverScrubber::mouseMoveEvent(QMouseEvent* event)
{
    emit hoverPage(pageAtX(event->position().toPoint().x()),
                   event->position().toPoint());
    QSlider::mouseMoveEvent(event);      // keep normal drag behaviour
}

void HoverScrubber::enterEvent(QEnterEvent* event)
{
    emit hoverPage(pageAtX(event->position().toPoint().x()),
                   event->position().toPoint());
    QSlider::enterEvent(event);
}

void HoverScrubber::leaveEvent(QEvent* event)
{
    emit hoverLeft();
    QSlider::leaveEvent(event);
}

} // namespace ur
