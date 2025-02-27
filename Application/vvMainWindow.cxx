// Copyright 2013 Velodyne Acoustics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// Copyright 2013 Velodyne Acoustics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "vvMainWindow.h"
#include "ui_vvMainWindow.h"
#include "lqDockableSpreadSheetReaction.h"
#include "lqSaveLidarStateReaction.h"
#include "lqLoadLidarStateReaction.h"
#include "lqEnableAdvancedArraysReaction.h"
#include "lqOpenSensorReaction.h"
#include "lqOpenPcapReaction.h"
#include "lqOpenRecentFilesReaction.h"
#include "lqUpdateCalibrationReaction.h"
#include "lqSaveLidarFrameReaction.h"
#include "lqSaveLASReaction.h"

#include <vtkSMProxyManager.h>
#include <vtkSMSessionProxyManager.h>

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqInterfaceTracker.h>
#include <pqObjectBuilder.h>
#include <pqOutputWidget.h>
#include <pqRenderView.h>
#include <pqRenderViewSelectionReaction.h>
#include <pqDeleteReaction.h>
#include <pqHelpReaction.h>
#include <pqDesktopServicesReaction.h>
#include <pqServer.h>
#include <pqSettings.h>
#include <pqLidarViewManager.h>
#include <pqParaViewMenuBuilders.h>
#include <pqPythonManager.h>
#include <pqTabbedMultiViewWidget.h>
#include <pqSetName.h>
#include <vtkSMPropertyHelper.h>
#include "pqAxesToolbar.h"
#include "pqCameraToolbar.h"
#include <pqParaViewBehaviors.h>
#include <pqDataRepresentation.h>
#include <pqPythonShell.h>
#include <pqLoadDataReaction.h>

#include <QToolBar>
#include <QShortcut>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>
#include <QDockWidget>
#include <QDragEnterEvent>

#include <cassert>
#include <iostream>
#include <sstream>

#include "lqPlayerControlsToolbar.h"

// Declare the plugin to load.
#define PARAVIEW_BUILDING_PLUGIN
#include "vtkPVPlugin.h"
PV_PLUGIN_IMPORT_INIT(PythonQtPlugin); //could link it dynamically actually wip ?

