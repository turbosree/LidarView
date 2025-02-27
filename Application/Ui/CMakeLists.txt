# Sanitize checks
if(NOT LV_INSTALL_LIBRARY_DIR )
  message(FATAL_ERROR "LV_INSTALL_LIBRARY_DIR not set")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_library(ApplicationUi
  Widgets/vvCalibrationDialog.cxx
  Widgets/vvCalibrationDialog.h
  Widgets/vvCalibrationDialog.ui
  Widgets/vvCropReturnsDialog.cxx
  Widgets/vvCropReturnsDialog.h
  Widgets/vvCropReturnsDialog.ui
  Widgets/vvSelectFramesDialog.cxx
  Widgets/vvSelectFramesDialog.h
  Widgets/vvSelectFramesDialog.ui
  Widgets/images/resources.qrc

  pqLidarViewManager.cxx
  pqLidarViewManager.h
  lqOpenPcapReaction.cxx
  lqOpenPcapReaction.h
  lqOpenRecentFilesReaction.cxx
  lqOpenRecentFilesReaction.h
  lqOpenSensorReaction.cxx
  lqOpenSensorReaction.h
  lqUpdateCalibrationReaction.cxx
  lqUpdateCalibrationReaction.h
  vvPythonQtDecorators.h
  )
target_link_libraries(ApplicationUi PUBLIC
  ParaView::pqCore
  ParaView::pqWidgets
  ParaView::pqComponents
  ParaView::pqApplicationComponents
  ParaView::pqPython
  ParaView::pvpythonmodules
  ParaView::PythonInitializer
  ParaView::RemotingAnimation
  ParaView::RemotingApplication
  ParaView::RemotingClientServerStream
  ParaView::RemotingCore
  ParaView::RemotingLive
  ParaView::RemotingMisc
  ParaView::RemotingServerManager
  ParaView::RemotingServerManagerPython
  ParaView::RemotingSettings
  ParaView::RemotingViews
  ParaView::RemotingViewsPython
  lqApplicationComponents #actually LVCore/ApplicationComponents
  PythonQt::PythonQt # Required to Wrap additional functions
  )

target_include_directories(ApplicationUi PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Widgets
  ${CMAKE_CURRENT_BINARY_DIR}
  )

include(GenerateExportHeader)
generate_export_header(ApplicationUi)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/applicationui_export.h DESTINATION ${LV_INSTALL_LIBRARY_DIR})

# Install Library needed
install(TARGETS ApplicationUi
        RUNTIME DESTINATION ${LV_INSTALL_RUNTIME_DIR}
        LIBRARY DESTINATION ${LV_INSTALL_LIBRARY_DIR}
)

#-----------------------------------------------------------------------------
# Add python source
#-----------------------------------------------------------------------------

add_subdirectory(python)
