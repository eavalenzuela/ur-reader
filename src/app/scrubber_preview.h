#pragma once

#include <QImage>
#include <QPoint>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace ur {

// Floating popup that hovers above the scrubber and shows a thumbnail +
// page-number for the page under the cursor. Frameless top-level (Qt::ToolTip
// flag) so it draws above the toolbar without being clipped.
//
// The MainWindow drives it: show / hide / update happen in response to
// HoverScrubber signals and ThumbnailService::thumbnailReady. The widget
// itself just renders what it's told.
//
// MODULE: app shell.
class ScrubberPreview : public QWidget {
    Q_OBJECT
public:
    explicit ScrubberPreview(QWidget* parent = nullptr);

    // Show the preview for `pageIndex` (1-based shown to the user, 0-based
    // here for consistency with the rest of the app). `image` may be null
    // when the thumbnail hasn't decoded yet — a placeholder is rendered.
    // `globalAnchor` is the cursor's screen position; the preview centres
    // horizontally on it and sits above it, clamped to the screen edges.
    void showForPage(int          pageIndex,
                     int          totalPages,
                     const QImage& image,
                     QPoint        globalAnchor);

    // Replace the thumbnail if the popup is still showing this page (a late
    // decode for a page the user has already moved away from is dropped).
    void updateThumbnail(int pageIndex, const QImage& image);

    void hidePreview();

    int currentPage() const { return m_pageIndex; }

private:
    void reposition(QPoint globalAnchor);
    void renderThumbnail(const QImage& image);

    QLabel* m_imageLabel;
    QLabel* m_textLabel;
    int     m_pageIndex = -1;
    QPoint  m_lastAnchor;
};

} // namespace ur