class vvMainWindow::pqInternals
{
public:
  pqInternals(vvMainWindow* window)
    : Ui()
    , MainView(0)
  {
    this->Ui.setupUi(window);
    this->paraviewInit(window);
    this->setupUi();

    window->show();
    window->raise();
    window->activateWindow();
  }
  Ui::vvMainWindow Ui;
  pqRenderView* MainView = nullptr;
  pqServer* Server = nullptr;
  pqObjectBuilder* Builder = nullptr;

private:
  void paraviewInit(vvMainWindow* window)
  {
    pqApplicationCore* core = pqApplicationCore::instance();

    // need to be created before the first scene
    QToolBar* vcrToolbar = new lqPlayerControlsToolbar(window)
      << pqSetName("Player Control");
    window->addToolBar(Qt::TopToolBarArea, vcrToolbar);

    QToolBar* cameraToolbar = new pqCameraToolbar(window)
      << pqSetName("cameraToolbar");
    window->addToolBar(Qt::TopToolBarArea, cameraToolbar);

    QToolBar* axesToolbar = new pqAxesToolbar(window)
      << pqSetName("axesToolbar");
    window->addToolBar(Qt::TopToolBarArea, axesToolbar);

    // create pythonshell
    pqPythonShell* shell = new pqPythonShell(window);
    shell->setObjectName("pythonShell");
    shell->setFontSize(8);
    this->Ui.pythonShellDock->setWidget(shell);
    pqLidarViewManager::instance()->setPythonShell(shell);

    // Give the macros menu to the pqPythonMacroSupervisor
    pqPythonManager* manager =
      qobject_cast<pqPythonManager*>(pqApplicationCore::instance()->manager("PYTHON_MANAGER"));
    if (manager)
    {
      QToolBar* macrosToolbar = new QToolBar("Macros Toolbars", window)
        << pqSetName("MacrosToolbar");
      manager->addWidgetForRunMacros(macrosToolbar);
      window->addToolBar(Qt::TopToolBarArea, macrosToolbar);
    }

    // Define application behaviors.
    pqParaViewBehaviors::enableQuickLaunchShortcuts();
    pqParaViewBehaviors::enableSpreadSheetVisibilityBehavior();
    pqParaViewBehaviors::enableObjectPickingBehavior();
    pqParaViewBehaviors::enableCrashRecoveryBehavior();
    pqParaViewBehaviors::enableAutoLoadPluginXMLBehavior();
    pqParaViewBehaviors::enableDataTimeStepBehavior();
    pqParaViewBehaviors::enableCommandLineOptionsBehavior();
    pqParaViewBehaviors::enableLiveSourceBehavior();
    pqParaViewBehaviors::enableApplyBehavior();
    pqParaViewBehaviors::enableStandardViewFrameActions();
    pqParaViewBehaviors::enableStandardPropertyWidgets();
    pqParaViewBehaviors::setEnableDefaultViewBehavior(false);

    // Check if the settings are well formed i.e. if an OriginalMainWindow
    // state was previously saved. If not, we don't want to automatically
    // restore the settings state nor save it on quitting LidarView.
    // An OriginalMainWindow state will be force saved once the UI is completly
    // set up.
    pqSettings* const settings = pqApplicationCore::instance()->settings();
    bool shouldClearSettings = false;
    QStringList keys = settings->allKeys();

    if (keys.size() == 0)
    {
      // There were no settings before, let's save the current state as
      // OriginalMainWindow state
      shouldClearSettings = true;
    }
    else
    {
      // Checks if the existing settings are well formed and if not, clear them.
      // An original MainWindow state will be force saved later once the UI is
      // entirely set up
      for (int keyIndex = 0; keyIndex < keys.size(); ++keyIndex)
      {
        if (keys[keyIndex].contains("OriginalMainWindow"))
        {
          shouldClearSettings = true;
          break;
        }
      }
    }

    if (shouldClearSettings)
    {
      pqParaViewBehaviors::enablePersistentMainWindowStateBehavior();
    }
    else
    {
      if (keys.size() > 0)
      {
        vtkGenericWarningMacro("Settings weren't set correctly. Clearing settings.");
      }

      // As pqPersistentMainWindowStateBehavior is not created right now,
      // we can clear the settings as the current bad state won't be saved on
      // closing LidarView
      settings->clear();
    }

    // the paraview behaviors, which will in our case instantiate the enableStandardViewFrameActions
    // must be created before creating the first renderview, otherwise this view won't have the default
    // view toolbar buttons/actions
    new pqParaViewBehaviors(window, window);

    // Connect to builtin server.
    this->Builder = core->getObjectBuilder();
    this->Server = this->Builder->createServer(pqServerResource("builtin:"));
    pqActiveObjects::instance().setActiveServer(this->Server);

    // Set default render view settings
    vtkSMSessionProxyManager* pxm =
      vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager();
    vtkSMProxy* renderviewsettings = pxm->GetProxy("RenderViewSettings");
    assert(renderviewsettings);

    vtkSMPropertyHelper(renderviewsettings, "ResolveCoincidentTopology").Set(0);

    // Set the central widget
    pqTabbedMultiViewWidget* mv = new pqTabbedMultiViewWidget;
    mv->setTabVisibility(false);
    window->setCentralWidget(mv);

    new lqDockableSpreadSheetReaction(this->Ui.actionSpreadsheet, window);

    this->MainView =
      qobject_cast<pqRenderView*>(this->Builder->createView(pqRenderView::renderViewType(), this->Server));
    assert(this->MainView);

    // Add view to layout
    this->Builder->addToLayout(this->MainView);

    vtkSMPropertyHelper(this->MainView->getProxy(), "CenterAxesVisibility").Set(0);
    double bgcolor[3] = { 0, 0, 0 };
    vtkSMPropertyHelper(this->MainView->getProxy(), "Background").Set(bgcolor, 3);
    // MultiSamples doesn't work, we need to set that up before registering the proxy.
    // vtkSMPropertyHelper(view->getProxy(),"MultiSamples").Set(1);
    this->MainView->getProxy()->UpdateVTKObjects();

    // Add save/load lidar state action
    new lqSaveLidarStateReaction(this->Ui.actionSaveLidarState);
    new lqLoadLidarStateReaction(this->Ui.actionLoadLidarState);

    // Change calibration reaction
    new lqUpdateCalibrationReaction(this->Ui.actionChoose_Calibration_File);

    // Specify each Properties Panel as we do want to present one panel per dock
    this->Ui.propertiesPanel->setPanelMode(pqPropertiesPanel::SOURCE_PROPERTIES);
    this->Ui.viewPropertiesPanel->setPanelMode(pqPropertiesPanel::VIEW_PROPERTIES);
    this->Ui.displayPropertiesPanel->setPanelMode(pqPropertiesPanel::DISPLAY_PROPERTIES);

    // Enable help from the properties panel.
    QObject::connect(this->Ui.propertiesPanel,
      SIGNAL(helpRequested(const QString&, const QString&)),
      window, SLOT(showHelpForProxy(const QString&, const QString&)));

    /// hook delete to pqDeleteReaction.
    QAction* tempDeleteAction = new QAction(window);
    pqDeleteReaction* handler = new pqDeleteReaction(tempDeleteAction);
    handler->connect(this->Ui.propertiesPanel,
      SIGNAL(deleteRequested(pqProxy*)),
      SLOT(deleteSource(pqProxy*)));

    // specify how corner are occupied by the dockable widget
    window->setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    window->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    window->setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    window->setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // organize dockable widget in tab
    window->setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    window->tabifyDockWidget(this->Ui.propertiesDock, this->Ui.colorMapEditorDock);
    window->tabifyDockWidget(this->Ui.viewPropertiesDock, this->Ui.colorMapEditorDock);
    window->tabifyDockWidget(this->Ui.displayPropertiesDock, this->Ui.colorMapEditorDock);
    window->tabifyDockWidget(this->Ui.informationDock, this->Ui.memoryInspectorDock);
    window->tabifyDockWidget(this->Ui.informationDock, this->Ui.viewAnimationDock);
    window->tabifyDockWidget(this->Ui.informationDock, this->Ui.outputWidgetDock);
    window->tabifyDockWidget(this->Ui.informationDock, this->Ui.pythonShellDock);

    // hide docker by default
    this->Ui.pipelineBrowserDock->hide();
    this->Ui.propertiesDock->hide();
    this->Ui.viewPropertiesDock->hide();
    this->Ui.displayPropertiesDock->hide();
    this->Ui.colorMapEditorDock->hide();
    this->Ui.informationDock->hide();
    this->Ui.memoryInspectorDock->hide();
    this->Ui.viewAnimationDock->hide();
    this->Ui.outputWidgetDock->hide();
    this->Ui.pythonShellDock->hide();
    this->Ui.sensorListDock->hide();

    // Setup the View menu. This must be setup after all toolbars and dockwidgets
    // have been created.
    pqParaViewMenuBuilders::buildViewMenu(*this->Ui.menuViews, *window);

    /// If you want to automatically add a menu for sources as requested in the
    /// configuration pass in a non-null main window.
    pqParaViewMenuBuilders::buildSourcesMenu(*this->Ui.menuSources, nullptr);

    /// If you want to automatically add a menu for filters as requested in the
    /// configuration pass in a non-null main window.
    pqParaViewMenuBuilders::buildFiltersMenu(*this->Ui.menuFilters, nullptr);

    // setup the context menu for the pipeline browser.
    pqParaViewMenuBuilders::buildPipelineBrowserContextMenu(*this->Ui.pipelineBrowser->contextMenu());

    // build Paraview file menu
    QMenu *paraviewFileMenu = this->Ui.menuAdvance->addMenu("File (Paraview)");
    pqParaViewMenuBuilders::buildFileMenu(*paraviewFileMenu);
    // for some reason the menu builder rename the QMenu...
    paraviewFileMenu->setTitle("File (Paraview)");

    // build Paraview edit menu
    QMenu *paraviewEditMenu = this->Ui.menuAdvance->addMenu("Edit (Paraview)");
    // for some reason the menu builder rename the QMenu...
    pqParaViewMenuBuilders::buildEditMenu(*paraviewEditMenu);
    paraviewEditMenu->setTitle("Edit (Paraview)");

    // build Paraview tools menu
    QMenu *paraviewToolsMenu = this->Ui.menuAdvance->addMenu("Tools (Paraview)");
    pqParaViewMenuBuilders::buildToolsMenu(*paraviewToolsMenu);

    // build Paraview macro menu
    QMenu *paraviewMacroMenu = this->Ui.menuAdvance->addMenu("Macro (Paraview)");
    pqParaViewMenuBuilders::buildMacrosMenu(*paraviewMacroMenu);

    // Add Professional Support / How to SLAM Menu Actions
    new pqDesktopServicesReaction(QUrl("https://www.kitware.com/what-we-offer"),
      (this->Ui.actionHelpSupport ));

    new pqDesktopServicesReaction(QUrl("https://gitlab.kitware.com/keu-computervision/slam/-/blob/master/paraview_wrapping/doc/How_to_SLAM_with_LidarView.md"),
      (this->Ui.actionHelpSlam ));

    pqActiveObjects::instance().setActiveView(this->MainView);
  }

