#include "vtkLidarKITTIDataSetReader.h"

#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkInformationVector.h>
#include <vtkInformation.h>
#include <vtkPoints.h>
#include <vtkPointData.h>
#include <vtkDoubleArray.h>
#include <vtkPolyData.h>
#include <vtkMath.h>

#include <sstream>

# include <boost/filesystem.hpp>
namespace  {
//-----------------------------------------------------------------------------
vtkSmartPointer<vtkCellArray> NewVertexCells(vtkIdType numberOfVerts)
{
  vtkNew<vtkIdTypeArray> cells;
  cells->SetNumberOfValues(numberOfVerts * 2);
  vtkIdType* ids = cells->GetPointer(0);
  for (vtkIdType i = 0; i < numberOfVerts; ++i)
  {
    ids[i * 2] = 1;
    ids[i * 2 + 1] = i;
  }

  vtkSmartPointer<vtkCellArray> cellArray = vtkSmartPointer<vtkCellArray>::New();
  cellArray->SetCells(numberOfVerts, cells.GetPointer());
  return cellArray;
}

//-----------------------------------------------------------------------------
template<typename T>
vtkSmartPointer<T> CreateDataArray(const char* name, vtkPolyData* pd)
{
  vtkSmartPointer<T> array = vtkSmartPointer<T>::New();
  array->SetName(name);
  pd->GetPointData()->AddArray(array);
  return array;
}

typedef struct point {
  float x;
  float y;
  float z;
  float intensity;
} point_t;
}


//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkLidarKITTIDataSetReader)

//-----------------------------------------------------------------------------
vtkLidarKITTIDataSetReader::vtkLidarKITTIDataSetReader()
  : FileName(""),
    NumberOfTrailingFrames(0)
{

}

//-----------------------------------------------------------------------------
void vtkLidarKITTIDataSetReader::PrintSelf(std::ostream &os, vtkIndent indent)
{

}

//----------------------------------------------------------------------------
void vtkLidarKITTIDataSetReader::SetFileName(const std::string &filename)
{
  if (filename == this->FileName)
  {
    return;
  }

  if (!boost::filesystem::exists(filename))
  {
    vtkErrorMacro(<< "Folder not be found! Contrary to the function name, the \
                    nput must be the folder containing all bin file for a given sequence");
    return;
  }

  // count number of frame inside the folder
  this->NumberOfFrames = 0;
  boost::filesystem::path folder(filename);
  boost::filesystem::directory_iterator it{folder};
  while (it != boost::filesystem::directory_iterator{})
  {
    this->NumberOfFrames++;
    *it++;
  }
  this->FileName = filename + "/";
  this->Modified();
}

