//===========================================
//  Lumina Desktop source code
//  Copyright (c) 2017-2018, Ken Moore
//  Available under the 3-clause BSD license
//  See the LICENSE file for full details
//===========================================
#include "mainUI.h"
#include "ui_mainUI.h"

#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QPainter>
#include <QScreen>
#include <QSize>
#include <QSplitter>
#include <QtConcurrent>

#include "PrintWidget.h"

MainUI::MainUI() : QMainWindow(), ui(new Ui::MainUI()) {
  ui->setupUi(this);
  // this->setWindowTitle(tr("Lumina PDF Viewer"));
  this->setWindowIcon(QIcon::fromTheme("application-pdf"));
  this->setFocusPolicy(Qt::StrongFocus);
  presentationLabel = 0;
  CurrentPage = 1;
  lastdir = QDir::homePath();
  BACKEND = new Renderer();

  // Create the interface widgets
  PROPDIALOG = new PropDialog(BACKEND);
  BOOKMARKS = new BookmarkMenu(BACKEND, ui->splitter);
  BOOKMARKS->setContextMenuPolicy(Qt::CustomContextMenu);
  BOOKMARKS->setVisible(false);
  WIDGET = new PrintWidget(BACKEND, ui->splitter);
  WIDGET->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->splitter->setCollapsible(0, true);
  ui->splitter->setCollapsible(1, false);
  clockTimer = new QTimer(this);
  clockTimer->setInterval(1000); // 1-second updates to clock
  connect(clockTimer, SIGNAL(timeout()), this, SLOT(updateClock()));
  label_clock = new QLabel(this);
  label_clock->setAlignment(Qt::AlignCenter);
  label_clock->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  label_clock->setStyleSheet(
      "QLabel{color: palette(highlight-text); background-color: "
      "palette(highlight); border-radius: 5px; }");
  label_page = new QLabel(this);
  label_page->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  label_page->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);

  contextMenu = new QMenu(this);
  QObject::connect(contextMenu, SIGNAL(aboutToShow()), this,
                   SLOT(updateContextMenu()));

  QObject::connect(WIDGET, SIGNAL(customContextMenuRequested(const QPoint &)),
                   this, SLOT(showContextMenu(const QPoint &)));
  QObject::connect(WIDGET, SIGNAL(currentPageChanged()), this,
                   SLOT(updatePageNumber()));
  QObject::connect(BACKEND, SIGNAL(PageLoaded(int)), this, SLOT(slotPageLoaded(int)));
  QObject::connect(BACKEND, SIGNAL(SetProgress(int)), this,
                   SLOT(slotSetProgress(int)));
  QObject::connect(BACKEND, SIGNAL(reloadPages(int)), this,
                   SLOT(startLoadingPages(int)));
  QObject::connect(BACKEND, SIGNAL(goToPosition(int, float, float)), WIDGET,
                   SLOT(goToPosition(int, float, float)));
  QObject::connect(ui->splitter, &QSplitter::splitterMoved, this, [=]() {
    double percent = qBound(
        0.0, (ui->splitter->sizes().first() / (double)this->width()) * 100.0,
        33.3);
    ui->splitter->setSizes(QList<int>()
                           << this->width() * (percent / 100.0)
                           << this->width() * ((100.0 - percent) / 100.0));
  });

  PrintDLG = new QPrintDialog(this);
  QObject::connect(PrintDLG, SIGNAL(accepted(QPrinter *)), this,
                   SLOT(paintToPrinter(QPrinter *)));
  // connect(ui->menuStart_Presentation, SIGNAL(triggered(QAction*)), this,
  // SLOT(slotStartPresentation(QAction*)) );

  // Create the other interface widgets
  QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->toolBar->insertWidget(ui->actionPrevious_Page, spacer);
  progress = new QProgressBar(this);
  progress->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  progress->setFormat("%v/%m (%p%)"); // [current]/[total]
  progAct = ui->toolBar->addWidget( progress);
  progAct->setVisible(false);
  clockAct = ui->toolBar->addWidget(label_clock);
  clockAct->setVisible(false);
  pageAct = ui->toolBar->insertWidget(ui->actionNext_Page,label_page);
  pageAct->setVisible(false);

  zoomPercent = new QComboBox(this);
  zoomPercent->setEditable(true);
  zoomPercent->addItem(tr("Fit page"), -2);
  zoomPercent->addItem(tr("Fit width"), -1);
  zoomPercent->addItem(tr("25 %"), 25);
  zoomPercent->addItem(tr("50 %"), 50);
  zoomPercent->addItem(tr("75 %"), 75);
  zoomPercent->addItem(tr("100 %"), 100);
  zoomPercent->addItem(tr("150 %"), 150);
  zoomPercent->addItem(tr("200 %"), 200);
  zoomPercent->addItem(tr("300 %"), 300);
  zoomPercent->addItem(tr("400 %"), 400);
  zoomPercent->addItem(tr("500 %"), 500);
  zoomPercent->setCurrentIndex(0);
  zoomPercent->setInsertPolicy(QComboBox::NoInsert);
  zoomPercent->setInputMethodHints(Qt::ImhDigitsOnly);
  ui->toolBar->insertWidget(ui->actionZoom_In_2, zoomPercent);

  // Put the various actions into logical groups
  QActionGroup *tmp = new QActionGroup(this);
  tmp->setExclusive(true);
  tmp->addAction(ui->actionSingle_Page);
  tmp->addAction(ui->actionDual_Pages);
  //tmp->addAction(ui->actionAll_Pages);
  ui->actionSingle_Page->setChecked(true);

  //Disable the all pages view - does not work with partial cache of pages
  ui->actionAll_Pages->setVisible(false);
  ui->actionAll_Pages->setEnabled(false);

  // Connect up the buttons
  QObject::connect(ui->actionClose, SIGNAL(triggered()), this, SLOT(close()));
  QObject::connect(ui->actionPrint, SIGNAL(triggered()), PrintDLG,
                   SLOT(open()));
  QObject::connect(ui->actionOpen_PDF, SIGNAL(triggered()), this,
                   SLOT(OpenNewFile()));
  QObject::connect(ui->actionSingle_Page, SIGNAL(triggered()), WIDGET,
                   SLOT(setSinglePageViewMode()));
  QObject::connect(ui->actionDual_Pages, SIGNAL(triggered()), WIDGET,
                   SLOT(setFacingPagesViewMode()));
  QObject::connect(ui->actionAll_Pages, SIGNAL(triggered()), WIDGET,
                   SLOT(setAllPagesViewMode()));
  // connect(ui->actionScroll_Mode,  &QAction::triggered, this, [&] {
  // this->setScroll(true); }); connect(ui->actionSelect_Mode,
  // &QAction::triggered, this, [&] { this->setScroll(false); });
  QObject::connect(ui->actionZoom_In, &QAction::triggered, WIDGET,
                   [&] { WIDGET->zoomIn(1.2); });
  QObject::connect(ui->actionZoom_Out, &QAction::triggered, WIDGET,
                   [&] { WIDGET->zoomOut(1.2); });
  QObject::connect(ui->actionRotate_Counterclockwise, &QAction::triggered, this,
                   [&] {
                     if (results.size() != 0) {
                       foreach (TextData *x, results) { x->highlighted(false); }
                     }
                     BACKEND->setDegrees(-90);
                   });
  QObject::connect(ui->actionRotate_Clockwise, &QAction::triggered, this, [&] {
    if (results.size() != 0) {
      foreach (TextData *x, results) { x->highlighted(false); }
    }
    BACKEND->setDegrees(90);
  });
  QObject::connect(ui->actionZoom_In_2, SIGNAL(triggered()), this,
                   SLOT(zoomIn()));
  QObject::connect(ui->actionZoom_Out_2, SIGNAL(triggered()), this,
                   SLOT(zoomOut()));
  QObject::connect(ui->actionFirst_Page, SIGNAL(triggered()), this,
                   SLOT(firstPage()));
  QObject::connect(ui->actionPrevious_Page, SIGNAL(triggered()), this,
                   SLOT(prevPage()));
  QObject::connect(ui->actionNext_Page, SIGNAL(triggered()), this,
                   SLOT(nextPage()));
  QObject::connect(ui->actionLast_Page, SIGNAL(triggered()), this,
                   SLOT(lastPage()));
  QObject::connect(ui->actionGoTo_Page, SIGNAL(triggered()), this,
                   SLOT(gotoPage()));
  QObject::connect(zoomPercent, qOverload<int>(&QComboBox::currentIndexChanged),
                   [=]() { onZoomPageIndexChanged(); });
  QObject::connect(zoomPercent->lineEdit(), SIGNAL(returnPressed()), this,
                  SLOT(onZoomPageTextChanged()));
  QObject::connect(ui->actionProperties, &QAction::triggered, WIDGET,
                   [&] { PROPDIALOG->show(); });
  QObject::connect(BACKEND, &Renderer::OrigSize, this,
                   [&](QSizeF _pageSize) { pageSize = _pageSize; });
  QObject::connect(ui->actionFind, &QAction::triggered, this, [&] {
    if (ui->findGroup->isVisible()) {
      ui->findGroup->setVisible(false);
      this->setFocus();
    } else {
      ui->findGroup->setVisible(true);
      ui->findGroup->setFocus();
    }
  });
  QObject::connect(ui->actionFind_Next, &QAction::triggered, this,
                   [&] { find(ui->textEdit->text(), true); });
  QObject::connect(ui->actionFind_Previous, &QAction::triggered, this,
                   [&] { find(ui->textEdit->text(), false); });
  QObject::connect(ui->findNextB, &QPushButton::clicked, this,
                   [&] { find(ui->textEdit->text(), true); });
  QObject::connect(ui->findPrevB, &QPushButton::clicked, this,
                   [&] { find(ui->textEdit->text(), false); });
  QObject::connect(ui->matchCase, &QPushButton::clicked, this,
                   [&](bool value) { this->matchCase = value; });
  QObject::connect(ui->closeFind, &QPushButton::clicked, this, [&] {
    ui->findGroup->setVisible(false);
    this->setFocus();
  });
  QObject::connect(ui->actionClearHighlights, &QAction::triggered, WIDGET,
                   [&] { WIDGET->updatePreview(); });
  QObject::connect(ui->actionBookmarks, &QAction::triggered, this, [=]() {
    ui->splitter->setSizes(QList<int>()
                           << this->width() / 4 << 3 * this->width() / 4);
  });

  // int curP = WIDGET->currentPage()-1; //currentPage reports pages starting at
  // 1 int lastP = numPages-1;
  ui->actionFirst_Page->setText(tr("First Page"));
  ui->actionPrevious_Page->setText(tr("Previous Page"));
  ui->actionNext_Page->setText(tr("Next Page"));
  ui->actionLast_Page->setText(tr("Last Page"));
  /*ui->actionFirst_Page->setEnabled(curP!=0);
  ui->actionPrevious_Page->setEnabled(curP>0);
  ui->actionNext_Page->setEnabled(curP<lastP);
  ui->actionLast_Page->setEnabled(curP!=lastP);*/

  ui->actionStart_Here->setText(tr("Start Presentation (current slide)"));
  QObject::connect(ui->actionStart_Here, SIGNAL(triggered()), this,
                   SLOT(startPresentationHere()));
  ui->actionStart_Begin->setText(tr("Start Presentation (at beginning)"));
  QObject::connect(ui->actionStart_Begin, SIGNAL(triggered()), this,
                   SLOT(startPresentationBeginning()));
  ui->actionStop_Presentation->setText(tr("Stop Presentation"));
  QObject::connect(ui->actionStop_Presentation, SIGNAL(triggered()), this,
                   SLOT(closePresentation()));

  // Setup all the icons
  ui->actionPrint->setIcon(QIcon::fromTheme("document-print"));
  ui->actionClose->setIcon(QIcon::fromTheme("window-close"));
  ui->actionOpen_PDF->setIcon(QIcon::fromTheme("document-open"));
  ui->actionSingle_Page->setIcon( QIcon::fromTheme("view-split-top-bottom", QIcon::fromTheme("format-view-agenda") ));
  ui->actionDual_Pages->setIcon( QIcon::fromTheme("view-split-left-right", QIcon::fromTheme("format-view-grid-small") ));
  ui->actionAll_Pages->setIcon( QIcon::fromTheme("view-grid", QIcon::fromTheme("format-view-grid-large") ));
  ui->actionScroll_Mode->setIcon(QIcon::fromTheme("cursor-pointer"));
  ui->actionSelect_Mode->setIcon(QIcon::fromTheme("cursor-text"));
  ui->actionZoom_In->setIcon(QIcon::fromTheme("zoom-in"));
  ui->actionZoom_Out->setIcon(QIcon::fromTheme("zoom-out"));
  ui->actionZoom_In_2->setIcon(QIcon::fromTheme("zoom-in"));
  ui->actionZoom_Out_2->setIcon(QIcon::fromTheme("zoom-out"));
  ui->actionRotate_Counterclockwise->setIcon( QIcon::fromTheme("object-rotate-left"));
  ui->actionRotate_Clockwise->setIcon( QIcon::fromTheme("object-rotate-right"));
  ui->actionFirst_Page->setIcon(QIcon::fromTheme("go-first"));
  ui->actionPrevious_Page->setIcon(QIcon::fromTheme("go-previous"));
  ui->actionNext_Page->setIcon(QIcon::fromTheme("go-next"));
  ui->actionLast_Page->setIcon(QIcon::fromTheme("go-last"));
  ui->actionStart_Here->setIcon( QIcon::fromTheme("video-display", QIcon::fromTheme("media-playback-start-circled") ));
  ui->actionStart_Begin->setIcon( QIcon::fromTheme("view-presentation", QIcon::fromTheme("presentation-play") ));
  ui->actionStop_Presentation->setIcon( QIcon::fromTheme("media-playback-stop", QIcon::fromTheme("media-playback-stop-circled") ));
  ui->actionBookmarks->setIcon(QIcon::fromTheme("bookmark-new"));
  ui->actionFind->setIcon(QIcon::fromTheme("edit-find"));
  ui->actionFind_Next->setIcon( QIcon::fromTheme("go-down-search", QIcon::fromTheme("edit-find-next") ));
  ui->actionFind_Previous->setIcon( QIcon::fromTheme("go-up-search", QIcon::fromTheme("edit-find-prev")));
  ui->actionProperties->setIcon(QIcon::fromTheme("dialog-information"));
  ui->actionSettings->setIcon(QIcon::fromTheme("document-properties"));
  ui->findNextB->setIcon(QIcon::fromTheme("go-down-search"));
  ui->findPrevB->setIcon(QIcon::fromTheme("go-up-search"));
  ui->actionClearHighlights->setIcon(QIcon::fromTheme("format-text-clear"));
  ui->findNextB->setIcon(QIcon::fromTheme("go-down-search"));
  ui->matchCase->setIcon(QIcon::fromTheme("format-text-italic"));
  ui->closeFind->setIcon(QIcon::fromTheme("dialog-close"));

  // Now set the default state of the menu's and actions
  ui->actionStop_Presentation->setEnabled(false);
  ui->actionStart_Here->setEnabled(false);
  ui->actionStart_Begin->setEnabled(false);

  ui->findGroup->setVisible(false);

  // TESTING features/functionality
  bool TESTING = BACKEND->supportsExtraFeatures();
  ui->actionBookmarks->setEnabled(TESTING);
  ui->actionBookmarks->setVisible(TESTING);
  ui->actionProperties->setEnabled(TESTING);
  ui->actionProperties->setVisible(TESTING);
  ui->actionClearHighlights->setEnabled(TESTING);
  ui->actionClearHighlights->setVisible(TESTING);

  ui->actionSettings->setEnabled(false);
  ui->actionSettings->setVisible(false);
  ui->actionScroll_Mode->setEnabled(false);
  ui->actionScroll_Mode->setVisible(false);
  ui->actionSelect_Mode->setEnabled(false);
  ui->actionSelect_Mode->setVisible(false);
  //qDebug() << "Done with MainUI Init";
}

