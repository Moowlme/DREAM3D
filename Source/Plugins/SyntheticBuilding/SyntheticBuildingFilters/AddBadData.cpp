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
#include "AddBadData.h"

#include <QtCore/QTextStream>

#include "SIMPLib/Common/Constants.h"
#include "SIMPLib/DataContainers/DataContainer.h"
#include "SIMPLib/DataContainers/DataContainerArray.h"
#include "SIMPLib/FilterParameters/AbstractFilterParametersReader.h"
#include "SIMPLib/FilterParameters/DataArraySelectionFilterParameter.h"
#include "SIMPLib/FilterParameters/FloatFilterParameter.h"
#include "SIMPLib/FilterParameters/LinkedBooleanFilterParameter.h"
#include "SIMPLib/FilterParameters/SeparatorFilterParameter.h"
#include "SIMPLib/Geometry/ImageGeom.h"
#include "SIMPLib/Math/SIMPLibRandom.h"

#include "SyntheticBuilding/SyntheticBuildingConstants.h"
#include "SyntheticBuilding/SyntheticBuildingVersion.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AddBadData::AddBadData() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AddBadData::~AddBadData() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AddBadData::setupFilterParameters()
{
  FilterParameterVectorType parameters;
  std::vector<QString> linkedProps = {"PoissonVolFraction"};
  parameters.push_back(SIMPL_NEW_LINKED_BOOL_FP("Add Random Noise", PoissonNoise, FilterParameter::Category::Parameter, AddBadData, linkedProps));
  parameters.push_back(SIMPL_NEW_FLOAT_FP("Volume Fraction of Random Noise", PoissonVolFraction, FilterParameter::Category::Parameter, AddBadData));
  linkedProps.clear();
  linkedProps.push_back("BoundaryVolFraction");
  parameters.push_back(SIMPL_NEW_LINKED_BOOL_FP("Add Boundary Noise", BoundaryNoise, FilterParameter::Category::Parameter, AddBadData, linkedProps));
  parameters.push_back(SIMPL_NEW_FLOAT_FP("Volume Fraction of Boundary Noise", BoundaryVolFraction, FilterParameter::Category::Parameter, AddBadData));
  parameters.push_back(SeparatorFilterParameter::Create("Cell Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Int32, 1, AttributeMatrix::Type::Cell, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Boundary Euclidean Distances", GBEuclideanDistancesArrayPath, FilterParameter::Category::RequiredArray, AddBadData, req));
  }
  setFilterParameters(parameters);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AddBadData::readFilterParameters(AbstractFilterParametersReader* reader, int index)
{
  reader->openFilterGroup(this, index);
  setGBEuclideanDistancesArrayPath(reader->readDataArrayPath("GBEuclideanDistancesArrayPath", getGBEuclideanDistancesArrayPath()));
  setPoissonNoise(reader->readValue("PoissonNoise", getPoissonNoise()));
  setPoissonVolFraction(reader->readValue("PoissonVolFraction", getPoissonVolFraction()));
  setBoundaryNoise(reader->readValue("BoundaryNoise", getBoundaryNoise()));
  setBoundaryVolFraction(reader->readValue("BoundaryVolFraction", getBoundaryVolFraction()));
  reader->closeFilterGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AddBadData::initialize()
{
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AddBadData::dataCheck()
{
  clearErrorCode();
  clearWarningCode();

  getDataContainerArray()->getPrereqGeometryFromDataContainer<ImageGeom>(this, getGBEuclideanDistancesArrayPath().getDataContainerName());

  if((!m_PoissonNoise) && (!m_BoundaryNoise))
  {
    QString ss = QObject::tr("At least one type of noise must be selected");
    setErrorCondition(-1, ss);
  }

  std::vector<size_t> cDims(1, 1);
  m_GBEuclideanDistancesPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getGBEuclideanDistancesArrayPath(), cDims);
  if(nullptr != m_GBEuclideanDistancesPtr.lock())
  {
    m_GBEuclideanDistances = m_GBEuclideanDistancesPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AddBadData::execute()
{
  dataCheck();
  if(getErrorCode() < 0)
  {
    return;
  }

  add_noise();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void AddBadData::add_noise()
{
  notifyStatusMessage("Adding Noise");
  SIMPL_RANDOMNG_NEW()

  DataContainer::Pointer m = getDataContainerArray()->getDataContainer(getGBEuclideanDistancesArrayPath().getDataContainerName());

  QString attMatName = getGBEuclideanDistancesArrayPath().getAttributeMatrixName();
  QList<QString> voxelArrayNames = m->getAttributeMatrix(attMatName)->getAttributeArrayNames();

  float random = 0.0f;
  size_t totalPoints = m->getGeometryAs<ImageGeom>()->getNumberOfElements();
  for(size_t i = 0; i < totalPoints; ++i)
  {
    if(m_BoundaryNoise && m_GBEuclideanDistances[i] < 1)
    {
      random = static_cast<float>(rg.genrand_res53());
      if(random < m_BoundaryVolFraction)
      {
        for(QList<QString>::iterator iter = voxelArrayNames.begin(); iter != voxelArrayNames.end(); ++iter)
        {
          IDataArray::Pointer p = m->getAttributeMatrix(attMatName)->getAttributeArray(*iter);
          int var = 0;
          p->initializeTuple(i, &var);
        }
      }
    }
    if(m_PoissonNoise)
    {
      random = static_cast<float>(rg.genrand_res53());
      if(random < m_PoissonVolFraction)
      {
        for(QList<QString>::iterator iter = voxelArrayNames.begin(); iter != voxelArrayNames.end(); ++iter)
        {
          IDataArray::Pointer p = m->getAttributeMatrix(attMatName)->getAttributeArray(*iter);
          int var = 0;
          p->initializeTuple(i, &var);
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbstractFilter::Pointer AddBadData::newFilterInstance(bool copyFilterParameters) const
{
  AddBadData::Pointer filter = AddBadData::New();
  if(copyFilterParameters)
  {
    copyFilterParameterInstanceVariables(filter.get());
  }
  return filter;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AddBadData::getCompiledLibraryName() const
{
  return SyntheticBuildingConstants::SyntheticBuildingBaseName;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AddBadData::getBrandingString() const
{
  return "SyntheticBuilding";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AddBadData::getFilterVersion() const
{
  QString version;
  QTextStream vStream(&version);
  vStream << SyntheticBuilding::Version::Major() << "." << SyntheticBuilding::Version::Minor() << "." << SyntheticBuilding::Version::Patch();
  return version;
}
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AddBadData::getGroupName() const
{
  return SIMPL::FilterGroups::SyntheticBuildingFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QUuid AddBadData::getUuid() const
{
  return QUuid("{ac99b706-d1e0-5f78-9246-fbbe1efd93d2}");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AddBadData::getSubGroupName() const
{
  return SIMPL::FilterSubGroups::MiscFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString AddBadData::getHumanLabel() const
{
  return "Add Bad Data";
}

// -----------------------------------------------------------------------------
AddBadData::Pointer AddBadData::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::shared_ptr<AddBadData> AddBadData::New()
{
  struct make_shared_enabler : public AddBadData
  {
  };
  std::shared_ptr<make_shared_enabler> val = std::make_shared<make_shared_enabler>();
  val->setupFilterParameters();
  return val;
}

// -----------------------------------------------------------------------------
QString AddBadData::getNameOfClass() const
{
  return QString("AddBadData");
}

// -----------------------------------------------------------------------------
QString AddBadData::ClassName()
{
  return QString("AddBadData");
}

// -----------------------------------------------------------------------------
void AddBadData::setGBEuclideanDistancesArrayPath(const DataArrayPath& value)
{
  m_GBEuclideanDistancesArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath AddBadData::getGBEuclideanDistancesArrayPath() const
{
  return m_GBEuclideanDistancesArrayPath;
}

// -----------------------------------------------------------------------------
void AddBadData::setPoissonNoise(bool value)
{
  m_PoissonNoise = value;
}

// -----------------------------------------------------------------------------
bool AddBadData::getPoissonNoise() const
{
  return m_PoissonNoise;
}

// -----------------------------------------------------------------------------
void AddBadData::setPoissonVolFraction(float value)
{
  m_PoissonVolFraction = value;
}

// -----------------------------------------------------------------------------
float AddBadData::getPoissonVolFraction() const
{
  return m_PoissonVolFraction;
}

// -----------------------------------------------------------------------------
void AddBadData::setBoundaryNoise(bool value)
{
  m_BoundaryNoise = value;
}

// -----------------------------------------------------------------------------
bool AddBadData::getBoundaryNoise() const
{
  return m_BoundaryNoise;
}

// -----------------------------------------------------------------------------
void AddBadData::setBoundaryVolFraction(float value)
{
  m_BoundaryVolFraction = value;
}

// -----------------------------------------------------------------------------
float AddBadData::getBoundaryVolFraction() const
{
  return m_BoundaryVolFraction;
}
