#pragma once

#include <QMainWindow>
#include <memory>

#include "core/types.h"
#include "session/app_config.h"
#include "session/progress_file.h"

namespace ur {

class Book;
class ComicArchive;
class DecodeService;
class ReaderView;

// Top-level window. Owns the archive / book / decoder, both view widgets (in
// a QStackedWidget), the chrome toolbar, and the live ProgressData. Wires page
// changes to debounced progress saves.
//
// MODULE: app shell.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(std::unique_ptr<ComicArchive> archive,
               Book*                         book,
               ProgressData                  progress,
               AppConfig                     config,
               QWidget*                      parent = nullptr);
    ~MainWindow() override;

public slots:
    void setMode(ReadingMode mode);   // top-right toggle + 'M' key
    void toggleFullscreen();

private slots:
    void onPageChanged(int pageIndex);
    void scheduleProgressSave();      // debounced via a QTimer

protected:
    void keyPressEvent(QKeyEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;   // right-click menu
    void closeEvent(QCloseEvent*) override;               // final flush + save
    bool eventFilter(QObject* watched, QEvent* event) override;  // mouse activity

private:
    void        buildToolBar();
    void        updateModeButton();
    void        updateChrome(int pageIndex);
    ReaderView* activeView() const;

    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ur