MainUI::~MainUI() {
  if (BOOKMARKS != 0) {
    BOOKMARKS->deleteLater();
  }
  if (PROPDIALOG != 0) {
    PROPDIALOG->deleteLater();
  }
  delete BACKEND;
}

void MainUI::loadFile(QString path) {
  if (!QFile::exists(path) || path.isEmpty()) {
    return;
  }
  QString password;
  bool ok = true;

  while (ok && !BACKEND->loadDocument(path, password) &&
         BACKEND->needPassword()) {
    password = QInputDialog::getText(this, tr("Unlock PDF"), tr("Password:"),
                                     QLineEdit::Password, "", &ok);
    if (!ok) {
      break;
    } // cancelled
  }
  // Clear the current display

  WIDGET->setVisible(false);
  BOOKMARKS->setVisible(false);
  // Load the new document info
  this->setWindowTitle(BACKEND->title());
  if (BACKEND->needPassword()) {
    return;
  } // cancelled;
  qDebug() << " - Document Setup : start loading pages now";

  // Populate or repopulate the Bookmarks menu and Properties Dialog
  QTimer::singleShot(10, BOOKMARKS, SLOT(loadBookmarks()));
  QTimer::singleShot(10, PROPDIALOG, SLOT(setInformation()));
  QTimer::singleShot(50, [&]() { startLoadingPages(0); });
}

