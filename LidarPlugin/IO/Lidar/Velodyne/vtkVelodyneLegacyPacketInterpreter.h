#ifndef VTKVELODYNELEGACYPACKETINTERPRETER_H
#define VTKVELODYNELEGACYPACKETINTERPRETER_H

#include <vtkUnsignedCharArray.h>
#include <vtkUnsignedIntArray.h>
#include <vtkUnsignedShortArray.h>
#include "vtkVelodyneBasePacketInterpreter.h"

#include <memory>

using namespace DataPacketFixedLength;

class RPMCalculator;
class FramingState;
class vtkRollingDataAccumulator;


class VTK_EXPORT vtkVelodyneLegacyPacketInterpreter : public vtkVelodyneBasePacketInterpreter
{
public:
  static vtkVelodyneLegacyPacketInterpreter* New();
  vtkTypeMacro(vtkVelodyneLegacyPacketInterpreter, vtkVelodyneBasePacketInterpreter);
  void PrintSelf(ostream& vtkNotUsed(os), vtkIndent vtkNotUsed(indent)){};
  enum DualFlag
  {
    DUAL_DISTANCE_NEAR = 0x1,  // point with lesser distance
    DUAL_DISTANCE_FAR = 0x2,   // point with greater distance
    DUAL_INTENSITY_HIGH = 0x4, // point with lesser intensity
    DUAL_INTENSITY_LOW = 0x8,  // point with greater intensity
    DUAL_DOUBLED = 0xf,        // point is single return
    DUAL_DISTANCE_MASK = 0x3,
    DUAL_INTENSITY_MASK = 0xc,
  };

  void ProcessPacket(unsigned char const * data, unsigned int dataLength) override;

  bool SplitFrame(bool force = false) override;

  bool IsLidarPacket(unsigned char const * data, unsigned int dataLength) override;


  void ResetCurrentFrame() override;

  bool PreProcessPacket(unsigned char const * data, unsigned int dataLength,
                        fpos_t filePosition = fpos_t(), double packetNetworkTime = 0,
                        std::vector<FrameInformation>* frameCatalog = nullptr) override;

  std::string GetSensorInformation() override;


protected:
  vtkSmartPointer<vtkPolyData> CreateNewEmptyFrame(vtkIdType numberOfPoints, vtkIdType prereservedNumberOfPoints = 60000) override;

  // Process the laser return from the firing data
  // firingData - one of HDL_FIRING_PER_PKT from the packet
  // hdl64offset - either 0 or 32 to support 64-laser systems
  // firingBlock - block of packet for firing [0-11]
  // azimuthDiff - average azimuth change between firings
  // timestamp - the timestamp of the packet
  // geotransform - georeferencing transform
  void ProcessFiring(const HDLFiringData* firingData,
    int firingBlockLaserOffset, int firingBlock, int azimuthDiff, double timestamp,
    unsigned int rawtime, bool isThisFiringDualReturnData, bool isDualReturnPacket);

  void PushFiringData(unsigned char laserId, unsigned char rawLaserId,
                      unsigned short azimuth, const unsigned short elevation, const double timestamp,
                      const unsigned int rawtime, const HDLLaserReturn* laserReturn,
                      const unsigned int channelNumber, const bool isFiringDualReturnData);


  void Init();

  double ComputeTimestamp(unsigned int tohTime, const FrameInformation& frameInfo);


  bool CheckReportedSensorAndCalibrationFileConsistent(const HDLDataPacket* dataPacket);

  vtkSmartPointer<vtkPoints> Points;
  vtkSmartPointer<vtkDoubleArray> PointsX;
  vtkSmartPointer<vtkDoubleArray> PointsY;
  vtkSmartPointer<vtkDoubleArray> PointsZ;
  vtkSmartPointer<vtkUnsignedCharArray> Intensity;
  vtkSmartPointer<vtkUnsignedCharArray> LaserId;
  vtkSmartPointer<vtkUnsignedShortArray> Azimuth;
  vtkSmartPointer<vtkDoubleArray> Distance;
  vtkSmartPointer<vtkUnsignedShortArray> DistanceRaw;
  vtkSmartPointer<vtkDoubleArray> Timestamp;
  vtkSmartPointer<vtkDoubleArray> VerticalAngle;
  vtkSmartPointer<vtkUnsignedIntArray> RawTime;
  vtkSmartPointer<vtkIntArray> IntensityFlag;
  vtkSmartPointer<vtkIntArray> DistanceFlag;
  vtkSmartPointer<vtkUnsignedIntArray> Flags;
  vtkSmartPointer<vtkIdTypeArray> DualReturnMatching;

  // sensor information
  unsigned char SensorPowerMode;
  SensorType ReportedSensor;
  DualReturnSensorMode ReportedSensorReturnMode;
  bool IsHDL64Data;
  bool IsVLS128;
  uint8_t ReportedFactoryField1;
  uint8_t ReportedFactoryField2;

  bool alreadyWarnedForIgnoredHDL64FiringPacket;

  bool OutputPacketProcessingDebugInfo;

  // Bolean to manage the correction of intensity which indicates if the user want to correct the
  // intensities
  bool WantIntensityCorrection;

  // WIP : We now have two method to compute the RPM :
  // - One method which computes the rpm using the point cloud
  // this method is not packets dependant but need a none empty
  // point cloud which can be tricky (hard cropping, none spinning sensor)
  // - One method which computes the rpm directly using the packets. the problem
  // is, if the packets format change, we will have to adapt the rpm computation
  // - For now, we will use the packet dependant method
//  std::unique_ptr<RPMCalculator> RpmCalculator_;

  RPMCalculator* RpmCalculator_;

  FramingState* CurrentFrameState;
  unsigned int LastTimestamp;
  std::vector<double> RpmByFrames;
  double TimeAdjust;
  vtkIdType LastPointId[HDL_MAX_NUM_LASERS];
  vtkIdType FirstPointIdOfDualReturnPair;

  unsigned char epororPowerMode;

  bool IsCorrectionFromLiveStream = true;

  // Sensor parameters presented as rolling data, extracted from enough packets
  vtkRollingDataAccumulator* rollingCalibrationData;

  bool ShouldCheckSensor;
  uint32_t lastGpsTimestamp = 0;

  vtkVelodyneLegacyPacketInterpreter();
  ~vtkVelodyneLegacyPacketInterpreter();

private:
  vtkVelodyneLegacyPacketInterpreter(const vtkVelodyneLegacyPacketInterpreter&) = delete;
  void operator=(const vtkVelodyneLegacyPacketInterpreter&) = delete;

};

/**
 * @brief VelodyneSpecificFrameInformation frame information
 *        that are specific to velodyne's sensor
 */
struct VelodyneSpecificFrameInformation : public SpecificFrameInformation
{
  //! Offset specific to the lidar data format
  //! Used because some frames start at the middle of a packet
  int FiringToSkip = 0;

  //! Indicates the number of time rolled that has occured
  //! since the beginning of the .pcap file. hence, to have
  //! a non rolling timestamp one should add to the rolling
  //! timestamp NbrOfRollingTime * MaxTimeBeforeRolling
  int NbrOfRollingTime = 0;

  void reset() { *this = VelodyneSpecificFrameInformation(); }
  std::unique_ptr<SpecificFrameInformation> clone() { return std::make_unique<VelodyneSpecificFrameInformation>(*this); }
};

#endif // VTKVELODYNELEGACYPACKETINTERPRETER_H
