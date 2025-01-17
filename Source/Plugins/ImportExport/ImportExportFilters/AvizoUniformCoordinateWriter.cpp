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
#include "AvizoUniformCoordinateWriter.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include "ImportExport/ImportExportConstants.h"
#include "ImportExport/ImportExportVersion.h"

#include <QtCore/QTextStream>

#include "SIMPLib/DataContainers/DataContainer.h"
#include "SIMPLib/DataContainers/DataContainerArray.h"
#include "SIMPLib/FilterParameters/AbstractFilterParametersReader.h"
#include "SIMPLib/FilterParameters/BooleanFilterParameter.h"
#include "SIMPLib/FilterParameters/DataArraySelectionFilterParameter.h"
#include "SIMPLib/FilterParameters/OutputFileFilterParameter.h"
#include "SIMPLib/FilterParameters/SeparatorFilterParameter.h"
#include "SIMPLib/FilterParameters/StringFilterParameter.h"
#include "SIMPLib/Geometry/ImageGeom.h"
#include "SIMPLib/Utilities/FileSystemPathHelper.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AvizoUniformCoordinateWriter::AvizoUniformCoordinateWriter() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AvizoUniformCoordinateWriter::~AvizoUniformCoordinateWriter() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::setupFilterParameters()
{
  FilterParameterVectorType parameters;

  parameters.push_back(SIMPL_NEW_OUTPUT_FILE_FP("Output File", OutputFile, FilterParameter::Category::Parameter, AvizoUniformCoordinateWriter, "*.am", "Amira Mesh"));
  parameters.push_back(SIMPL_NEW_BOOL_FP("Write Binary File", WriteBinaryFile, FilterParameter::Category::Parameter, AvizoUniformCoordinateWriter));
  {
    DataArraySelectionFilterParameter::RequirementType req;
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("FeatureIds", FeatureIdsArrayPath, FilterParameter::Category::RequiredArray, AvizoUniformCoordinateWriter, req));
  }
  parameters.push_back(SIMPL_NEW_STRING_FP("Units", Units, FilterParameter::Category::Parameter, AvizoUniformCoordinateWriter, 0));

  setFilterParameters(parameters);
}

// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::readFilterParameters(AbstractFilterParametersReader* reader, int index)
{
  reader->openFilterGroup(this, index);
  setFeatureIdsArrayPath(reader->readDataArrayPath("FeatureIdsArrayPath", getFeatureIdsArrayPath()));
  setOutputFile(reader->readString("OutputFile", getOutputFile()));
  setWriteBinaryFile(reader->readValue("WriteBinaryFile", getWriteBinaryFile()));
  reader->closeFilterGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::initialize()
{
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::dataCheck()
{
  clearErrorCode();
  clearWarningCode();

  DataContainer::Pointer dc = getDataContainerArray()->getPrereqDataContainer(this, getFeatureIdsArrayPath().getDataContainerName(), false);
  if(getErrorCode() < 0 || nullptr == dc.get())
  {
    return;
  }

  ImageGeom::Pointer image = dc->getPrereqGeometry<ImageGeom>(this);
  if(getErrorCode() < 0 || nullptr == image.get())
  {
    return;
  }

  FileSystemPathHelper::CheckOutputFile(this, "Output File Path", getOutputFile(), true);

  if(m_WriteFeatureIds)
  {
    std::vector<size_t> dims(1, 1);
    m_FeatureIdsPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getFeatureIdsArrayPath(), dims);
    if(nullptr != m_FeatureIdsPtr.lock())
    {
      m_FeatureIds = m_FeatureIdsPtr.lock()->getPointer(0);
    } /* Now assign the raw pointer to data from the DataArray<T> object */
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::execute()
{
  dataCheck();
  if(getErrorCode() < 0)
  {
    return;
  }

  // Make sure any directory path is also available as the user may have just typed
  // in a path without actually creating the full path
  QFileInfo fi(m_OutputFile);
  QString parentPath = fi.path();
  QDir dir;
  if(!dir.mkpath(parentPath))
  {
    QString ss = QObject::tr("Error creating parent path '%1'").arg(parentPath);
    setErrorCondition(-1, ss);
    return;
  }

  FILE* avizoFile = fopen(getOutputFile().toLatin1().data(), "wb");
  if(nullptr == avizoFile)
  {
    QString ss = QObject::tr("Error creating file '%1'").arg(getOutputFile());
    setErrorCondition(-93001, ss);
    return;
  }

  generateHeader(avizoFile);

  int err = writeData(avizoFile);
  if(err < 0)
  {
    QString ss = QObject::tr("Error writing file '%1'").arg(getOutputFile());
    setErrorCondition(-93002, ss);
  }
  fclose(avizoFile);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::generateHeader(FILE* f)
{
  if(m_WriteBinaryFile)
  {
#ifdef CMP_WORDS_BIGENDIAN
    fprintf(f, "# AmiraMesh BINARY 2.1\n");
#else
    fprintf(f, "# AmiraMesh BINARY-LITTLE-ENDIAN 2.1\n");
#endif
  }
  else
  {
    fprintf(f, "# AmiraMesh 3D ASCII 2.0\n");
  }
  fprintf(f, "\n");
  fprintf(f, "# Dimensions in x-, y-, and z-direction\n");
  SizeVec3Type dims = getDataContainerArray()->getDataContainer(m_FeatureIdsArrayPath.getDataContainerName())->getGeometryAs<ImageGeom>()->getDimensions();

  fprintf(f, "define Lattice %llu %llu %llu\n", static_cast<unsigned long long>(dims[0]), static_cast<unsigned long long>(dims[1]), static_cast<unsigned long long>(dims[2]));

  fprintf(f, "Parameters {\n");
  fprintf(f, "     DREAM3DParams {\n");
  fprintf(f, "         Author \"DREAM.3D %s\",\n", ImportExport::Version::PackageComplete().toLatin1().data());
  fprintf(f, "         DateTime \"%s\"\n", QDateTime::currentDateTime().toString().toLatin1().data());
  fprintf(f, "         FeatureIds Path \"%s\"\n", getFeatureIdsArrayPath().serialize("/").toLatin1().data());
  fprintf(f, "     }\n");

  fprintf(f, "     Units {\n");
  fprintf(f, "         Coordinates \"%s\"\n", getUnits().toLatin1().data());
  fprintf(f, "     }\n");

  fprintf(f, "     Content \"%llux%llux%llu int, uniform coordinates\",\n", static_cast<unsigned long long int>(dims[0]), static_cast<unsigned long long int>(dims[1]),
          static_cast<unsigned long long int>(dims[2]));

  FloatVec3Type origin = getDataContainerArray()->getDataContainer(m_FeatureIdsArrayPath.getDataContainerName())->getGeometryAs<ImageGeom>()->getOrigin();
  FloatVec3Type res = getDataContainerArray()->getDataContainer(m_FeatureIdsArrayPath.getDataContainerName())->getGeometryAs<ImageGeom>()->getSpacing();
  fprintf(f, "     # Bounding Box is xmin xmax ymin ymax zmin zmax\n");
  fprintf(f, "     BoundingBox %f %f %f %f %f %f\n", origin[0], origin[0] + (res[0] * dims[0]), origin[1], origin[1] + (res[1] * dims[1]), origin[2], origin[2] + (res[2] * dims[2]));

  fprintf(f, "     CoordType \"uniform\"\n");
  fprintf(f, "}\n\n");

  fprintf(f, "Lattice { int FeatureIds } = @1\n");

  fprintf(f, "# Data section follows\n");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int AvizoUniformCoordinateWriter::writeData(FILE* f)
{
  QString start("@1\n");
  fprintf(f, "%s", start.toLatin1().data());

  size_t totalPoints = m_FeatureIdsPtr.lock()->getNumberOfTuples();

  if(m_WriteBinaryFile)
  {
    fwrite(m_FeatureIds, sizeof(int32_t), totalPoints, f);
  }
  else
  {
    // The "20 Items" is purely arbitrary and is put in to try and save some space in the ASCII file
    int count = 0;
    for(size_t i = 0; i < totalPoints; ++i)
    {
      fprintf(f, "%d", m_FeatureIds[i]);
      if(count < 20)
      {
        fprintf(f, " ");
        count++;
      }
      else
      {
        fprintf(f, "\n");
        count = 0;
      }
    }
  }
  fprintf(f, "\n");
  return 1;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbstractFilter::Pointer AvizoUniformCoordinateWriter::newFilterInstance(bool copyFilterParameters) const
{
  AvizoUniformCoordinateWriter::Pointer filter = AvizoUniformCoordinateWriter::New();
  if(copyFilterParameters)
  {
    copyFilterParameterInstanceVariables(filter.get());
  }
  return filter;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AvizoUniformCoordinateWriter::getCompiledLibraryName() const
{
  return ImportExportConstants::ImportExportBaseName;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AvizoUniformCoordinateWriter::getBrandingString() const
{
  return "IO";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AvizoUniformCoordinateWriter::getFilterVersion() const
{
  QString version;
  QTextStream vStream(&version);
  vStream << ImportExport::Version::Major() << "." << ImportExport::Version::Minor() << "." << ImportExport::Version::Patch();
  return version;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AvizoUniformCoordinateWriter::getGroupName() const
{
  return SIMPL::FilterGroups::IOFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QUuid AvizoUniformCoordinateWriter::getUuid() const
{
  return QUuid("{339f1349-9236-5023-9a56-c82fb8eafd12}");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AvizoUniformCoordinateWriter::getSubGroupName() const
{
  return SIMPL::FilterSubGroups::OutputFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AvizoUniformCoordinateWriter::getHumanLabel() const
{
  return "Avizo Uniform Coordinate Exporter";
}

// -----------------------------------------------------------------------------
AvizoUniformCoordinateWriter::Pointer AvizoUniformCoordinateWriter::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::shared_ptr<AvizoUniformCoordinateWriter> AvizoUniformCoordinateWriter::New()
{
  struct make_shared_enabler : public AvizoUniformCoordinateWriter
  {
  };
  std::shared_ptr<make_shared_enabler> val = std::make_shared<make_shared_enabler>();
  val->setupFilterParameters();
  return val;
}

// -----------------------------------------------------------------------------
QString AvizoUniformCoordinateWriter::getNameOfClass() const
{
  return QString("AvizoUniformCoordinateWriter");
}

// -----------------------------------------------------------------------------
QString AvizoUniformCoordinateWriter::ClassName()
{
  return QString("AvizoUniformCoordinateWriter");
}

// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::setOutputFile(const QString& value)
{
  m_OutputFile = value;
}

// -----------------------------------------------------------------------------
QString AvizoUniformCoordinateWriter::getOutputFile() const
{
  return m_OutputFile;
}

// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::setWriteBinaryFile(bool value)
{
  m_WriteBinaryFile = value;
}

// -----------------------------------------------------------------------------
bool AvizoUniformCoordinateWriter::getWriteBinaryFile() const
{
  return m_WriteBinaryFile;
}

// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::setUnits(const QString& value)
{
  m_Units = value;
}

// -----------------------------------------------------------------------------
QString AvizoUniformCoordinateWriter::getUnits() const
{
  return m_Units;
}

// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::setWriteFeatureIds(bool value)
{
  m_WriteFeatureIds = value;
}

// -----------------------------------------------------------------------------
bool AvizoUniformCoordinateWriter::getWriteFeatureIds() const
{
  return m_WriteFeatureIds;
}

// -----------------------------------------------------------------------------
void AvizoUniformCoordinateWriter::setFeatureIdsArrayPath(const DataArrayPath& value)
{
  m_FeatureIdsArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath AvizoUniformCoordinateWriter::getFeatureIdsArrayPath() const
{
  return m_FeatureIdsArrayPath;
}