QScreen *MainUI::getScreen(bool current, bool &cancelled) {
  // Note: the "cancelled" boolian is actually an output - not an input
  QList<QScreen *> screens = QApplication::screens();
  cancelled = false;
  if (screens.length() == 1) {
    return screens[0];
  } // only one option
  // Multiple screens available - figure it out
  if (current) {
    // Just return the screen the window is currently on
    for (int i = 0; i < screens.length(); i++) {
      if (screens[i]->geometry().contains(this->mapToGlobal(this->pos()))) {
        return screens[i];
      }
    }
    // If it gets this far, there was an error and it should just return the
    // primary screen
    return QApplication::primaryScreen();
  } else {
    // Ask the user to select a screen (for presentations, etc..)
    QStringList names;
    for (int i = 0; i < screens.length(); i++) {
      QString screensize = QString::number(screens[i]->size().width()) + "x" +
                           QString::number(screens[i]->size().height());
      names << QString(tr("%1 (%2)")).arg(screens[i]->name(), screensize);
    }
    bool ok = false;
    QString sel = QInputDialog::getItem(this, tr("Select Screen"),
                                        tr("Screen:"), names, 0, false, &ok);
    cancelled = !ok;
    if (!ok) {
      return screens[0];
    } // cancelled - just return the first one
    int index = names.indexOf(sel);
    if (index < 0) {
      return screens[0];
    }                      // error - should never happen though
    return screens[index]; // return the selected screen
  }
}

