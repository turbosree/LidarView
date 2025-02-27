#include "lqOpenSensorReaction.h"

//#include <vtkSMSessionProxyManager.h>
#include <vtkNew.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>

#include <pqActiveObjects.h>
#include <pqDeleteReaction.h>
#include <pqPVApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPipelineSource.h>
#include <pqServerManagerModel.h>
#include <pqView.h>

#include "lqHelper.h"
#include "lqUpdateCalibrationReaction.h"
#include "pqLidarViewManager.h"
#include "vvCalibrationDialog.h"
#include "lqSensorListWidget.h"

#include <QString>
#include <string>

//-----------------------------------------------------------------------------
lqOpenSensorReaction::lqOpenSensorReaction(QAction *action) :
  Superclass(action)
{

}

//-----------------------------------------------------------------------------
void lqOpenSensorReaction::onTriggered()
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
  pqView* view = pqActiveObjects::instance().activeView();
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  // Launch the calibration Dialog before creating the Source to allow to cancel the action
  // (with the "cancel" button in the dialog)
  vvCalibrationDialog dialog(pqLidarViewManager::instance()->getMainWindow());
  DisplayDialogOnActiveWindow(dialog);
  if (!dialog.exec())
  {
    return;
  }

  // We remove all lidarReader and PositionOrientationReader (and every filter depending on them) in the pipeline
  // TODO : As soon as LidarReader is available in multi sensor mode, we have to remove only the vtkLidarReader sources
  RemoveAllProxyTypeFromPipelineBrowser<vtkLidarReader *>();
  RemoveAllProxyTypeFromPipelineBrowser<vtkPositionOrientationPacketReader *>();

  // If the user don't enable multi sensors, we have to clean the pipeline by removing all stream
  // In the lqSensorListWidget, every PositionOrientationStream is linked to a LidarStream
  // If a LidarStream is delete, it will automatically delete its PositionOrientationStream.
  // So we just have to delete all lidarStream.
  if(!dialog.isEnableMultiSensors())
  {
    RemoveAllProxyTypeFromPipelineBrowser<vtkLidarStream *>();
  }

  // Create the lidarSensor
  // We have to use pqObjectBuilder::createSource to add the created source to the pipeline
  // The source will be created immediately so the signal "sourceAdded" of the pqServerManagerModel
  // is send during "create source".
  // To get the pqPipelineSource modified with the new property, you have to connect to the signal
  // "dataUpdated" of the pqServerManagerModel
  pqPipelineSource* lidarSource = builder->createSource("sources", "LidarStream", server);
  QString lidarName = lidarSource->getSMName();
  controller->Show(lidarSource->getSourceProxy(), 0, view->getViewProxy());

  pqPipelineSource * posOrSource = nullptr;
  QString posOrName = "";

  // Update lidarSource and posOrSource
  // If the GPs interpretation is asked, the posOrsource will be created in the lqUpdateCalibrationReaction
  // because it has to manage it if the user enable interpreting GPS packet after the first instantiation
  lqUpdateCalibrationReaction::UpdateCalibration(lidarSource, posOrSource, dialog);

  if (posOrSource)
  {
    posOrName = posOrSource->getSMName();
    controller->Show(posOrSource->getSourceProxy(), 0, view->getViewProxy());
    posOrSource->getProxy()->InvokeCommand("Start");
  }

  // "Start" of the lidar Source have to be called
  lidarSource->getProxy()->InvokeCommand("Start");

  //Update applogic to be able to use function only define in applogic.
  pqLidarViewManager::instance()->runPython(QString("lv.UpdateApplogicLidar('%1', '%2')\n").arg(lidarName, posOrName));
}