  //-----------------------------------------------------------------------------
  void setupUi()
  {
    new pqRenderViewSelectionReaction(this->Ui.actionSelect_Visible_Points, this->MainView,
      pqRenderViewSelectionReaction::SELECT_SURFACE_POINTS);
    new pqRenderViewSelectionReaction(this->Ui.actionSelect_All_Points, this->MainView,
      pqRenderViewSelectionReaction::SELECT_FRUSTUM_POINTS);

    pqLidarViewManager::instance()->setup();

    pqSettings* const settings = pqApplicationCore::instance()->settings();
    const QVariant& gridVisible =
      settings->value("LidarPlugin/MeasurementGrid/Visibility", true);
    this->Ui.actionMeasurement_Grid->setChecked(gridVisible.toBool());

    new lqOpenSensorReaction(this->Ui.actionOpen_Sensor_Stream);
    new lqOpenPcapReaction(this->Ui.actionOpenPcap);

    new lqOpenRecentFilesReaction(this->Ui.menuRecent_Files, this->Ui.actionClear_Menu);

    lqSensorListWidget * listSensor = lqSensorListWidget::instance();
    listSensor->setCalibrationFunction(&lqUpdateCalibrationReaction::UpdateExistingSource);

    new lqSaveLidarFrameReaction(this->Ui.actionSavePCD, "PCDWriter", "pcd", false, false, true, true);
    new lqSaveLidarFrameReaction(this->Ui.actionSaveCSV, "DataSetCSVWriter", "csv", false, false, true, true);
    new lqSaveLidarFrameReaction(this->Ui.actionSavePLY, "PPLYWriter", "ply", false, false, true, true);
    new lqSaveLASReaction(this->Ui.actionSaveLAS, false, false, true, true);

    connect(this->Ui.actionMeasurement_Grid, SIGNAL(toggled(bool)), pqLidarViewManager::instance(),
      SLOT(onMeasurementGrid(bool)));

    connect(this->Ui.actionResetDefaultSettings, SIGNAL(triggered()),
      pqLidarViewManager::instance(), SLOT(onResetDefaultSettings()));

    connect(this->Ui.actionShowErrorDialog, SIGNAL(triggered()), this->Ui.outputWidgetDock,
      SLOT(show()));

    connect(this->Ui.actionPython_Console, SIGNAL(triggered()), this->Ui.pythonShellDock,
      SLOT(show()));

    // Add save/load lidar state action
    new lqEnableAdvancedArraysReaction(this->Ui.actionEnableAdvancedArrays);
  }
};

