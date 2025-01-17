/* ============================================================================
 * Copyright (c) 2009-2016 BlueQuartz Software, LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * Neither the name of BlueQuartz Software, the US Air Force, nor the names of its
 * contributors may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The code contained herein was partially funded by the following contracts:
 *    United States Air Force Prime Contract FA8650-07-D-5800
 *    United States Air Force Prime Contract FA8650-10-D-5210
 *    United States Prime Contract Navy N00173-07-C-2068
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#include "AbaqusHexahedronWriter.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QTextStream>

#include "SIMPLib/Common/Constants.h"
#include "SIMPLib/DataContainers/DataContainer.h"
#include "SIMPLib/DataContainers/DataContainerArray.h"
#include "SIMPLib/FilterParameters/AbstractFilterParametersReader.h"
#include "SIMPLib/FilterParameters/DataArraySelectionFilterParameter.h"
#include "SIMPLib/FilterParameters/IntFilterParameter.h"
#include "SIMPLib/FilterParameters/OutputPathFilterParameter.h"
#include "SIMPLib/FilterParameters/SeparatorFilterParameter.h"
#include "SIMPLib/FilterParameters/StringFilterParameter.h"
#include "SIMPLib/Geometry/ImageGeom.h"
#include "SIMPLib/Utilities/TimeUtilities.h"

#include "ImportExport/ImportExportConstants.h"
#include "ImportExport/ImportExportVersion.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbaqusHexahedronWriter::AbaqusHexahedronWriter() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbaqusHexahedronWriter::~AbaqusHexahedronWriter() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::setupFilterParameters()
{
  FilterParameterVectorType parameters;
  parameters.push_back(SIMPL_NEW_INTEGER_FP("Hourglass Stiffness", HourglassStiffness, FilterParameter::Category::Parameter, AbaqusHexahedronWriter, 0));
  parameters.push_back(SIMPL_NEW_STRING_FP("Job Name", JobName, FilterParameter::Category::Parameter, AbaqusHexahedronWriter));
  parameters.push_back(SIMPL_NEW_OUTPUT_PATH_FP("Output Path", OutputPath, FilterParameter::Category::Parameter, AbaqusHexahedronWriter));
  parameters.push_back(SIMPL_NEW_STRING_FP("Output File Prefix", FilePrefix, FilterParameter::Category::Parameter, AbaqusHexahedronWriter));
  parameters.push_back(SeparatorFilterParameter::Create("Cell Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Int32, 1, AttributeMatrix::Type::Cell, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Feature Ids", FeatureIdsArrayPath, FilterParameter::Category::RequiredArray, AbaqusHexahedronWriter, req));
  }
  setFilterParameters(parameters);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::readFilterParameters(AbstractFilterParametersReader* reader, int index)
{
  reader->openFilterGroup(this, index);
  setFeatureIdsArrayPath(reader->readDataArrayPath("FeatureIdsArrayPath", getFeatureIdsArrayPath()));
  setFilePrefix(reader->readString("FilePrefix", getFilePrefix()));
  setOutputPath(reader->readString("OutputPath", getOutputPath()));
  setHourglassStiffness(reader->readValue("HourglassStiffness", getHourglassStiffness()));
  setJobName(reader->readString("JobName", getJobName()));
  reader->closeFilterGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::initialize()
{
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::dataCheck()
{
  clearErrorCode();
  clearWarningCode();

  if(m_OutputPath.isEmpty())
  {
    QString ss = QObject::tr("The output path must be set");
    setErrorCondition(-12001, ss);
  }

  QFileInfo fi(m_OutputPath);
  QDir parentPath = fi.path();
  if(!parentPath.exists())
  {
    QString ss = QObject::tr("The directory path for the output file does not exist. DREAM.3D will attempt to create this path during execution of the filter");
    setWarningCondition(-10100, ss);
  }

  std::vector<size_t> cDims(1, 1); // The component dimensions of the needed array.
  m_FeatureIdsPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getFeatureIdsArrayPath(), cDims);
  if(nullptr != m_FeatureIdsPtr.lock())
  {
    m_FeatureIds = m_FeatureIdsPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  ImageGeom::Pointer image = getDataContainerArray()->getPrereqGeometryFromDataContainer<ImageGeom>(this, getFeatureIdsArrayPath().getDataContainerName());
  if(getErrorCode() < 0)
  {
    return;
  }

  size_t volTuples = image->getNumberOfElements();

  if(volTuples != m_FeatureIdsPtr.lock()->getNumberOfTuples())
  {
    QString ss = QObject::tr("The number of Tuples for the DataArray %1 is %2 and for the associated Image Geometry is %3. The number of tuples must match")
                     .arg(m_FeatureIdsPtr.lock()->getName())
                     .arg(m_FeatureIdsPtr.lock()->getNumberOfTuples())
                     .arg(volTuples);
    setErrorCondition(-10200, ss);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::execute()
{
  dataCheck();
  if(getErrorCode() < 0)
  {
    return;
  }

  // Check Output Path
  QDir dir;
  if(!dir.mkpath(m_OutputPath))
  {
    QString ss = QObject::tr("Error creating parent path '%1'").arg(m_OutputPath);
    setErrorCondition(-1, ss);
    return;
  }

  DataContainer::Pointer r = getDataContainerArray()->getDataContainer(m_FeatureIdsArrayPath.getDataContainerName());

  SizeVec3Type cDims = r->getGeometryAs<ImageGeom>()->getDimensions();
  size_t pDims[3] = {cDims[0] + 1, cDims[1] + 1, cDims[2] + 1};
  FloatVec3Type origin = r->getGeometryAs<ImageGeom>()->getOrigin();
  FloatVec3Type spacing = r->getGeometryAs<ImageGeom>()->getSpacing();
  size_t totalPoints = r->getGeometryAs<ImageGeom>()->getNumberOfElements();

  // Create file names
  QString nodesFile = m_OutputPath + QDir::separator() + m_FilePrefix + "_nodes.inp";
  QString elemsFile = m_OutputPath + QDir::separator() + m_FilePrefix + "_elems.inp";
  QString sectsFile = m_OutputPath + QDir::separator() + m_FilePrefix + "_sects.inp";
  QString elsetFile = m_OutputPath + QDir::separator() + m_FilePrefix + "_elset.inp";
  QString masterFile = m_OutputPath + QDir::separator() + m_FilePrefix + ".inp";
  QList<QString> fileNames;
  fileNames << nodesFile << elemsFile << sectsFile << elsetFile << masterFile;

  int32_t err = writeNodes(fileNames, cDims.data(), origin.data(), spacing.data()); // Nodes file
  if(err < 0)
  {
    QString ss = QObject::tr("Error writing output nodes file '%1'").arg(nodesFile);
    setErrorCondition(-1, ss);
    return;
  }
  if(getCancel()) // Filter has been cancelled
  {
    deleteFile(fileNames); // delete files
    return;
  }

  err = writeElems(fileNames, cDims.data(), pDims); // Elements file
  if(err < 0)
  {
    QString ss = QObject::tr("Error writing output elems file '%1'").arg(elemsFile);
    setErrorCondition(-1, ss);
    return;
  }
  if(getCancel()) // Filter has been cancelled
  {
    deleteFile(fileNames); // delete files
    return;
  }

  err = writeSects(sectsFile, totalPoints); // Sections file
  if(err < 0)
  {
    QString ss = QObject::tr("Error writing output sects file '%1'").arg(sectsFile);
    setErrorCondition(-1, ss);
    return;
  }
  if(getCancel()) // Filter has been cancelled
  {
    deleteFile(fileNames); // delete files
    return;
  }

  err = writeElset(fileNames, totalPoints); // Element set file
  if(err < 0)
  {
    QString ss = QObject::tr("Error writing output elset file '%1'").arg(elsetFile);
    setErrorCondition(-1, ss);
    return;
  }
  if(getCancel()) // Filter has been cancelled
  {
    deleteFile(fileNames); // delete files
    return;
  }

  err = writeMaster(masterFile); // Master file
  if(err < 0)
  {
    QString ss = QObject::tr("Error writing output master file '%1'").arg(masterFile);
    setErrorCondition(-1, ss);
    return;
  }
  if(getCancel()) // Filter has been cancelled
  {
    deleteFile(fileNames); // delete files
    return;
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int32_t AbaqusHexahedronWriter::writeNodes(const QList<QString>& fileNames, size_t* cDims, float* origin, float* spacing)
{
  uint64_t millis = QDateTime::currentMSecsSinceEpoch();
  uint64_t currentMillis = millis;
  uint64_t startMillis = millis;
  uint64_t estimatedTime = 0;
  float timeDiff = 0.0f;
  QString buf;
  QTextStream ss(&buf);

  size_t pDims[3] = {cDims[0] + 1, cDims[1] + 1, cDims[2] + 1};
  size_t nodeIndex = 1;
  size_t totalPoints = pDims[0] * pDims[1] * pDims[2];
  size_t increment = static_cast<size_t>(totalPoints * 0.01f);
  if(increment == 0) // check to prevent divide by 0
  {
    increment = 1;
  }

  int32_t err = 0;
  FILE* f = nullptr;
  f = fopen(fileNames.at(0).toLatin1().data(), "wb");
  if(nullptr == f)
  {
    return -1;
  }

  fprintf(f, "** Generated by : %s\n", ImportExport::Version::PackageComplete().toLatin1().data());
  fprintf(f, "** ----------------------------------------------------------------\n**\n*Node\n");

  for(size_t z = 0; z < pDims[2]; z++)
  {
    for(size_t y = 0; y < pDims[1]; y++)
    {
      for(size_t x = 0; x < pDims[0]; x++)
      {
        float xCoord = origin[0] + (x * spacing[0]);
        float yCoord = origin[1] + (y * spacing[1]);
        float zCoord = origin[2] + (z * spacing[2]);
        fprintf(f, "%llu, %f, %f, %f\n", static_cast<unsigned long long int>(nodeIndex), xCoord, yCoord, zCoord);
        if(nodeIndex % increment == 0)
        {
          currentMillis = QDateTime::currentMSecsSinceEpoch();
          if(currentMillis - millis > 1000)
          {
            buf.clear();
            ss << "Writing Nodes (File 1/5) " << static_cast<int>((float)(nodeIndex) / (float)(totalPoints)*100) << "% Completed ";
            timeDiff = ((float)nodeIndex / (float)(currentMillis - startMillis));
            estimatedTime = (float)(totalPoints - nodeIndex) / timeDiff;
            ss << " || Est. Time Remain: " << DREAM3D::convertMillisToHrsMinSecs(estimatedTime);
            notifyStatusMessage(buf);
            millis = QDateTime::currentMSecsSinceEpoch();
            if(getCancel()) // Filter has been cancelled
            {
              fclose(f);
              return 1;
            }
          }
        }
        ++nodeIndex;
      }
    }
  }

  // Write the last node, which is a dummy node used for stress - strain curves.
  fprintf(f, "%d, %f, %f, %f\n", 999999, 0.0f, 0.0f, 0.0f);
  fprintf(f, "**\n** ----------------------------------------------------------------\n**\n");

  // Close the file
  notifyStatusMessage("Writing Nodes (File 1/5) Complete");
  fclose(f);
  return err;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int32_t AbaqusHexahedronWriter::writeElems(const QList<QString>& fileNames, size_t* cDims, size_t* pDims)
{
  uint64_t millis = QDateTime::currentMSecsSinceEpoch();
  uint64_t currentMillis = millis;
  uint64_t startMillis = millis;
  uint64_t estimatedTime = 0;
  float timeDiff = 0.0f;
  QString buf;
  QTextStream ss(&buf);
  size_t totalPoints = cDims[0] * cDims[1] * cDims[2];
  size_t increment = static_cast<size_t>(totalPoints * 0.01f);
  if(increment == 0) // check to prevent divide by 0
  {
    increment = 1;
  }

  int32_t err = 0;
  FILE* f = nullptr;
  f = fopen(fileNames.at(1).toLatin1().data(), "wb");
  if(nullptr == f)
  {
    return -1;
  }

  // Get a typedef to shorten up the printf codes
  using _lli_t_ = long long int;

  size_t index = 1;
  fprintf(f, "** Generated by : %s\n", ImportExport::Version::PackageComplete().toLatin1().data());
  fprintf(f, "** ----------------------------------------------------------------\n**\n*Element, type=C3D8\n");
  for(size_t z = 0; z < cDims[2]; z++)
  {
    for(size_t y = 0; y < cDims[1]; y++)
    {
      for(size_t x = 0; x < cDims[0]; x++)
      {
        std::vector<int64_t> nodeId = getNodeIds(x, y, z, pDims);
        fprintf(f, "%llu, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld\n", (_lli_t_)index, (_lli_t_)nodeId[5], (_lli_t_)nodeId[1], (_lli_t_)nodeId[0], (_lli_t_)nodeId[4], (_lli_t_)nodeId[7],
                (_lli_t_)nodeId[3], (_lli_t_)nodeId[2], (_lli_t_)nodeId[6]);
        if(index % increment == 0)
        {
          currentMillis = QDateTime::currentMSecsSinceEpoch();
          if(currentMillis - millis > 1000)
          {
            buf.clear();
            ss << "Writing Elements (File 2/5) " << static_cast<int>((float)(index) / (float)(totalPoints)*100) << "% Completed ";
            timeDiff = ((float)index / (float)(currentMillis - startMillis));
            estimatedTime = (float)(totalPoints - index) / timeDiff;
            ss << " || Est. Time Remain: " << DREAM3D::convertMillisToHrsMinSecs(estimatedTime);
            notifyStatusMessage(buf);
            millis = QDateTime::currentMSecsSinceEpoch();
            if(getCancel()) // Filter has been cancelled
            {
              fclose(f);
              return 1;
            }
          }
        }
        ++index;
      }
    }
  }

  fprintf(f, "**\n** ----------------------------------------------------------------\n**\n");

  // Close the file
  notifyStatusMessage("Writing Elements (File 2/5) Complete");
  fclose(f);
  return err;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int32_t AbaqusHexahedronWriter::writeElset(const QList<QString>& fileNames, size_t totalPoints)
{
  uint64_t millis = QDateTime::currentMSecsSinceEpoch();
  uint64_t currentMillis = millis;
  uint64_t startMillis = millis;
  uint64_t estimatedTime = 0;
  float timeDiff = 0.0f;
  QString buf;
  QTextStream ss(&buf);

  int32_t err = 0;
  FILE* f = nullptr;
  f = fopen(fileNames.at(3).toLatin1().data(), "wb");
  if(nullptr == f)
  {
    return -1;
  }

  fprintf(f, "** Generated by : %s\n", ImportExport::Version::PackageComplete().toLatin1().data());
  fprintf(f, "** ----------------------------------------------------------------\n**\n** The element sets\n");
  fprintf(f, "*Elset, elset=cube, generate\n");
  fprintf(f, "1, %llu, 1\n", static_cast<unsigned long long int>(totalPoints));
  fprintf(f, "**\n** Each Grain is made up of multiple elements\n**");
  notifyStatusMessage(("Writing Element Sets (File 4/5) 1% Completed || Est. Time Remain: "));

  // find total number of Grain Ids
  int32_t maxGrainId = 0;
  for(size_t i = 0; i < totalPoints; i++) // find number of grainIds
  {
    if(m_FeatureIds[i] > maxGrainId)
    {
      maxGrainId = m_FeatureIds[i];
    }
  }

  int32_t increment = static_cast<int32_t>(maxGrainId * 0.1f);
  if(increment == 0) // check to prevent divide by 0
  {
    increment = 1;
  }

  int32_t voxelId = 1;
  while(voxelId <= maxGrainId)
  {
    size_t elementPerLine = 0;
    fprintf(f, "\n*Elset, elset=Grain%d_set\n", voxelId);

    for(size_t i = 0; i < totalPoints + 1; i++)
    {
      if(m_FeatureIds[i] == voxelId)
      {
        if(elementPerLine != 0) // no comma at start
        {
          if((elementPerLine % 16) != 0u) // 16 per line
          {
            fprintf(f, ", ");
          }
          else
          {
            fprintf(f, ",\n");
          }
        }
        fprintf(f, "%llu", static_cast<unsigned long long int>(i + 1));
        elementPerLine++;
      }
    }
    if(voxelId % increment == 0)
    {
      currentMillis = QDateTime::currentMSecsSinceEpoch();
      if(currentMillis - millis > 1000)
      {
        buf.clear();
        ss << "Writing Element Sets (File 4/5) " << static_cast<int>((float)(voxelId) / (float)(maxGrainId)*100) << "% Completed ";
        timeDiff = ((float)voxelId / (float)(currentMillis - startMillis));
        estimatedTime = (float)(maxGrainId - voxelId) / timeDiff;
        ss << " || Est. Time Remain: " << DREAM3D::convertMillisToHrsMinSecs(estimatedTime);
        notifyStatusMessage(buf);
        millis = QDateTime::currentMSecsSinceEpoch();
        if(getCancel()) // Filter has been cancelled
        {
          fclose(f);
          return 1;
        }
      }
    }
    voxelId++;
  }
  fprintf(f, "\n**\n** ----------------------------------------------------------------\n**\n");

  // Close the file
  notifyStatusMessage("Writing Element Sets (File 4/5) Complete");
  fclose(f);
  return err;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int32_t AbaqusHexahedronWriter::writeMaster(const QString& file)
{
  int32_t err = 0;
  FILE* f = nullptr;
  f = fopen(file.toLatin1().data(), "wb");
  if(nullptr == f)
  {
    return -1;
  }

  fprintf(f, "*Heading\n");
  fprintf(f, "%s\n", m_JobName.toLatin1().data());
  fprintf(f, "** Job name : %s\n", m_JobName.toLatin1().data());
  fprintf(f, "** Generated by : %s\n", ImportExport::Version::PackageComplete().toLatin1().data());
  fprintf(f, "*Preprint, echo = NO, model = NO, history = NO, contact = NO\n");
  fprintf(f, "**\n** ----------------------------Geometry----------------------------\n**\n");
  fprintf(f, "*Include, Input = %s\n", (m_FilePrefix + "_nodes.inp").toLatin1().data());
  fprintf(f, "*Include, Input = %s\n", (m_FilePrefix + "_elems.inp").toLatin1().data());
  fprintf(f, "*Include, Input = %s\n", (m_FilePrefix + "_elset.inp").toLatin1().data());
  fprintf(f, "*Include, Input = %s\n", (m_FilePrefix + "_sects.inp").toLatin1().data());
  fprintf(f, "**\n** ----------------------------------------------------------------\n**\n");

  // Close the file
  notifyStatusMessage("Writing Master (File 5/5) Complete");
  fclose(f);
  return err;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int32_t AbaqusHexahedronWriter::writeSects(const QString& file, size_t totalPoints)
{
  int32_t err = 0;
  FILE* f = nullptr;
  f = fopen(file.toLatin1().data(), "wb");
  if(nullptr == f)
  {
    return -1;
  }
  fprintf(f, "** Generated by : %s\n", ImportExport::Version::PackageComplete().toLatin1().data());
  fprintf(f, "** ----------------------------------------------------------------\n**\n** Each section is a separate grain\n");

  // find total number of Grain Ids
  int32_t maxGrainId = 0;
  for(size_t i = 0; i < totalPoints; i++)
  {
    if(m_FeatureIds[i] > maxGrainId)
    {
      maxGrainId = m_FeatureIds[i];
    }
  }

  // We are now defining the sections, which is for each grain
  int32_t grain = 1;
  while(grain <= maxGrainId)
  {
    fprintf(f, "** Section: Grain%d\n", grain);
    fprintf(f, "*Solid Section, elset=Grain%d_set, material=Grain_Mat%d\n", grain, grain);
    fprintf(f, "*Hourglass Stiffness\n%d\n", m_HourglassStiffness);
    fprintf(f, "** --------------------------------------\n");
    grain++;
  }
  fprintf(f, "**\n** ----------------------------------------------------------------\n**\n");

  // Close the file
  notifyStatusMessage("Writing Sections (File 3/5) Complete");
  fclose(f);
  return err;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
std::vector<int64_t> AbaqusHexahedronWriter::getNodeIds(size_t x, size_t y, size_t z, size_t* pDims)
{
  std::vector<int64_t> nodeId(8, 0);

  nodeId[0] = static_cast<int64_t>(1 + (pDims[0] * pDims[1] * z) + (pDims[0] * y) + x);
  nodeId[1] = static_cast<int64_t>(1 + (pDims[0] * pDims[1] * z) + (pDims[0] * y) + (x + 1));
  nodeId[2] = static_cast<int64_t>(1 + (pDims[0] * pDims[1] * z) + (pDims[0] * (y + 1)) + x);
  nodeId[3] = static_cast<int64_t>(1 + (pDims[0] * pDims[1] * z) + (pDims[0] * (y + 1)) + (x + 1));

  nodeId[4] = static_cast<int64_t>(1 + (pDims[0] * pDims[1] * (z + 1)) + (pDims[0] * y) + x);
  nodeId[5] = static_cast<int64_t>(1 + (pDims[0] * pDims[1] * (z + 1)) + (pDims[0] * y) + (x + 1));
  nodeId[6] = static_cast<int64_t>(1 + (pDims[0] * pDims[1] * (z + 1)) + (pDims[0] * (y + 1)) + x);
  nodeId[7] = static_cast<int64_t>(1 + (pDims[0] * pDims[1] * (z + 1)) + (pDims[0] * (y + 1)) + (x + 1));

#if 0
  {
    printf("           %lld-------%lld  \n", static_cast<long long int>(nodeId[4]), static_cast<long long int>(nodeId[5]));
    printf("            /|        /|   \n");
    printf("           / |       / |   \n");
    printf("          /  |      /  |   \n");
    printf("       %lld--------%lld  |   \n", static_cast<long long int>(nodeId[6]), static_cast<long long int>(nodeId[7]));
    printf("         |   |      |  |   \n");
    printf("         | %lld------|-%lld  \n", static_cast<long long int>(nodeId[0]), static_cast<long long int>(nodeId[1]));
    printf("         |  /       | /    \n");
    printf("         | /        |/     \n");
    printf("        %lld--------%lld     \n", static_cast<long long int>(nodeId[2]), static_cast<long long int>(nodeId[3]));
#endif
  return nodeId;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::deleteFile(const QList<QString>& fileNames)
{
  for(int i = 0; i < fileNames.size(); i++)
  {
    QFileInfo fi(fileNames.at(i));
    if(fi.exists())
    {
      QFile::remove(fileNames.at(i));
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbstractFilter::Pointer AbaqusHexahedronWriter::newFilterInstance(bool copyFilterParameters) const
{
  AbaqusHexahedronWriter::Pointer filter = AbaqusHexahedronWriter::New();
  if(copyFilterParameters)
  {
    copyFilterParameterInstanceVariables(filter.get());
  }
  return filter;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::getCompiledLibraryName() const
{
  return ImportExportConstants::ImportExportBaseName;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::getBrandingString() const
{
  return "IO";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::getFilterVersion() const
{
  QString version;
  QTextStream vStream(&version);
  vStream << ImportExport::Version::Major() << "." << ImportExport::Version::Minor() << "." << ImportExport::Version::Patch();
  return version;
}
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::getGroupName() const
{
  return SIMPL::FilterGroups::IOFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QUuid AbaqusHexahedronWriter::getUuid() const
{
  return QUuid("{0559aa37-c5ad-549a-82d4-bff4bfcb6cc6}");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::getSubGroupName() const
{
  return SIMPL::FilterSubGroups::OutputFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::getHumanLabel() const
{
  return "Abaqus Hexahedron Exporter";
}

// -----------------------------------------------------------------------------
AbaqusHexahedronWriter::Pointer AbaqusHexahedronWriter::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::shared_ptr<AbaqusHexahedronWriter> AbaqusHexahedronWriter::New()
{
  struct make_shared_enabler : public AbaqusHexahedronWriter
  {
  };
  std::shared_ptr<make_shared_enabler> val = std::make_shared<make_shared_enabler>();
  val->setupFilterParameters();
  return val;
}

// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::getNameOfClass() const
{
  return QString("AbaqusHexahedronWriter");
}

// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::ClassName()
{
  return QString("AbaqusHexahedronWriter");
}

// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::setOutputPath(const QString& value)
{
  m_OutputPath = value;
}

// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::getOutputPath() const
{
  return m_OutputPath;
}

// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::setFilePrefix(const QString& value)
{
  m_FilePrefix = value;
}

// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::getFilePrefix() const
{
  return m_FilePrefix;
}

// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::setFeatureIdsArrayPath(const DataArrayPath& value)
{
  m_FeatureIdsArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath AbaqusHexahedronWriter::getFeatureIdsArrayPath() const
{
  return m_FeatureIdsArrayPath;
}

// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::setHourglassStiffness(int value)
{
  m_HourglassStiffness = value;
}

// -----------------------------------------------------------------------------
int AbaqusHexahedronWriter::getHourglassStiffness() const
{
  return m_HourglassStiffness;
}

// -----------------------------------------------------------------------------
void AbaqusHexahedronWriter::setJobName(const QString& value)
{
  m_JobName = value;
}

// -----------------------------------------------------------------------------
QString AbaqusHexahedronWriter::getJobName() const
{
  return m_JobName;
}
