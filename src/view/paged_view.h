#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QVector>

#include "view/reader_view.h"

QT_BEGIN_NAMESPACE
class QGraphicsPixmapItem;
QT_END_NAMESPACE

namespace ur {

// Single-page and double-page-spread reading.
//
// Pages are grouped into "units" of one or two page indices, accounting for
// double-page mode, cover-alone, pair-offset, and per-page spread overrides.
// The current unit is what's on screen; navigation moves between units.
//
// MODULE: view widgets.
class PagedView : public ReaderView {
    Q_OBJECT
public:
    explicit PagedView(QWidget* parent = nullptr);

    void setBook(Book* book, DecodeService* decoder) override;
    void goToPage(int pageIndex) override;
    int  currentPage() const override;

    void setDirection(ReadingDirection dir) override;
    void setFitMode(FitMode fit) override;

    void setDoublePage(bool on);
    void setCoverAlone(bool on);    // first page unpaired in double-page mode
    void setPairOffset(int offset); // nudge to fix misaligned spreads
    bool doublePage() const { return m_doublePage; }

protected:
    void mousePressEvent(QMouseEvent*) override;   // strict 2-zone paging
    void keyPressEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private slots:
    void onPageReady(int pageIndex, const QImage& image);

private:
    void rebuildUnits();                       // recompute spread grouping
    void showCurrentUnit();                    // request pages + render
    void layoutItems();                        // position the 1-2 page items
    void applyFit();
    void step(int delta);                      // move by delta units
    int  unitIndexForPage(int pageIndex) const;

    Book*          m_book = nullptr;
    DecodeService* m_decoder = nullptr;

    QGraphicsScene        m_scene;
    QVector<QVector<int>> m_units;              // each unit = 1 or 2 page indices
    int                   m_currentUnit = 0;
    QHash<int, QGraphicsPixmapItem*> m_items;   // page index -> item, current unit

    bool m_doublePage = false;
    bool m_coverAlone = true;
    int  m_pairOffset = 0;
    int  m_pageGap = 8;
};

} // namespace ur