//-----------------------------------------------------------------------------
void vtkLidarKITTIDataSetReader::SetNumberOfTrailingFrames(const int nbTrailingFrames)
{
  if (this->NumberOfTrailingFrames == nbTrailingFrames)
  {
    return;
  }
  this->NumberOfTrailingFrames = nbTrailingFrames;
}

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> vtkLidarKITTIDataSetReader::GetFrame(int frameNumber, int wantedNumberOfTrailingFrames)
{
  // create a new empty frame
  vtkSmartPointer<vtkPolyData> poly = vtkSmartPointer<vtkPolyData>::New();

  vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
  points->SetDataTypeToFloat();
  points->SetNumberOfPoints(0);
  points->GetData()->SetName("Points_m_XYZ");
  poly->SetPoints(points.GetPointer());

  vtkSmartPointer<vtkDoubleArray> xArray = CreateDataArray<vtkDoubleArray>("X", poly);
  vtkSmartPointer<vtkDoubleArray> yArray = CreateDataArray<vtkDoubleArray>("y", poly);
  vtkSmartPointer<vtkDoubleArray> zArray = CreateDataArray<vtkDoubleArray>("z", poly);
  vtkSmartPointer<vtkDoubleArray> intensityArray = CreateDataArray<vtkDoubleArray>("intensity", poly);
  vtkSmartPointer<vtkDoubleArray> azimutArray = CreateDataArray<vtkDoubleArray>("azimut", poly);
  vtkSmartPointer<vtkDoubleArray> elevationArray = CreateDataArray<vtkDoubleArray>("elevation", poly);
  vtkSmartPointer<vtkDoubleArray> radiusArray = CreateDataArray<vtkDoubleArray>("radius", poly);
  vtkSmartPointer<vtkDoubleArray> idArray = CreateDataArray<vtkDoubleArray>("id", poly);

  int startFrame = std::max(0, frameNumber-wantedNumberOfTrailingFrames);
  for (int i = startFrame; i <= frameNumber; i++)
  {

    // get the desire bin file
    std::stringstream ss;
    ss << std::setw(10) << std::setfill('0') << i;
    std::string filename = this->GetFileName() + ss.str() + ".bin";

    ifstream is;
    is.open(filename, ios::binary|ios::in);
    // get length of file:
    is.seekg(0, ios::end);
    int length = is.tellg();
    is.seekg(0, ios::beg);

    // variable use to detect a laser jump
    double old_azimut = 0;
    int laser_id = 1;

    // buffer used to read the points
    char buffer[16];
    const int nbPoints = length / 16;
    for (int i = 0; i < nbPoints; i++)
    {
      is.read(buffer, 16);
      point_t* pt;
      pt = reinterpret_cast<point_t*>(buffer);
      double x = pt->x;
      double y = pt->y;
      double z = pt->z;

      double radius = sqrt(x*x + y*y + z*z);

      // crop points which belong to the vehicle
      if (radius < 4)
      {
        continue;
      }


      double azimut = 180 / vtkMath::Pi() * atan2(pt->y,pt->x);
      if(old_azimut < 0 && azimut >= 0)
      {
        laser_id++;
        if (laser_id > 64)
        {
          vtkErrorMacro(<< "An error occur while parsing the frame, more than 64 laser where detected")
        }
      }
      double elevation = 180 / vtkMath::Pi() * acos(pt->z/radius);

      // fill the polydata
      points->InsertNextPoint(pt->x, pt->y, pt->z);
      xArray->InsertNextValue(pt->x);
      yArray->InsertNextValue(pt->y);
      zArray->InsertNextValue(pt->z);
      radiusArray->InsertNextValue(radius);
      intensityArray->InsertNextValue(pt->intensity);
      azimutArray->InsertNextValue(azimut);
      idArray->InsertNextValue(laser_id);
      elevationArray->InsertNextValue(elevation);

      // update old azimut
      old_azimut = azimut;
    }
    is.close();
  }

  poly->SetVerts(NewVertexCells(poly->GetNumberOfPoints()));

  return poly;
}

//----------------------------------------------------------------------------
int vtkLidarKITTIDataSetReader::GetNumberOfFrames()
{
  return this->NumberOfFrames;
}

//----------------------------------------------------------------------------
int vtkLidarKITTIDataSetReader::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkPolyData* output = vtkPolyData::GetData(outputVector);
  vtkInformation* info = outputVector->GetInformationObject(0);

  int timestep = 0;
  if (info->Has(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP()))
  {
    double timeRequest = info->Get(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP());
    timestep = static_cast<int>(floor(timeRequest + 0.5));
  }
  if (timestep < 0 || timestep >= this->GetNumberOfFrames())
  {
    vtkErrorMacro("Cannot meet timestep request: " << timestep << ".  Have "
                                                   << this->GetNumberOfFrames() << " datasets.");
    return 0;
  }
  output->ShallowCopy(GetFrame(timestep, this->NumberOfTrailingFrames));
  return 1;
}

//----------------------------------------------------------------------------
int vtkLidarKITTIDataSetReader::RequestInformation(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformation* info = outputVector->GetInformationObject(0);
  int numberOfTimesteps = this->NumberOfFrames;
  std::vector<double> timesteps;
  for (size_t i = 0; i < numberOfTimesteps; ++i)
  {
    timesteps.push_back(i);
  }

  if (numberOfTimesteps)
  {
    double timeRange[2] = { timesteps.front(), timesteps.back() };
    info->Set(vtkStreamingDemandDrivenPipeline::TIME_STEPS(), &timesteps.front(), timesteps.size());
    info->Set(vtkStreamingDemandDrivenPipeline::TIME_RANGE(), timeRange, 2);
  }
  else
  {
    info->Remove(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    info->Remove(vtkStreamingDemandDrivenPipeline::TIME_RANGE());
  }
  return 1;
}
