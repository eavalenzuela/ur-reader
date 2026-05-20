#pragma once

#include <QGraphicsScene>
#include <QTimer>
#include <QVector>

#include "view/reader_view.h"

QT_BEGIN_NAMESPACE
class QGraphicsPixmapItem;
QT_END_NAMESPACE

namespace ur {

// Continuous vertical-scroll (webtoon) reading. Every page is an item in a
// tall scene, each scaled to the viewport width. Heights are estimated until
// a page decodes, then the stack reflows with the real size.
//
// MODULE: view widgets.
class ScrollView : public ReaderView {
    Q_OBJECT
public:
    explicit ScrollView(QWidget* parent = nullptr);

    void setBook(Book* book, DecodeService* decoder) override;
    void goToPage(int pageIndex) override;
    int  currentPage() const override;          // top-most visible page

    void setSnap(bool on);                        // free-scroll default

protected:
    void mousePressEvent(QMouseEvent*) override; // left/right = page up/down
    void keyPressEvent(QKeyEvent*) override;     // Space = ~90% jump
    void resizeEvent(QResizeEvent*) override;
    void scrollContentsBy(int dx, int dy) override;

private slots:
    void onPageReady(int pageIndex, const QImage& image);

private:
    void  rebuildLayout();           // recompute item scales and y-positions
    void  refreshVisibleWindow();    // prefetch around the viewport
    void  settleSnap();              // align to nearest page top when idle
    qreal displayWidth() const;

    Book*          m_book = nullptr;
    DecodeService* m_decoder = nullptr;

    QGraphicsScene                m_scene;
    QVector<QGraphicsPixmapItem*> m_items;   // one per page
    QVector<qreal>                m_top;     // scene-y of each page's top
    bool                          m_snap = false;
    QTimer                        m_snapTimer;
    bool                          m_suppressSnap = false;   // re-entry guard
};

} // namespace ur
