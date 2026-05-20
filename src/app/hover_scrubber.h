#pragma once

#include <QSlider>

namespace ur {

// The page slider in the chrome, augmented with hover tracking so the
// MainWindow can drive a thumbnail preview popup. Pure widget concerns
// only — owns no thumbnail state itself.
//
// MODULE: app shell.
class HoverScrubber : public QSlider {
    Q_OBJECT
public:
    explicit HoverScrubber(QWidget* parent = nullptr);

    // Maps an x-coordinate (widget pixels) to the corresponding page index,
    // honouring inverted appearance (RTL).
    int pageAtX(int x) const;

signals:
    // Fires while the cursor is over the scrubber, including during drag.
    // `anchor` is in widget coordinates — caller should mapToGlobal it.
    void hoverPage(int pageIndex, QPoint anchor);
    void hoverLeft();

protected:
    void mouseMoveEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
};

} // namespace ur
