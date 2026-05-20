#include "app/scrubber_preview.h"

#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QVBoxLayout>
#include <algorithm>

namespace ur {

namespace {
constexpr int kThumbDisplayW   = 160;
constexpr int kThumbDisplayMaxH = 240;       // cap so very tall webtoon pages don't grow huge
constexpr int kAnchorGapPx     = 18;          // gap between cursor and the bottom of the popup
constexpr int kScreenMargin    = 8;
}

ScrubberPreview::ScrubberPreview(QWidget* parent)
    : QWidget(parent,
              Qt::ToolTip | Qt::FramelessWindowHint
                  | Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground, false);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setFixedSize(kThumbDisplayW, kThumbDisplayMaxH);
    m_imageLabel->setStyleSheet(QStringLiteral(
        "background: #111; color: #888; border: 1px solid #333;"));

    m_textLabel = new QLabel(this);
    m_textLabel->setAlignment(Qt::AlignCenter);
    m_textLabel->setStyleSheet(QStringLiteral(
        "color: #ddd; background: #222; padding: 2px 6px;"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);
    layout->addWidget(m_imageLabel);
    layout->addWidget(m_textLabel);

    setStyleSheet(QStringLiteral("background: #222;"));
    hide();
}

void ScrubberPreview::renderThumbnail(const QImage& image)
{
    if (image.isNull()) {
        m_imageLabel->setPixmap(QPixmap());
        m_imageLabel->setText(QStringLiteral("…"));
        return;
    }
    const QImage scaled = image.scaled(kThumbDisplayW, kThumbDisplayMaxH,
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
    m_imageLabel->setText(QString());
    m_imageLabel->setPixmap(QPixmap::fromImage(scaled));
}

void ScrubberPreview::showForPage(int pageIndex, int totalPages,
                                  const QImage& image, QPoint globalAnchor)
{
    m_pageIndex = pageIndex;
    renderThumbnail(image);
    m_textLabel->setText(QStringLiteral("%1 / %2")
                             .arg(pageIndex + 1).arg(totalPages));
    adjustSize();
    reposition(globalAnchor);
    if (!isVisible())
        show();
    raise();
}

void ScrubberPreview::updateThumbnail(int pageIndex, const QImage& image)
{
    if (pageIndex != m_pageIndex)
        return;                          // user moved away; drop the late frame
    renderThumbnail(image);
    // Re-anchor in case the popup size changed.
    adjustSize();
    reposition(m_lastAnchor);
}

void ScrubberPreview::hidePreview()
{
    m_pageIndex = -1;
    hide();
}

void ScrubberPreview::reposition(QPoint globalAnchor)
{
    m_lastAnchor = globalAnchor;

    QRect screenRect;
    if (QScreen* screen = QGuiApplication::screenAt(globalAnchor))
        screenRect = screen->availableGeometry();
    else if (QScreen* primary = QGuiApplication::primaryScreen())
        screenRect = primary->availableGeometry();

    const QSize sz = size();
    int x = globalAnchor.x() - sz.width() / 2;
    int y = globalAnchor.y() - sz.height() - kAnchorGapPx;

    if (!screenRect.isNull()) {
        x = std::clamp(x, screenRect.left() + kScreenMargin,
                       screenRect.right() - sz.width() - kScreenMargin);
        // If the popup would clip off the top of the screen, flip it below
        // the cursor instead.
        if (y < screenRect.top() + kScreenMargin)
            y = globalAnchor.y() + kAnchorGapPx;
    }
    move(x, y);
}

} // namespace ur
