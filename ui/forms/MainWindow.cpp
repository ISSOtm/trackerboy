
#include "MainWindow.hpp"
#include "Tileset.hpp"
#include "widgets/docks/ModulePropertiesWidget.hpp"
#include "widgets/docks/OrderWidget.hpp"
#include "widgets/docks/SongPropertiesWidget.hpp"
#include "widgets/docks/SongWidget.hpp"
#include "widgets/docks/TableForm.hpp"
#include "widgets/PatternEditor.hpp"

#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QScreen>

#include <array>

#pragma warning(push, 0)
#include "ui_MainWindow.h"
#pragma warning(pop)

constexpr int TOOLBAR_ICON_WIDTH = 16;
constexpr int TOOLBAR_ICON_HEIGHT = 16;

MainWindow::MainWindow(Trackerboy &trackerboy) :
    QMainWindow(),
    mUi(new Ui::MainWindow()),
    mModuleFileDialog(new QFileDialog(this)),
    mApp(trackerboy),
    mWaveEditor(new WaveEditor(mApp.waveModel, this)),
    mInstrumentEditor(new InstrumentEditor(mApp.instrumentModel, mApp.waveModel, *mWaveEditor, this)),
    mConfigDialog(new ConfigDialog(mApp.config, this))
{
    // setup the designer ui
    mUi->setupUi(this);

    // setup ui not specified in the ui file
    setupUi();

    setupConnections();

    // read in configuration, window geometry and window state
    readSettings();
    mApp.config.readSettings();
    mConfigDialog->resetControls();



    // new documents have an empty string for a filename
    setFilename("");
    
    // associate menu actions with the model
    mApp.songModel.setActions(mUi->actionNewSong, mUi->actionRemoveSong, nullptr, nullptr);

    QIcon appIcon;
    appIcon.addFile(":/icons/app/appicon-16.png");
    appIcon.addFile(":/icons/app/appicon-24.png");
    appIcon.addFile(":/icons/app/appicon-32.png");
    appIcon.addFile(":/icons/app/appicon-48.png");
    appIcon.addFile(":/icons/app/appicon-256.png");
    setWindowIcon(appIcon);

    //mRenderer->setDevice(mConfig.device(), mConfig.samplerate());
    //mRenderer->start();
}

MainWindow::~MainWindow() {
    delete mUi;
}

void MainWindow::closeEvent(QCloseEvent *evt) {
    if (maybeSave()) {
        writeSettings();
        mApp.config.writeSettings();
        evt->accept();
    } else {
        evt->ignore();
    }
}

// SLOTS ---------------------------------------------------------------------

void MainWindow::updateWindowTitle() {
    setWindowTitle(QString("%1[*] - Trackerboy").arg(mDocumentName));
}

// action slots

void MainWindow::fileNew() {
    if (maybeSave()) {
        
        setModelsEnabled(false);
        mApp.document.clear();
        setModelsEnabled(true);

        setFilename("");

    }
}

void MainWindow::fileOpen() {
    if (maybeSave()) {
        mModuleFileDialog->setFileMode(QFileDialog::FileMode::ExistingFile);
        mModuleFileDialog->setAcceptMode(QFileDialog::AcceptOpen);
        mModuleFileDialog->setWindowTitle("Open");
        if (mModuleFileDialog->exec() == QDialog::Accepted) {
            QString filename = mModuleFileDialog->selectedFiles().first();

            // disable models
            setModelsEnabled(false);

            auto error = mApp.document.open(filename);

            // renable models
            setModelsEnabled(true);

            if (error == trackerboy::FormatError::none) {
                setFilename(filename);
            }
        }
    }
}

bool MainWindow::fileSave() {
    if (mFilename.isEmpty()) {
        return fileSaveAs();
    } else {
        return mApp.document.save(mFilename);
    }
}

bool MainWindow::fileSaveAs() {
    mModuleFileDialog->setFileMode(QFileDialog::FileMode::AnyFile);
    mModuleFileDialog->setAcceptMode(QFileDialog::AcceptSave);
    mModuleFileDialog->setWindowTitle("Save As");
    if (mModuleFileDialog->exec() != QDialog::Accepted) {
        return false;
    }
    QString filename = mModuleFileDialog->selectedFiles().first();
    if (mApp.document.save(filename)) {
        setFilename(filename);
        return true;
    }

    return false;
}

void MainWindow::moduleRemoveSong() {

    auto result = QMessageBox::question(this,
        "Trackerboy",
        "Do you want to remove this song? There is no undo for this action"
    );

    if (result == QMessageBox::Yes) {
        mApp.songModel.remove();
    }

}

