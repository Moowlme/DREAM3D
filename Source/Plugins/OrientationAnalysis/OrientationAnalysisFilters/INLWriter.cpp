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
#include "INLWriter.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QTextStream>

#include "SIMPLib/Common/Constants.h"
#include "SIMPLib/DataContainers/DataContainer.h"
#include "SIMPLib/DataContainers/DataContainerArray.h"
#include "SIMPLib/FilterParameters/AbstractFilterParametersReader.h"
#include "SIMPLib/FilterParameters/DataArraySelectionFilterParameter.h"
#include "SIMPLib/FilterParameters/OutputFileFilterParameter.h"
#include "SIMPLib/FilterParameters/SeparatorFilterParameter.h"
#include "SIMPLib/Geometry/ImageGeom.h"
#include "SIMPLib/Utilities/FileSystemPathHelper.h"

#include "EbsdLib/Core/EbsdLibConstants.h"
#include "EbsdLib/IO/TSL/AngConstants.h"

#include "OrientationAnalysis/OrientationAnalysisConstants.h"
#include "OrientationAnalysis/OrientationAnalysisVersion.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
INLWriter::INLWriter() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
INLWriter::~INLWriter() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void INLWriter::setupFilterParameters()
{
  FileWriter::setupFilterParameters();
  FilterParameterVectorType parameters;
  parameters.push_back(SIMPL_NEW_OUTPUT_FILE_FP("Output File", OutputFile, FilterParameter::Category::Parameter, INLWriter, "*.txt", "INL Format"));
  parameters.push_back(SeparatorFilterParameter::Create("Cell Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Int32, 1, AttributeMatrix::Type::Cell, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Feature Ids", FeatureIdsArrayPath, FilterParameter::Category::RequiredArray, INLWriter, req));
  }
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Int32, 1, AttributeMatrix::Type::Cell, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Phases", CellPhasesArrayPath, FilterParameter::Category::RequiredArray, INLWriter, req));
  }
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Float, 3, AttributeMatrix::Type::Cell, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Euler Angles", CellEulerAnglesArrayPath, FilterParameter::Category::RequiredArray, INLWriter, req));
  }
  parameters.push_back(SeparatorFilterParameter::Create("Cell Ensemble Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req =
        DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::UInt32, 1, AttributeMatrix::Type::CellEnsemble, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Crystal Structures", CrystalStructuresArrayPath, FilterParameter::Category::RequiredArray, INLWriter, req));
  }
  {
    DataArraySelectionFilterParameter::RequirementType req =
        DataArraySelectionFilterParameter::CreateRequirement(SIMPL::Defaults::AnyPrimitive, 1, AttributeMatrix::Type::CellEnsemble, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Material Names", MaterialNameArrayPath, FilterParameter::Category::RequiredArray, INLWriter, req));
  }
  {
    DataArraySelectionFilterParameter::RequirementType req =
        DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Int32, 1, AttributeMatrix::Type::CellEnsemble, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Number of Features", NumFeaturesArrayPath, FilterParameter::Category::RequiredArray, INLWriter, req));
  }
  setFilterParameters(parameters);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void INLWriter::readFilterParameters(AbstractFilterParametersReader* reader, int index)
{
  reader->openFilterGroup(this, index);
  setCellEulerAnglesArrayPath(reader->readDataArrayPath("CellEulerAnglesArrayPath", getCellEulerAnglesArrayPath()));
  setCrystalStructuresArrayPath(reader->readDataArrayPath("CrystalStructuresArrayPath", getCrystalStructuresArrayPath()));
  setCellPhasesArrayPath(reader->readDataArrayPath("CellPhasesArrayPath", getCellPhasesArrayPath()));
  setFeatureIdsArrayPath(reader->readDataArrayPath("FeatureIdsArrayPath", getFeatureIdsArrayPath()));
  setNumFeaturesArrayPath(reader->readDataArrayPath("NumFeaturesArrayPath", getNumFeaturesArrayPath()));
  setMaterialNameArrayPath(reader->readDataArrayPath("MaterialNameArrayPath", getMaterialNameArrayPath()));
  setOutputFile(reader->readString("OutputFile", getOutputFile()));
  reader->closeFilterGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void INLWriter::initialize()
{
  m_MaterialNamePtr = StringDataArray::NullPointer();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void INLWriter::dataCheck()
{
  clearErrorCode();
  clearWarningCode();

  getDataContainerArray()->getPrereqGeometryFromDataContainer<ImageGeom>(this, getFeatureIdsArrayPath().getDataContainerName());

  FileSystemPathHelper::CheckOutputFile(this, "Output File Path", getOutputFile(), true);

  QVector<DataArrayPath> cellDataArrayPaths;
  QVector<DataArrayPath> ensembleDataArrayPaths;

  std::vector<size_t> cDims(1, 1);
  m_FeatureIdsPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getFeatureIdsArrayPath(), cDims);
  if(nullptr != m_FeatureIdsPtr.lock())
  {
    m_FeatureIds = m_FeatureIdsPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
  if(getErrorCode() >= 0)
  {
    cellDataArrayPaths.push_back(getFeatureIdsArrayPath());
  }

  m_CellPhasesPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getCellPhasesArrayPath(), cDims);
  if(nullptr != m_CellPhasesPtr.lock())
  {
    m_CellPhases = m_CellPhasesPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
  if(getErrorCode() >= 0)
  {
    cellDataArrayPaths.push_back(getCellPhasesArrayPath());
  }

  m_CrystalStructuresPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<uint32_t>>(this, getCrystalStructuresArrayPath(), cDims);
  if(nullptr != m_CrystalStructuresPtr.lock())
  {
    m_CrystalStructures = m_CrystalStructuresPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
  if(getErrorCode() >= 0)
  {
    ensembleDataArrayPaths.push_back(getCrystalStructuresArrayPath());
  }

  m_NumFeaturesPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getNumFeaturesArrayPath(), cDims);
  if(nullptr != m_NumFeaturesPtr.lock())
  {
    m_NumFeatures = m_NumFeaturesPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
  if(getErrorCode() >= 0)
  {
    ensembleDataArrayPaths.push_back(getNumFeaturesArrayPath());
  }

  m_MaterialNamePtr = getDataContainerArray()->getPrereqArrayFromPath<StringDataArray>(this, getMaterialNameArrayPath(), cDims);
  if(getErrorCode() >= 0)
  {
    ensembleDataArrayPaths.push_back(getMaterialNameArrayPath());
  }

  cDims[0] = 3;
  m_CellEulerAnglesPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<float>>(this, getCellEulerAnglesArrayPath(), cDims);
  if(nullptr != m_CellEulerAnglesPtr.lock())
  {
    m_CellEulerAngles = m_CellEulerAnglesPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
  if(getErrorCode() >= 0)
  {
    cellDataArrayPaths.push_back(getCellEulerAnglesArrayPath());
  }

  getDataContainerArray()->validateNumberOfTuples(this, cellDataArrayPaths);
  getDataContainerArray()->validateNumberOfTuples(this, ensembleDataArrayPaths);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int32_t INLWriter::writeHeader()
{
  return 0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
uint32_t mapCrystalSymmetryToTslSymmetry(uint32_t symmetry)
{
  switch(symmetry)
  {
  case EbsdLib::CrystalStructure::Cubic_High:
    return EbsdLib::Ang::PhaseSymmetry::Cubic;
  case EbsdLib::CrystalStructure::Cubic_Low:
    return EbsdLib::Ang::PhaseSymmetry::Tetrahedral;
  case EbsdLib::CrystalStructure::Tetragonal_High:
    return EbsdLib::Ang::PhaseSymmetry::DiTetragonal;
  case EbsdLib::CrystalStructure::Tetragonal_Low:
    return EbsdLib::Ang::PhaseSymmetry::Tetragonal;
  case EbsdLib::CrystalStructure::OrthoRhombic:
    return EbsdLib::Ang::PhaseSymmetry::Orthorhombic;
  case EbsdLib::CrystalStructure::Monoclinic:
    return EbsdLib::Ang::PhaseSymmetry::Monoclinic_c;
    return EbsdLib::Ang::PhaseSymmetry::Monoclinic_b;
    return EbsdLib::Ang::PhaseSymmetry::Monoclinic_a;
  case EbsdLib::CrystalStructure::Triclinic:
    return EbsdLib::Ang::PhaseSymmetry::Triclinic;
  case EbsdLib::CrystalStructure::Hexagonal_High:
    return EbsdLib::Ang::PhaseSymmetry::DiHexagonal;
  case EbsdLib::CrystalStructure::Hexagonal_Low:
    return EbsdLib::Ang::PhaseSymmetry::Hexagonal;
  case EbsdLib::CrystalStructure::Trigonal_High:
    return EbsdLib::Ang::PhaseSymmetry::DiTrigonal;
  case EbsdLib::CrystalStructure::Trigonal_Low:
    return EbsdLib::Ang::PhaseSymmetry::Trigonal;
  default:
    return EbsdLib::CrystalStructure::UnknownCrystalStructure;
  }
  return EbsdLib::CrystalStructure::UnknownCrystalStructure;
}
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int32_t INLWriter::writeFile()
{
  clearErrorCode();
  clearWarningCode();
  dataCheck();
  if(getErrorCode() < 0)
  {
    return getErrorCode();
  }

  DataContainer::Pointer m = getDataContainerArray()->getDataContainer(getFeatureIdsArrayPath().getDataContainerName());

  size_t totalPoints = m_FeatureIdsPtr.lock()->getNumberOfTuples();

  int32_t err = 0;
  SizeVec3Type dims = m->getGeometryAs<ImageGeom>()->getDimensions();
  FloatVec3Type res = m->getGeometryAs<ImageGeom>()->getSpacing();
  FloatVec3Type origin = m->getGeometryAs<ImageGeom>()->getOrigin();

  // Make sure any directory path is also available as the user may have just typed
  // in a path without actually creating the full path
  QFileInfo fi(getOutputFile());
  QDir dir(fi.path());
  if(!dir.mkpath("."))
  {
    QString ss = QObject::tr("Error creating parent path '%1'").arg(fi.path());
    setErrorCondition(-1, ss);
    return -1;
  }

  FILE* f = fopen(getOutputFile().toLatin1().data(), "wb");
  if(nullptr == f)
  {
    QString ss = QObject::tr("Error opening output file '%1'").arg(getOutputFile());
    setErrorCondition(-1, ss);
    return -1;
  }

  // Write the header, Each line starts with a "#" symbol
  fprintf(f, "# File written from %s\r\n", OrientationAnalysis::Version::PackageComplete().toLatin1().data());
  fprintf(f, "# DateTime: %s\r\n", QDateTime::currentDateTime().toString().toLatin1().data());
  fprintf(f, "# X_STEP: %f\r\n", res[0]);
  fprintf(f, "# Y_STEP: %f\r\n", res[1]);
  fprintf(f, "# Z_STEP: %f\r\n", res[2]);
  fprintf(f, "#\r\n");
  fprintf(f, "# X_MIN: %f\r\n", origin[0]);
  fprintf(f, "# Y_MIN: %f\r\n", origin[1]);
  fprintf(f, "# Z_MIN: %f\r\n", origin[2]);
  fprintf(f, "#\r\n");
  fprintf(f, "# X_MAX: %f\r\n", origin[0] + (dims[0] * res[0]));
  fprintf(f, "# Y_MAX: %f\r\n", origin[1] + (dims[1] * res[1]));
  fprintf(f, "# Z_MAX: %f\r\n", origin[2] + (dims[2] * res[2]));
  fprintf(f, "#\r\n");
  fprintf(f, "# X_DIM: %llu\r\n", static_cast<long long unsigned int>(dims[0]));
  fprintf(f, "# Y_DIM: %llu\r\n", static_cast<long long unsigned int>(dims[1]));
  fprintf(f, "# Z_DIM: %llu\r\n", static_cast<long long unsigned int>(dims[2]));
  fprintf(f, "#\r\n");

  StringDataArray* materialNames = m_MaterialNamePtr.lock().get();

#if 0
  -------------------------------------------- -
#Phase_1 : MOX with 30 % Pu
#Symmetry_1 : 43
#Features_1 : 4
#
#Phase_2 : Brahman
#Symmetry_2 : 62
#Features_2 : 6
#
#Phase_3 : Void
#Symmetry_3 : 22
#Features_3 : 1
#
#Total_Features : 11
  -------------------------------------------- -
#endif

  uint32_t symmetry = 0;
  int32_t count = static_cast<int32_t>(materialNames->getNumberOfTuples());
  for(int32_t i = 1; i < count; ++i)
  {
    QString matName = materialNames->getValue(i);
    fprintf(f, "# Phase_%d: %s\r\n", i, matName.toLatin1().data());
    symmetry = m_CrystalStructures[i];
    symmetry = mapCrystalSymmetryToTslSymmetry(symmetry);
    fprintf(f, "# Symmetry_%d: %u\r\n", i, symmetry);
    fprintf(f, "# Features_%d: %d\r\n", i, m_NumFeatures[i]);
    fprintf(f, "#\r\n");
  }

  std::set<int32_t> uniqueFeatureIds;
  for(size_t i = 0; i < totalPoints; ++i)
  {
    uniqueFeatureIds.insert(m_FeatureIds[i]);
  }
  count = static_cast<int32_t>(uniqueFeatureIds.size());
  fprintf(f, "# Num_Features: %d \r\n", count);
  fprintf(f, "#\r\n");

  //  fprintf(f, "# Column 1-3: phi1, PHI, phi2 (orientation of point in radians)\r\n");
  //  fprintf(f, "# Column 4-6: x, y, z (coordinates of point in microns)\r\n");
  //  fprintf(f, "# Column 7: Feature ID\r\n");
  //  fprintf(f, "# Column 8: Phase ID\r\n");

  fprintf(f, "# phi1 PHI phi2 x y z FeatureId PhaseId Symmetry\r\n");

  float phi1 = 0.0f, phi = 0.0f, phi2 = 0.0f;
  float xPos = 0.0f, yPos = 0.0f, zPos = 0.0f;
  int32_t featureId = 0;
  int32_t phaseId = 0;

  size_t index = 0;
  for(size_t z = 0; z < dims[2]; ++z)
  {
    for(size_t y = 0; y < dims[1]; ++y)
    {
      for(size_t x = 0; x < dims[0]; ++x)
      {
        index = (z * dims[0] * dims[1]) + (dims[0] * y) + x;
        phi1 = m_CellEulerAngles[index * 3];
        phi = m_CellEulerAngles[index * 3 + 1];
        phi2 = m_CellEulerAngles[index * 3 + 2];
        xPos = origin[0] + (x * res[0]);
        yPos = origin[1] + (y * res[1]);
        zPos = origin[2] + (z * res[2]);
        featureId = m_FeatureIds[index];
        phaseId = m_CellPhases[index];
        symmetry = m_CrystalStructures[phaseId];
        if(phaseId > 0)
        {
          if(symmetry == EbsdLib::CrystalStructure::Cubic_High)
          {
            symmetry = EbsdLib::Ang::PhaseSymmetry::Cubic;
          }
          else if(symmetry == EbsdLib::CrystalStructure::Hexagonal_High)
          {
            symmetry = EbsdLib::Ang::PhaseSymmetry::DiHexagonal;
          }
          else
          {
            symmetry = EbsdLib::Ang::PhaseSymmetry::UnknownSymmetry;
          }
        }
        else
        {
          symmetry = EbsdLib::Ang::PhaseSymmetry::UnknownSymmetry;
        }

        fprintf(f, "%f %f %f %f %f %f %d %d %d\r\n", phi1, phi, phi2, xPos, yPos, zPos, featureId, phaseId, symmetry);
      }
    }
  }

  fclose(f);

  return err;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbstractFilter::Pointer INLWriter::newFilterInstance(bool copyFilterParameters) const
{
  INLWriter::Pointer filter = INLWriter::New();
  if(copyFilterParameters)
  {
    copyFilterParameterInstanceVariables(filter.get());
  }
  return filter;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString INLWriter::getCompiledLibraryName() const
{
  return OrientationAnalysisConstants::OrientationAnalysisBaseName;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString INLWriter::getBrandingString() const
{
  return "Orientation Analysis";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString INLWriter::getFilterVersion() const
{
  QString version;
  QTextStream vStream(&version);
  vStream << OrientationAnalysis::Version::Major() << "." << OrientationAnalysis::Version::Minor() << "." << OrientationAnalysis::Version::Patch();
  return version;
}
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString INLWriter::getGroupName() const
{
  return SIMPL::FilterGroups::IOFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QUuid INLWriter::getUuid() const
{
  return QUuid("{27c724cc-8b69-5ebe-b90e-29d33858a032}");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString INLWriter::getSubGroupName() const
{
  return SIMPL::FilterSubGroups::OutputFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString INLWriter::getHumanLabel() const
{
  return "Export INL File";
}

// -----------------------------------------------------------------------------
INLWriter::Pointer INLWriter::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::shared_ptr<INLWriter> INLWriter::New()
{
  struct make_shared_enabler : public INLWriter
  {
  };
  std::shared_ptr<make_shared_enabler> val = std::make_shared<make_shared_enabler>();
  val->setupFilterParameters();
  return val;
}

// -----------------------------------------------------------------------------
QString INLWriter::getNameOfClass() const
{
  return QString("INLWriter");
}

// -----------------------------------------------------------------------------
QString INLWriter::ClassName()
{
  return QString("INLWriter");
}

// -----------------------------------------------------------------------------
void INLWriter::setMaterialNameArrayPath(const DataArrayPath& value)
{
  m_MaterialNameArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath INLWriter::getMaterialNameArrayPath() const
{
  return m_MaterialNameArrayPath;
}

// -----------------------------------------------------------------------------
void INLWriter::setFeatureIdsArrayPath(const DataArrayPath& value)
{
  m_FeatureIdsArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath INLWriter::getFeatureIdsArrayPath() const
{
  return m_FeatureIdsArrayPath;
}

// -----------------------------------------------------------------------------
void INLWriter::setCellPhasesArrayPath(const DataArrayPath& value)
{
  m_CellPhasesArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath INLWriter::getCellPhasesArrayPath() const
{
  return m_CellPhasesArrayPath;
}

// -----------------------------------------------------------------------------
void INLWriter::setCrystalStructuresArrayPath(const DataArrayPath& value)
{
  m_CrystalStructuresArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath INLWriter::getCrystalStructuresArrayPath() const
{
  return m_CrystalStructuresArrayPath;
}

// -----------------------------------------------------------------------------
void INLWriter::setNumFeaturesArrayPath(const DataArrayPath& value)
{
  m_NumFeaturesArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath INLWriter::getNumFeaturesArrayPath() const
{
  return m_NumFeaturesArrayPath;
}

// -----------------------------------------------------------------------------
void INLWriter::setCellEulerAnglesArrayPath(const DataArrayPath& value)
{
  m_CellEulerAnglesArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath INLWriter::getCellEulerAnglesArrayPath() const
{
  return m_CellEulerAnglesArrayPath;
}

// -----------------------------------------------------------------------------
void INLWriter::setMaterialNameArrayName(const QString& value)
{
  m_MaterialNameArrayName = value;
}

// -----------------------------------------------------------------------------
QString INLWriter::getMaterialNameArrayName() const
{
  return m_MaterialNameArrayName;
}
