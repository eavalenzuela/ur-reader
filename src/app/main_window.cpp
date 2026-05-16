#include "app/main_window.h"

#include <QBrush>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <algorithm>

#include "archive/comic_archive.h"
#include "app/chrome_controller.h"
#include "decode/decode_service.h"
#include "model/book.h"
#include "session/progress_file.h"
#include "view/paged_view.h"
#include "view/scroll_view.h"

namespace ur {

struct MainWindow::Impl {
    // Declared in dependency order so destruction unwinds safely:
    // decoder (its threads use the book) -> book (refers to archive) -> archive.
    std::unique_ptr<ComicArchive>  archive;
    std::unique_ptr<Book>          book;
    std::unique_ptr<DecodeService> decoder;

    QStackedWidget* stack  = nullptr;
    PagedView*      paged  = nullptr;
    ScrollView*     scroll = nullptr;

    ChromeController* chrome     = nullptr;
    QToolBar*         toolbar    = nullptr;
    QLabel*           pageLabel  = nullptr;
    QSlider*          slider     = nullptr;
    QToolButton*      modeButton = nullptr;

    QTimer       saveTimer;
    ProgressData progress;
    AppConfig    config;
    ReadingMode  mode = ReadingMode::Paged;
};

MainWindow::MainWindow(std::unique_ptr<ComicArchive> archive,
                       Book*                         book,
                       ProgressData                  progress,
                       AppConfig                     config,
                       QWidget*                      parent)
    : QMainWindow(parent)
    , d(std::make_unique<Impl>())
{
    d->archive = std::move(archive);
    d->book.reset(book);
    d->progress = std::move(progress);
    d->config = config;
    d->mode = d->progress.view.mode;

    // Apply persisted per-page spread overrides to the model.
    for (auto it = d->progress.spreadOverrides.constBegin();
         it != d->progress.spreadOverrides.constEnd(); ++it) {
        if (it.key() >= 0 && it.key() < d->book->pageCount())
            d->book->page(it.key()).override = it.value();
    }

    d->decoder = std::make_unique<DecodeService>(d->book.get(),
                                                 d->config.cacheBudgetBytes);

    // --- views ---
    d->paged = new PagedView;
    d->scroll = new ScrollView;
    const QBrush background(d->config.backgroundColor);
    d->paged->setBackgroundBrush(background);
    d->scroll->setBackgroundBrush(background);

    d->paged->setDirection(d->progress.view.direction);
    d->paged->setFitMode(d->progress.view.fit);
    d->paged->setCoverAlone(d->progress.view.coverAlone);
    d->paged->setPairOffset(d->progress.view.pairOffset);
    d->paged->setDoublePage(d->progress.view.doublePage);
    d->scroll->setDirection(d->progress.view.direction);
    d->scroll->setSnap(d->progress.view.snap);

    d->paged->setBook(d->book.get(), d->decoder.get());
    d->scroll->setBook(d->book.get(), d->decoder.get());

    d->stack = new QStackedWidget;
    d->stack->addWidget(d->paged);
    d->stack->addWidget(d->scroll);
    setCentralWidget(d->stack);

    // --- chrome ---
    d->chrome = new ChromeController(this);
    buildToolBar();
    connect(d->chrome, &ChromeController::chromeVisibilityChanged,
            d->toolbar, &QToolBar::setVisible);

    for (ReaderView* view : {static_cast<ReaderView*>(d->paged),
                             static_cast<ReaderView*>(d->scroll)}) {
        connect(view, &ReaderView::pageChanged,
                this, &MainWindow::onPageChanged);
        view->viewport()->setMouseTracking(true);
        view->viewport()->installEventFilter(this);
    }

    // --- debounced progress save ---
    d->saveTimer.setSingleShot(true);
    d->saveTimer.setInterval(800);
    connect(&d->saveTimer, &QTimer::timeout, this, [this]() {
        d->progress.progress.lastRead = QDateTime::currentDateTimeUtc();
        ProgressFile::save(d->progress);
    });

    // --- initial mode + page ---
    d->stack->setCurrentWidget(d->mode == ReadingMode::Paged
                                   ? static_cast<QWidget*>(d->paged)
                                   : static_cast<QWidget*>(d->scroll));
    updateModeButton();
    activeView()->goToPage(d->progress.progress.page);
    activeView()->setFocus();
    updateChrome(d->progress.progress.page);

    setWindowTitle(QStringLiteral("ur-reader — %1").arg(d->book->title()));
    resize(1000, 1200);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildToolBar()
{
    d->toolbar = addToolBar(QStringLiteral("Reader"));
    d->toolbar->setMovable(false);

    d->pageLabel = new QLabel(QStringLiteral("0 / 0"));
    d->pageLabel->setContentsMargins(8, 0, 8, 0);
    d->toolbar->addWidget(d->pageLabel);

    // The scrubber. It expands to fill the bar, pushing the mode toggle to
    // the right. Inverted for RTL so dragging matches page flow.
    d->slider = new QSlider(Qt::Horizontal);
    d->slider->setRange(0, std::max(0, d->book->pageCount() - 1));
    d->slider->setInvertedAppearance(
        d->progress.view.direction == ReadingDirection::RightToLeft);
    connect(d->slider, &QSlider::valueChanged, this, [this](int value) {
        activeView()->goToPage(value);
    });
    d->toolbar->addWidget(d->slider);

    d->modeButton = new QToolButton;
    d->modeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(d->modeButton, &QToolButton::clicked, this, [this]() {
        setMode(d->mode == ReadingMode::Paged ? ReadingMode::Scroll
                                              : ReadingMode::Paged);
    });
    d->toolbar->addWidget(d->modeButton);
}

void MainWindow::updateModeButton()
{
    d->modeButton->setText(d->mode == ReadingMode::Paged
                               ? QStringLiteral("Mode: Paged")
                               : QStringLiteral("Mode: Scroll"));
}

void MainWindow::updateChrome(int pageIndex)
{
    d->pageLabel->setText(
        QStringLiteral("%1 / %2").arg(pageIndex + 1).arg(d->book->pageCount()));
    const QSignalBlocker block(d->slider);
    d->slider->setValue(pageIndex);
}

ReaderView* MainWindow::activeView() const
{
    return d->mode == ReadingMode::Paged ? static_cast<ReaderView*>(d->paged)
                                         : static_cast<ReaderView*>(d->scroll);
}

void MainWindow::setMode(ReadingMode mode)
{
    if (mode == d->mode)
        return;
    const int page = activeView()->currentPage();   // carry the position over
    d->mode = mode;
    d->progress.view.mode = mode;
    d->stack->setCurrentWidget(mode == ReadingMode::Paged
                                   ? static_cast<QWidget*>(d->paged)
                                   : static_cast<QWidget*>(d->scroll));
    activeView()->goToPage(page);
    activeView()->setFocus();
    updateModeButton();
    updateChrome(page);
    scheduleProgressSave();
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
        d->chrome->setFullscreen(false);
    } else {
        showFullScreen();
        d->chrome->setFullscreen(true);
    }
}

void MainWindow::onPageChanged(int pageIndex)
{
    if (sender() != activeView())
        return;   // ignore the inactive (stacked-but-hidden) view
    d->progress.progress.page = pageIndex;
    if (pageIndex >= d->book->pageCount() - 1)
        d->progress.progress.completed = true;
    updateChrome(pageIndex);
    scheduleProgressSave();
}

void MainWindow::scheduleProgressSave()
{
    d->saveTimer.start();   // (re)start the debounce window
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_F:
        toggleFullscreen();
        return;
    case Qt::Key_Escape:
        if (isFullScreen())
            toggleFullscreen();
        return;
    case Qt::Key_M:
        setMode(d->mode == ReadingMode::Paged ? ReadingMode::Scroll
                                              : ReadingMode::Paged);
        return;
    default:
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    const bool paged = d->mode == ReadingMode::Paged;
    menu.addAction(paged ? QStringLiteral("Switch to scroll mode")
                         : QStringLiteral("Switch to paged mode"),
                   this, [this, paged]() {
                       setMode(paged ? ReadingMode::Scroll : ReadingMode::Paged);
                   });
    menu.addAction(isFullScreen() ? QStringLiteral("Exit fullscreen")
                                  : QStringLiteral("Enter fullscreen"),
                   this, &MainWindow::toggleFullscreen);
    menu.addSeparator();
    menu.addAction(QStringLiteral("Quit"), this, &QWidget::close);
    menu.exec(event->globalPos());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Final flush — the debounce timer may not have fired yet.
    d->progress.progress.lastRead = QDateTime::currentDateTimeUtc();
    ProgressFile::save(d->progress);
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseMove)
        d->chrome->notifyMouseActivity();
    return QMainWindow::eventFilter(watched, event);
}

} // namespace ur