void MainUI::startPresentation(bool atStart) {
  if(presentationActive()){ return; } //already running
  if (BACKEND->hashSize() == 0) {
    qDebug() << "MainUI::startPresentation() called while backend is empty!\n";
    return;
  } // just in case
  bool cancelled = false;
  QScreen *screen = getScreen(
      false,
      cancelled); // let the user select which screen to use (if multiples)
  if (cancelled) {
    return;
  }
  int page = 1;
  if (!atStart) {
    page = WIDGET->currentPage();
  }
  // PDPI = QSize(SCALEFACTOR*screen->physicalDotsPerInchX(),
  // SCALEFACTOR*screen->physicalDotsPerInchY()); Now create the full-screen
  // window on the selected screen
  if (presentationLabel == 0) {
    // Create the label and any special flags for it
    presentationLabel = new PresentationLabel();
    presentationLabel->setStyleSheet("background-color: black;");
    presentationLabel->setAlignment(Qt::AlignCenter);
    presentationLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(presentationLabel,
            SIGNAL(customContextMenuRequested(const QPoint &)), this,
            SLOT(showContextMenu(const QPoint &)));
    connect(presentationLabel, SIGNAL(nextSlide()), this, SLOT(nextPage()));
  }
  // Now put the label in the proper location
  presentationLabel->setGeometry(screen->geometry());
  presentationLabel->showFullScreen();

  ui->actionStop_Presentation->setEnabled(true);
  ui->actionStart_Here->setEnabled(false);
  ui->actionStart_Begin->setEnabled(false);
  updateClock();
  updatePageNumber();
  clockAct->setVisible(true);
  clockTimer->start();
  QApplication::processEvents();
  // Now start at the proper page
  ShowPage(page);
  this->grabKeyboard(); // Grab any keyboard events - even from the presentation
                        // window
}