//-----------------------------------------------------------------------------
vvMainWindow::vvMainWindow()
  : Internals(new vvMainWindow::pqInternals(this))
{
  pqApplicationCore::instance()->registerManager(
    "COLOR_EDITOR_PANEL", this->Internals->Ui.colorMapEditorDock);
  this->Internals->Ui.colorMapEditorDock->hide();

  // show output widget if we received an error message.
  this->connect(this->Internals->Ui.outputWidget, SIGNAL(messageDisplayed(const QString&, int)),
    SLOT(handleMessage(const QString&, int)));

  PV_PLUGIN_IMPORT(PythonQtPlugin);

  // Branding
  std::stringstream ss;
  ss << "Reset " << SOFTWARE_NAME << " settings";
  QString text = QString(ss.str().c_str());
  this->Internals->Ui.actionResetDefaultSettings->setText(text);
  ss.str("");
  ss.clear();

  ss << "This will reset all " << SOFTWARE_NAME << " settings by default";
  text = QString(ss.str().c_str());
  this->Internals->Ui.actionResetDefaultSettings->setIconText(text);
  ss.str("");
  ss.clear();

  ss << "About " << SOFTWARE_NAME;
  text = QString(ss.str().c_str());
  this->Internals->Ui.actionAbout_LidarView->setText(text);
  ss.str("");
  ss.clear();
}

