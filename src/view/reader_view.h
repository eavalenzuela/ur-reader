#pragma once

#include <QGraphicsView>
#include <QPoint>
#include "core/types.h"

namespace ur {

class Book;
class DecodeService;

// Common base for the two reading widgets. Both are QGraphicsView-based so
// they share zoom/pan, fit-mode handling, and the strict 2-zone click logic.
//
// MODULE: view widgets.
class ReaderView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ReaderView(QWidget* parent = nullptr);
    ~ReaderView() override;

    virtual void setBook(Book* book, DecodeService* decoder) = 0;
    virtual void goToPage(int pageIndex) = 0;
    virtual int  currentPage() const = 0;

    virtual void setDirection(ReadingDirection dir);
    virtual void setFitMode(FitMode fit);

signals:
    void pageChanged(int pageIndex);
    void reachedEnd();         // "next" issued past the last page
    void chromeRequested();    // right-click / chrome-reveal intent

protected:
    // Maps a click x-position to a step in *reading order* (-1 prev / +1
    // next), honouring direction. Shared by both subclasses — strict 2-zone.
    int clickStep(const QPoint& pos) const;

    ReadingDirection m_direction = ReadingDirection::RightToLeft;
    FitMode          m_fit = FitMode::Smart;
};

} // namespace ur