void MainUI::ShowPage(int page) {
  // Check for valid document/page
  //qDebug() << "Load Page:" << page << "/" << BACKEND->numPages() << "Index:" << page;
  //page = std::max(1, std::min(page, BACKEND->numPages()));
  if (page == BACKEND->numPages()+2 ) { //allow one blank/black page after the end of the last slide
    endPresentation();
    return; // invalid - no document loaded or invalid page specified
  }
  WIDGET->setCurrentPage(page); // page numbers start at 1 for this widget
  // Stop here if no presentation currently running

  if (!presentationActive()) {
    WIDGET->updatePreview();
    return;
  }
  //qDebug() << "Show Page:" << page << "/" << BACKEND->numPages();
  CurrentPage = page;
  QImage PAGEIMAGE;
  if (page < BACKEND->numPages() + 1) {
    PAGEIMAGE = BACKEND->imageHash(page);
  }

  // Now scale the image according to the user-designations and show it
  if (!PAGEIMAGE.isNull()) {
    QPixmap pix;
    pix.convertFromImage(PAGEIMAGE.scaled(presentationLabel->size(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
    presentationLabel->setPixmap(pix);
    presentationLabel->show(); // always make sure it was not hidden
  } else {
    // Blank page (useful so there is one blank page after the last slide before
    // stopping the presentation)
    presentationLabel->setPixmap(QPixmap());
    if(CurrentPage>BACKEND->numPages() || CurrentPage <1){
      presentationLabel->setText(tr("Presentation Finished: Hit \"ESC\" key to close presentation view"));
    }else{
      presentationLabel->setText(tr("Loading Page...."));
    }
    presentationLabel->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
  }
}

void MainUI::endPresentation() {
  if (!presentationActive()) { return; } // not in presentation mode
  presentationLabel->hide(); // just hide this (no need to re-create the label for future presentations)
  ui->actionStop_Presentation->setEnabled(false);
  ui->actionStart_Here->setEnabled(true);
  ui->actionStart_Begin->setEnabled(true);
  clockTimer->stop();
  clockAct->setVisible(false);
  this->releaseKeyboard();
  if(WIDGET->currentPage() >= BACKEND->numPages()){
    lastPage();
  }
  updatePageNumber();
}

bool MainUI::presentationActive(){
  if(presentationLabel == 0 ) { return false; }
  else { return presentationLabel->isVisible(); }
}

void MainUI::startLoadingPages(int degrees) {
  //qDebug() <<"Start Loading Pages";
  // if(BACKEND->hashSize() != 0) { return; } //currently loaded[ing]
  loadingQueue.clear();
  BACKEND->clearHash();
  WIDGET->setVisible(false);
  BOOKMARKS->setVisible(false);
  progress->setRange(0, BACKEND->numPages());
  progress->setValue(0);
  progAct->setVisible(true);
  pageAct->setVisible(false);
  // PERFORMANCE NOTES:
  // Using Poppler to scale the image (adjust dpi value) helps a bit but you
  // take a larger CPU loading hit (and still quite a lot of pixelization) Using
  // Qt to scale the image (adjust page value) smooths out the image quite a bit
  // without a lot of performance loss (but cannot scale up without
  // pixelization) The best approach seams to be to increase the DPI a bit, but
  // match that with the same scaling on the page size (smoothing)

  //QSize DPI(250, 250); // print-quality (some printers even go to 600 DPI nowdays)


  int curpage = WIDGET->currentPage();
  if(curpage<0){ curpage = 1; }
  else if(curpage>=BACKEND->numPages()){ curpage=BACKEND->numPages(); }
  WIDGET->setCurrentPage(curpage);

  /*qDebug() << "Screen Resolutions:";
  QList<QScreen*> screens = QApplication::screens();
  for(int i=0; i<screens.length(); i++){
    qDebug() << screens[i]->name() << screens[i]->logicalDotsPerInchX() <<
  screens[i]->logicalDotsPerInchY();
  }*/
  /*int n = BACKEND->numPages() + 1;
  for (int i = 1; i < n; i++) {
    // qDebug() << " - Kickoff page load:" << i;
    if (BACKEND->loadMultiThread()) {
      QtConcurrent::run(BACKEND, &Renderer::renderPage, i, DPI, degrees);
    } else {
      BACKEND->renderPage(i, DPI, degrees);
      if (i % 50 == 0) {
        QCoreApplication::processEvents();
      }
    }
  }*/
  //qDebug() << "Finish page loading kickoff";
}

void MainUI::slotSetProgress(int finished) { progress->setValue(finished); }

void MainUI::slotPageLoaded(int page) {
  int curpage = WIDGET->currentPage();
  if(curpage == page || (curpage <0 && page==1) ){
    //current page loaded

  //qDebug() << "slotPageLoaded";
  /*loadingQueue.push_back(page);
  int finished = loadingQueue.size();*/
  //qDebug() << "Page Loaded:" << page;
  //if (finished == BACKEND->numPages()) {
    progAct->setVisible(false);
    WIDGET->setVisible(true);
    BOOKMARKS->setVisible(true);
    if(BACKEND->getBookmarks().isEmpty()){
      ui->splitter->setSizes(QList<int>() << 0 << this->width());
    }
    ui->actionStop_Presentation->setEnabled(false);
    ui->actionStart_Here->setEnabled(true);
    ui->actionStart_Begin->setEnabled(true);
    pageAct->setVisible(true);
    PROPDIALOG->setSize(pageSize);
    //qDebug() << " - Document Setup: All pages loaded";
    ShowPage(page);
  }
  // QTimer::singleShot(10, WIDGET,
  //                   SLOT(updatePreview())); // start loading the file preview
  WIDGET->updatePreview();
}

void MainUI::paintToPrinter(QPrinter *PRINTER) {
  //qDebug() << "paintToPrinter()";
  /*if (BACKEND->hashSize() != BACKEND->numPages()) {
    //qDebug() << " - Page size mismatch";
    return;
  }*/
  // qDebug() << "paintToPrinter";
  int pages = BACKEND->numPages();
  int firstpage = 0;
  int copies = PRINTER->copyCount();
  bool collate = PRINTER->collateCopies();
  bool reverse = (PRINTER->pageOrder() == QPrinter::LastPageFirst);
  // qDebug() << "PRINTER DPI:" << PRINTER->resolution() <<
  // PRINTER->supportedResolutions();
  if (PRINTER->resolution() < 300) {
    // Try to get 300 DPI resolution at least
    PRINTER->setResolution(300);
    qDebug() << "Trying to change print resolution to 300 minimum";
    qDebug() << "  -- Resolutions listed as supported:"
             << PRINTER->supportedResolutions();
  }
  QSize PDPI(PRINTER->resolution(), PRINTER->resolution());

  // bool duplex = (PRINTER->duplex()!=QPrinter::DuplexNone);
  // Determine the first page that needs to be printed, and the range
  if ((PRINTER->fromPage() != PRINTER->toPage() || PRINTER->fromPage() != 0) &&
      PRINTER->printRange() == QPrinter::PageRange) {
    firstpage = PRINTER->fromPage() - 1;
    pages = PRINTER->toPage();
  }
  qDebug() << "Start Printing PDF: Pages" << PRINTER->fromPage() << " to "
           << PRINTER->toPage() << " Copies:" << copies
           << "  collate:" << collate << " Reverse Order:" << reverse;
  QList<int> pageCount;
  // Assemble the page order/count based on printer settings
  for (int i = firstpage; i < pages; i++) {
    // Make sure even/odd pages are not selected as desired
    // Qt 5.7.1 does not seem to have even/odd page selections - 8/11/2017
    pageCount << i; // add this page to the list
    // QT 5.9+ : Do not need to manually stack "copies". Already handled
    // internally for(int c=1; c<copies && !collate; c++){ pageCount << i; }
    // //add any copies of this page as needed
  }
  // qDebug() << "Got Page Range:" << pageCount;
  // QT 5.9+ : Do not need to manually reverse the pages (already handled
  // internally)
  if (reverse) {
    // Need to reverse the order of the list
    QList<int> tmp = pageCount;
    pageCount.clear();
    for (int i = tmp.length() - 1; i >= 0; i--) {
      pageCount << tmp[i];
    }
    // qDebug() << " - reversed:" << pageCount;
  }
  // QT 5.9+ : Do not need to manually stack "copies". Already handled
  // internally;
  /*if(collate && copies>0){
    QList<int> orig = pageCount; //original array of pages
    for(int c=1; c<copies; c++){
      pageCount << orig; //add a new copy of the entire page range
    }
  }*/
  // qDebug() << "Final Page Range:" << pageCount;
  // Generate the sizing information for the printer
  QSize sz(PRINTER->pageRect().width(), PRINTER->pageRect().height());
  bool landscape = PRINTER->orientation() == QPrinter::Landscape;
  if (landscape) {
    sz = QSize(sz.height(), sz.width());
  } // flip the size dimensions as needed
  // Now send out the pages in the right order/format
  QPainter painter(PRINTER);
  // Ensure all the antialiasing/smoothing options are turned on
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  QTransform transF;
  transF.rotate(90);
  // Show the progress bar
  progAct->setVisible(true);
  progress->setRange(0, pageCount.length() - 1);
  for (int i = 0; i < pageCount.length(); i++) {
    if (i != 0) {
      PRINTER->newPage();
    }
    // qDebug() << "Printing Page:" << pageCount[i];
    progress->setValue(i);
    QApplication::processEvents();
    BACKEND->renderPage(pageCount[i]+1, PDPI); //backend starts with page 1, not zero
    QImage img = BACKEND->imageHash(pageCount[i]+1)
                     .scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // Now draw the image
    painter.drawImage(0, 0, img);
    // Also paint the annotations at their locations
    for (int k = 0; k < BACKEND->annotSize(i); k++) {
      Annotation *annot = BACKEND->annotList(i, k);
      if (annot->print()) {
        if (annot->getType() == 14) {
          painter.setPen(QPen(annot->getColor()));
          foreach (QVector<QPointF> pointList, annot->getInkList())
            painter.drawLines(pointList);
        } else {
          painter.drawImage(annot->getLoc(), annot->renderImage());
        }
      }
    }
  }
  progAct->setVisible(false);
}

void MainUI::OpenNewFile() {
  // Prompt for a file
  QString path = QFileDialog::getOpenFileName(this, tr("Open PDF"), lastdir,
                                              tr("PDF Documents (*.pdf)"));
  // Now Open it
  if (!path.isEmpty()) {
    loadFile(path);
  }
}

void MainUI::updateClock() {
  label_clock->setText("<b>"+
      QDateTime::currentDateTime().toString( Qt::DefaultLocaleShortDate)+"</b>");
}

void MainUI::updatePageNumber() {
  // qDebug() << "UpdatePageNumber";
  QString text;
  if (presentationLabel == 0 || !presentationLabel->isVisible()) {
    text = tr("Page %1 of %2");
  } else {
    text = "%1/%2";
  }
  label_page->setText(text.arg(QString::number(WIDGET->currentPage()),
                               QString::number(BACKEND->numPages())));
}

/*void MainUI::setScroll(bool tog) {
  if(tog) {
    QApplication::setOverrideCursor(Qt::OpenHandCursor);
  }else{
    QApplication::setOverrideCursor(Qt::IBeamCursor);
  }
}*/

void MainUI::updateContextMenu() {
  contextMenu->clear();
  contextMenu->addSection(QString(tr("Page %1 of %2"))
                              .arg(QString::number(WIDGET->currentPage()),
                                   QString::number(BACKEND->numPages())));
  contextMenu->addAction(ui->actionPrevious_Page);
  contextMenu->addAction(ui->actionNext_Page);
  contextMenu->addSeparator();
  contextMenu->addAction(ui->actionFirst_Page);
  contextMenu->addAction(ui->actionLast_Page);
  contextMenu->addSeparator();
  if (presentationLabel == 0 || !presentationLabel->isVisible()) {
    contextMenu->addAction(ui->actionStart_Begin);
    contextMenu->addAction(ui->actionStart_Here);
  } else {
    contextMenu->addAction(ui->actionStop_Presentation);
  }
}

void MainUI::keyPressEvent(QKeyEvent *event) {
  //qDebug() << "Got Key Press Event!";
  // See if this is one of the special hotkeys and act appropriately
  bool inPresentation = (presentationLabel != 0);
  switch (event->key()) {
  case Qt::Key_Escape:
  case Qt::Key_Backspace: {
    if (inPresentation)
      endPresentation();
  } break;
  case Qt::Key_Right:
  case Qt::Key_Space:
  case Qt::Key_PageDown: {
    nextPage();
  } break;
  case Qt::Key_Left:
  case Qt::Key_PageUp: {
    prevPage();
  } break;
  case Qt::Key_Home: {
    firstPage();
  } break;
  case Qt::Key_End: {
    lastPage();
  } break;
  case Qt::Key_F11: {
    if (inPresentation) {
      endPresentation();
    } else {
      startPresentationHere();
    }
  } break;
  case Qt::Key_Up: {
    // Scroll the widget up
  } break;
  case Qt::Key_Down: {
    // Scroll the widget down
    /*qDebug() << "Send Wheel Event";
    QWheelEvent wEvent( WIDGET->mapFromGlobal(QCursor::pos()),
    QCursor::pos(),QPoint(0,0), QPoint(0,30), 0, Qt::Vertical, Qt::LeftButton,
    Qt::NoModifier); QApplication::sendEvent(WIDGET, &wEvent);*/
  } break;
  case Qt::Key_Enter: {
    /*if(ui->findGroup->hasFocus()) {
      find(ui->textEdit->text(), true);
    }*/
  } break;
  default: { QMainWindow::keyPressEvent(event); } break;
  }

  return;
}

void MainUI::wheelEvent(QWheelEvent *event) {
  // Scroll the window according to the mouse wheel
  //qDebug() << "Got Wheel Event!";
  QMainWindow::wheelEvent(event);
}

// Simplification routines
void MainUI::nextPage() {
  int next = WIDGET->currentPage() + 1;
  if(presentationActive() || next <= BACKEND->numPages()){
    ShowPage(next);
  }
} // currentPage() starts at 1 rather than 0

void MainUI::prevPage() {
  int next = WIDGET->currentPage() - 1;
  if(presentationActive() || next >= 1){
    ShowPage(next);
  }
}

void MainUI::gotoPage() {
  bool ok;
  int page = QInputDialog::getInt(this, tr("Go to page"),
                                         tr("Page number:"), 1, 1, BACKEND->numPages(), 1, &ok);
    if (ok)
        ShowPage(page);
}

void MainUI::zoomIn() {
  if (zoomPercent->findText( zoomPercent->currentText() ) == -1 ) {
      double percentage = zoomPercent->currentText().remove('%').toDouble() * 1.2 / 100;
      WIDGET->fitView();
      WIDGET->zoomIn( percentage );
      zoomPercent->lineEdit()->setText(QString::number(percentage * 100.0, 'f', 2) + QString(" %"));
  }
  else {
    switch( zoomPercent->currentData().toInt() ) {
    case -2:
    case -1:
      WIDGET->zoomIn( 1.2 );
      zoomPercent->lineEdit()->setText(QString("120 %"));
      break;
    default:
      double percentage = zoomPercent->currentData().toInt() * 1.2 / 100;
      WIDGET->fitView();
      WIDGET->zoomIn( percentage );
      zoomPercent->lineEdit()->setText(QString::number(percentage * 100.0, 'f', 2) + QString(" %"));
      break;
    }
  }
}

void MainUI::zoomOut() {
  if (zoomPercent->findText( zoomPercent->currentText() ) == -1 ) {
      double percentage = zoomPercent->currentText().remove('%').toDouble() / 1.2 / 100;
      WIDGET->fitView();
      WIDGET->zoomIn( percentage );
      zoomPercent->lineEdit()->setText(QString::number(percentage * 100.0, 'f', 2) + QString(" %"));
  }
  else {
    switch( zoomPercent->currentData().toInt() ) {
    case -2:
    case -1:
      WIDGET->zoomIn( 1.2 );
      zoomPercent->lineEdit()->setText(QString("120 %"));
      break;
    default:
      double percentage = zoomPercent->currentData().toInt() / 1.2 / 100;
      WIDGET->fitView();
      WIDGET->zoomIn( percentage );
      zoomPercent->lineEdit()->setText(QString::number(percentage * 100.0, 'f', 2) + QString(" %"));
      break;
    }
  }
}

void MainUI::onZoomPageIndexChanged() {
  int percentage = zoomPercent->currentData().toInt();
  switch (zoomPercent->currentData().toInt())
  {
    case -2:
      WIDGET->fitView();
      break;
    case -1:
      WIDGET->fitToWidth();
      break;
    default:
      WIDGET->fitView();
      WIDGET->zoomIn(percentage / 100.0);
      break;
  }
}

void MainUI::onZoomPageTextChanged() {
   int percentage =  zoomPercent->currentText().remove('%').toInt() ;
   if (percentage > 0 ) {
    WIDGET->fitView();
    WIDGET->zoomIn(percentage / 100.0);
   }
}
void MainUI::find(QString text, bool forward) {
  if (!text.isEmpty()) {
    static bool previousMatchCase = matchCase;
    // qDebug() << "Finding Text";
    // Detemine if it is the first time searching or a new search string
    bool newSearch = results.empty() || !(results[0]->text() == text);

    // Also search again if match case is turned on/off
    if (previousMatchCase != matchCase) {
      newSearch = true;
      previousMatchCase = matchCase;
    }

    // Clear results and highlights if the user gives a new search string
    if (newSearch) {
      // clear out the old results
      if (!results.empty()) {
        foreach (TextData *td, results)
          delete td;
        results.clear();
      }
      WIDGET->updatePreview();
      ui->resultsLabel->setText("");
      // Get the new search results
      results = BACKEND->searchDocument(text, matchCase);
      // qDebug() << "Total Results: " << results.size();
      currentHighlight = (forward) ? -1 : results.size();
    }

    // qDebug() << "Jumping to next result";
    if (!results.empty()) {
      // Jump to the location of the next or previous textbox and highlight
      if (forward) {
        currentHighlight++;
        if (currentHighlight >= results.size())
          currentHighlight %= results.size();
      } else {
        currentHighlight--;
        // Ensure currentHighlight will be between 0 and results.size() - 1
        if (currentHighlight < 0)
          currentHighlight = results.size() - 1;
      }

      ui->resultsLabel->setText(QString::number(currentHighlight + 1) + " of " +
                                QString::number(results.size()) + " results");

      TextData *currentText = results[currentHighlight];

      if (BACKEND->supportsExtraFeatures()) {
        WIDGET->highlightText(currentText);
      } else {
        ui->resultsLabel->setText("No results found");
      }
    }
  }
}