//-----------------------------------------------------------------------------
vvMainWindow::~vvMainWindow()
{
  delete this->Internals;
  this->Internals = NULL;
}

//-----------------------------------------------------------------------------
void vvMainWindow::dragEnterEvent(QDragEnterEvent* evt)
{
  evt->acceptProposedAction();
}

//-----------------------------------------------------------------------------
void vvMainWindow::dropEvent(QDropEvent* evt)
{
  QList<QUrl> urls = evt->mimeData()->urls();
  if (urls.isEmpty())
  {
    return;
  }

  QList<QString> files;

  foreach (QUrl url, urls)
  {
    if (!url.toLocalFile().isEmpty())
    {
      files.append(url.toLocalFile());
    }
  }

  // If we have no file we return
  if (files.empty() || files.first().isEmpty())
  {
    return;
  }

  if (files[0].endsWith(".pcap"))
  {
    lqOpenPcapReaction::createSourceFromFile(files[0]);
  }
  else if (files[0].endsWith(".pcd"))
  {
    QMessageBox::warning(nullptr, tr(""), tr("Unsupported input format") );
    return;
  }
  else
  {
    pqLoadDataReaction::loadData(files);
  }
}

//-----------------------------------------------------------------------------
void vvMainWindow::showHelpForProxy(const QString& groupname, const
  QString& proxyname)
{
  pqHelpReaction::showProxyHelp(groupname, proxyname);
}

void vvMainWindow::handleMessage(const QString &, int type)
{
  QDockWidget* dock = this->Internals->Ui.outputWidgetDock;
  if (!dock)
    return;
    
  if (!dock->isVisible() && (type == QtCriticalMsg || type == QtFatalMsg || type == QtWarningMsg))
  {
    // if dock is not visible, we always pop it up as a floating dialog. This
    // avoids causing re-renders which may cause more errors and more confusion.
    QRect rectApp = this->geometry();

    QRect rectDock(
      QPoint(0, 0), QSize(static_cast<int>(rectApp.width() * 0.4), dock->sizeHint().height()));
    rectDock.moveCenter(
      QPoint(rectApp.center().x(), rectApp.bottom() - dock->sizeHint().height() / 2));
    dock->setFloating(true);
    dock->setGeometry(rectDock);
    dock->show();
  }
  if (dock->isVisible())
  {
    dock->raise();
  }
}