void MainWindow::windowResetLayout() {

    // this slot will reset all docks and toolbars to the default layout
    // since qt designer does not make this easy, we will do this here instead
    // of setting it up in the ui file.

    // when adding new docks to the ui file, add it to the list here

    // setup corners, left and right get both corners
    setCorner(Qt::Corner::TopLeftCorner, Qt::DockWidgetArea::LeftDockWidgetArea);
    setCorner(Qt::Corner::TopRightCorner, Qt::DockWidgetArea::RightDockWidgetArea);
    setCorner(Qt::Corner::BottomLeftCorner, Qt::DockWidgetArea::LeftDockWidgetArea);
    setCorner(Qt::Corner::BottomRightCorner, Qt::DockWidgetArea::RightDockWidgetArea);

    // toolbars
    // just add them in order to the top toolbar area

    std::array<QToolBar*, 4> toolbarArray = {
        mUi->toolbarFile,
        mUi->toolbarOrder,
        mUi->toolbarTracker,
        mSongToolbar
    };

    for (auto toolbar : toolbarArray) {
        addToolBar(Qt::ToolBarArea::TopToolBarArea, toolbar);
    }

    std::array<QDockWidget*, 6> dockArray = {
        mDockSongProperties,
        mDockModuleProperties,
        mDockSongs,
        mDockOrders,
        mDockInstruments,
        mDockWaveforms
    };

    // remove everything
    for (auto dock : dockArray) {
        dock->setFloating(false);
        removeDockWidget(dock);
    }

    // add everything back in the desired order

    // Note: a | means the docks are tabbed
    // left area: (dockSongProperties | dockModuleProperties | dockSongs) dockOrders
    addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, mDockSongProperties);
    tabifyDockWidget(mDockSongProperties, mDockModuleProperties);
    tabifyDockWidget(mDockModuleProperties, mDockSongs);
    addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, mDockOrders);

    // top area: dockInstruments dockWaveforms
    addDockWidget(Qt::DockWidgetArea::TopDockWidgetArea, mDockInstruments);
    addDockWidget(Qt::DockWidgetArea::TopDockWidgetArea, mDockWaveforms);
    
    // set visible
    for (auto dock : dockArray) {
        dock->setVisible(true);
    }

    // resize
    
    int topwidth = (width()) / 2;
    resizeDocks({ mDockInstruments, mDockWaveforms }, { topwidth, topwidth }, Qt::Horizontal);

    resizeDocks({ mDockSongProperties, mDockOrders }, { mDockSongProperties->minimumHeight(), mDockOrders->maximumHeight() }, Qt::Vertical);
}

void MainWindow::onSoundChange() {
    auto &sound = mApp.config.sound();
    //auto rate = audio::SAMPLERATE_TABLE[sound.samplerate];
    mSamplerateLabel->setText(QString("%1 Hz").arg(sound.samplerate));
}

void MainWindow::statusSetInstrument(int index) {
    int id = (index == -1) ? 0 : mApp.instrumentModel.instrument(index)->id();
    mStatusInstrument->setText(QString("Instrument: %1").arg(id, 2, 16, QChar('0')));
}

void MainWindow::statusSetWaveform(int index) {
    int id = (index == -1) ? 0 : mApp.waveModel.waveform(index)->id();
    mStatusWaveform->setText(QString("Waveform: %1").arg(id, 2, 16, QChar('0')));
}

void MainWindow::statusSetOctave(int octave) {
    mStatusOctave->setText(QString("Octave: %1").arg(octave));
}

// PRIVATE METHODS -----------------------------------------------------------


bool MainWindow::maybeSave() {
    if (!mApp.document.isModified()) {
        return true;
    }

    auto const result = QMessageBox::warning(
        this,
        tr("Trackerboy"),
        tr("Save changes to \"%1\"?").arg(mDocumentName),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
        );

    switch (result) {
        case QMessageBox::Save:
            return fileSave();
        case QMessageBox::Cancel:
            return false;
        default:
            break;
    }

    return true;
}

