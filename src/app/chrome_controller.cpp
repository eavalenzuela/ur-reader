#include "app/chrome_controller.h"

namespace ur {

ChromeController::ChromeController(QObject* parent)
    : QObject(parent)
{
    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(2000);   // hide chrome ~2s after the last move
    connect(&m_idleTimer, &QTimer::timeout, this, [this]() {
        if (m_fullscreen && m_visible) {
            m_visible = false;
            emit chromeVisibilityChanged(false);
        }
    });
}

void ChromeController::setFullscreen(bool on)
{
    m_fullscreen = on;
    m_visible = true;
    emit chromeVisibilityChanged(true);
    if (on)
        m_idleTimer.start();         // begin the auto-hide countdown
    else
        m_idleTimer.stop();          // windowed: chrome stays put
}

void ChromeController::notifyMouseActivity()
{
    if (!m_fullscreen)
        return;
    if (!m_visible) {
        m_visible = true;
        emit chromeVisibilityChanged(true);
    }
    m_idleTimer.start();             // restart the idle countdown
}

} // namespace ur
