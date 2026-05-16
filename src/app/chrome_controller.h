#pragma once

#include <QObject>
#include <QTimer>

namespace ur {

// Owns chrome (toolbar: scrubber, page indicator, mode toggle) visibility:
//
//   - windowed   : chrome always visible.
//   - fullscreen : chrome auto-hides; reveals on mouse activity, hides again
//                  after an idle timeout.
//
// Exiting fullscreen is via Esc or the right-click context menu — handled in
// MainWindow, not here.
//
// MODULE: app shell.
class ChromeController : public QObject {
    Q_OBJECT
public:
    explicit ChromeController(QObject* parent = nullptr);

    void setFullscreen(bool on);
    void notifyMouseActivity();      // call on mouse-move while reading

signals:
    void chromeVisibilityChanged(bool visible);

private:
    bool   m_fullscreen = false;
    bool   m_visible    = true;
    QTimer m_idleTimer;              // drives the fullscreen auto-hide
};

} // namespace ur