void MainWindow::readSettings() {
    QSettings settings;
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    if (geometry.isEmpty()) {
        // initialize window size to 3/4 of the screen's width and height
        // screen() was added at version 5.14
        const QRect availableGeometry = window()->screen()->availableGeometry();
        resize(availableGeometry.width() / 4 * 3, availableGeometry.height() / 4 * 3);
        move((availableGeometry.width() - width()) / 2,
            (availableGeometry.height() - height()) / 2);
        
    } else {
#else
    if (!geometry.isEmpty()) {
#endif
        restoreGeometry(geometry);
    }
    const QByteArray windowState = settings.value("windowState").toByteArray();
    if (windowState.isEmpty()) {
        // no saved window state, use the default
        // this way we don't have to fiddle with the layout in designer
        windowResetLayout();
    } else {
        restoreState(windowState);
    }
}

void MainWindow::setFilename(QString filename) {
    mFilename = filename;
    if (filename.isEmpty()) {
        mDocumentName = "Untitled";
    } else {
        QFileInfo info(filename);
        mDocumentName = info.fileName();
    }
    updateWindowTitle();
}

void MainWindow::setModelsEnabled(bool enabled) {
    mApp.instrumentModel.setEnabled(enabled);
    mApp.songModel.setEnabled(enabled);
    mApp.waveModel.setEnabled(enabled);
}

void MainWindow::setupConnections() {
    connect(&mApp.document, &ModuleDocument::modifiedChanged, this, &QMainWindow::setWindowModified);

    // Actions
    #define connectAction(action, slot) connect(mUi->action, &QAction::triggered, this, &MainWindow::slot)
    // File
    connectAction(actionNew, fileNew);
    connectAction(actionOpen, fileOpen);
    connectAction(actionSave, fileSave);
    connectAction(actionSaveAs, fileSaveAs);
    connectAction(actionQuit, close);
    connectAction(actionRemoveSong, moduleRemoveSong);
    connect(mUi->actionConfiguration, &QAction::triggered, mConfigDialog, &QDialog::show);
    // Module
    connect(mUi->actionNewSong, &QAction::triggered, &mApp.songModel, &SongListModel::add);
    // Order
    auto orderModel = mApp.songModel.orderModel();
    connect(mUi->actionInsertOrder, &QAction::triggered, orderModel, &OrderModel::insert);
    connect(mUi->actionRemoveOrder, &QAction::triggered, orderModel, &OrderModel::remove);
    connect(mUi->actionDuplicateOrder, &QAction::triggered, orderModel, &OrderModel::duplicate);
    connect(mUi->actionMoveOrderUp, &QAction::triggered, orderModel, &OrderModel::moveUp);
    connect(mUi->actionMoveOrderDown, &QAction::triggered, orderModel, &OrderModel::moveDown);


    QApplication::connect(mUi->actionAboutQt, &QAction::triggered, &QApplication::aboutQt);

    // connect piano signals to renderer preview slots

    auto wavePiano = mWaveEditor->piano();
    connect(wavePiano, &PianoWidget::keyDown, &mApp.renderer, &Renderer::previewWaveform);
    connect(wavePiano, &PianoWidget::keyUp, &mApp.renderer, &Renderer::stopPreview);

    auto instPiano = mInstrumentEditor->piano();
    connect(instPiano, &PianoWidget::keyDown, &mApp.renderer, &Renderer::previewInstrument);
    connect(instPiano, &PianoWidget::keyUp, &mApp.renderer, &Renderer::stopPreview);

    // song combobox in mSongToolbar
    

    connect(mSongCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &mApp.songModel, QOverload<int>::of(&SongListModel::select));
    connect(&mApp.songModel, &SongListModel::currentIndexChanged, mSongCombo, &QComboBox::setCurrentIndex);


    connect(&mApp.config, &Config::soundConfigChanged, this, &MainWindow::onSoundChange);

    // statusbar

    connect(&mApp.instrumentModel, &InstrumentListModel::currentIndexChanged, this, &MainWindow::statusSetInstrument);
    connect(&mApp.waveModel, &WaveListModel::currentIndexChanged, this, &MainWindow::statusSetWaveform);


}

void MainWindow::setupUi() {

    // Main widget

    auto pgrid = new PatternEditor(mApp.songModel);
    setCentralWidget(pgrid);

    // TOOLBARS ==============================================================

    // song toolbar
    mSongToolbar = new QToolBar("Songs toolbar");
    mSongToolbar->setObjectName("mSongToolbar");
    mSongToolbar->addAction(mUi->actionPreviousSong);
    mSongToolbar->addAction(mUi->actionNextSong);
    mSongCombo = new QComboBox();
    mSongCombo->setModel(&mApp.songModel);
    mSongToolbar->addWidget(mSongCombo);
    mSongToolbar->setIconSize(QSize(TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT));
    addToolBar(Qt::ToolBarArea::TopToolBarArea, mSongToolbar);

    // toolbar icons
    Tileset tileset(QImage(":/icons/toolbar.png"), TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT);
    QList<QToolBar*> toolbars = { mUi->toolbarFile, mUi->toolbarOrder, mUi->toolbarTracker, mSongToolbar};

    for (int i = 0, iconIndex = 0; i != toolbars.size(); ++i) {
        auto toolbar = toolbars.at(i);
        auto actions = toolbar->actions();
        for (int j = 0; j != actions.size(); ++j) {
            auto action = actions.at(j);
            if (action->isSeparator() || action->isWidgetType()) {
                continue;
            }
            action->setIcon(tileset.getIcon(iconIndex));
            ++iconIndex;
        }
    }

    // DIALOGS ===============================================================

    mModuleFileDialog->setNameFilter(tr("Trackerboy Module (*.tbm)"));
    mModuleFileDialog->setWindowModality(Qt::WindowModal);

    // DOCKS =================================================================

    // setup Instruments dock
    mDockInstruments = new QDockWidget(tr("Instruments"), this);
    mDockInstruments->setObjectName("mDockInstruments");
    TableForm *instrumentTableForm = new TableForm(
        mApp.instrumentModel,
        mInstrumentEditor,
        "Ctrl-I",
        "instrument",
        mDockInstruments
    );
    mDockInstruments->setWidget(instrumentTableForm);
    

    // setup Waveforms dock
    mDockWaveforms = new QDockWidget(tr("Waveforms"), this);
    mDockWaveforms->setObjectName("mDockWaveforms");
    TableForm *waveTableForm = new TableForm(
        mApp.waveModel,
        mWaveEditor,
        "Ctrl-W",
        "waveform",
        mDockWaveforms
    );
    mDockWaveforms->setWidget(waveTableForm);

    // setup Songs dock
    mDockSongs = new QDockWidget(tr("Songs"), this);
    mDockSongs->setObjectName("mDockSongs");
    auto songWidget = new SongWidget(mApp.songModel, mDockSongs);
    mDockSongs->setWidget(songWidget);

    // module properties dock
    mDockModuleProperties = new QDockWidget(tr("Module properties"), this);
    mDockModuleProperties->setObjectName("mDockModuleProperties");
    auto modulePropertiesWidget = new ModulePropertiesWidget(mDockModuleProperties);
    mDockModuleProperties->setWidget(modulePropertiesWidget);

    // song properties dock
    mDockSongProperties = new QDockWidget(tr("Song properties"), this);
    mDockSongProperties->setObjectName("mDockSongProperties");
    auto songPropertiesWidget = new SongPropertiesWidget(mDockSongProperties);
    mDockSongProperties->setWidget(songPropertiesWidget);

    // setup Orders dock
    
    
    OrderActions orderActions = {
        mUi->actionInsertOrder,
        mUi->actionRemoveOrder,
        mUi->actionDuplicateOrder,
        mUi->actionMoveOrderUp,
        mUi->actionMoveOrderDown
    };
    auto orderModel = mApp.songModel.orderModel();
    orderModel->setActions(orderActions);
    mDockOrders = new QDockWidget(tr("Orders"), this);
    mDockOrders->setObjectName("mDockOrders");
    auto orderWidget = new OrderWidget(*orderModel, mUi->menuOrder, mDockOrders);
    mDockOrders->setWidget(orderWidget);

    addDockWidget(Qt::TopDockWidgetArea, mDockInstruments);
    addDockWidget(Qt::TopDockWidgetArea, mDockWaveforms);
    addDockWidget(Qt::LeftDockWidgetArea, mDockSongProperties);
    addDockWidget(Qt::LeftDockWidgetArea, mDockModuleProperties);
    addDockWidget(Qt::LeftDockWidgetArea, mDockSongs);
    addDockWidget(Qt::LeftDockWidgetArea, mDockOrders);




    // MENUS =================================================================

    // add the context menu for instruments list view to our menubar
    auto menu = instrumentTableForm->menu();
    menu->setTitle("Instrument");
    mUi->menubar->insertMenu(mUi->menuTracker->menuAction(), menu);
    // same thing but for waveforms
    menu = waveTableForm->menu();
    menu->setTitle("Waveform");
    mUi->menubar->insertMenu(mUi->menuTracker->menuAction(), menu);

    // add the popup menu to menubar
    QMenu *windowMenu = createPopupMenu();
    if (windowMenu != nullptr) {
        // TODO: override createPopupMenu() with this code
        windowMenu->setTitle("Window");
        windowMenu->addSeparator();
        auto resetLayoutAction = windowMenu->addAction("Reset layout");
        connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::windowResetLayout);
        mUi->menubar->insertMenu(mUi->menuHelp->menuAction(), windowMenu);
    }

    // STATUSBAR ==============================================================
    #define addLabelToStatusbar(var, text) var = new QLabel(text); mUi->statusbar->addPermanentWidget(var)
    
    addLabelToStatusbar(mStatusInstrument, "Instrument: 00");
    addLabelToStatusbar(mStatusWaveform, "Waveform: 00");
    addLabelToStatusbar(mStatusOctave, "Octave: 3");
    addLabelToStatusbar(mStatusFramerate, "59.7 FPS");
    addLabelToStatusbar(mStatusSpeed, "6.0 FPR");
    addLabelToStatusbar(mStatusTempo, "150 BPM");
    addLabelToStatusbar(mStatusElapsed, "00:00:00");
    addLabelToStatusbar(mStatusPos, "00 / 00");
    addLabelToStatusbar(mSamplerateLabel, "");
}

void MainWindow::writeSettings() {
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}
