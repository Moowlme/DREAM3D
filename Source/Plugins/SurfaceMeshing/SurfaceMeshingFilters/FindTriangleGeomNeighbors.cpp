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
#include "FindTriangleGeomNeighbors.h"

#include <QtCore/QDateTime>
#include <QtCore/QTextStream>

#include "SIMPLib/Common/Constants.h"
#include "SIMPLib/DataContainers/DataContainer.h"
#include "SIMPLib/DataContainers/DataContainerArray.h"
#include "SIMPLib/FilterParameters/AbstractFilterParametersReader.h"
#include "SIMPLib/FilterParameters/AttributeMatrixSelectionFilterParameter.h"
#include "SIMPLib/FilterParameters/DataArraySelectionFilterParameter.h"
#include "SIMPLib/FilterParameters/LinkedPathCreationFilterParameter.h"
#include "SIMPLib/FilterParameters/SeparatorFilterParameter.h"
#include "SIMPLib/FilterParameters/StringFilterParameter.h"
#include "SIMPLib/Geometry/TriangleGeom.h"

#include "SurfaceMeshing/SurfaceMeshingConstants.h"
#include "SurfaceMeshing/SurfaceMeshingVersion.h"

/* Create Enumerations to allow the created Attribute Arrays to take part in renaming */
enum createdPathID : RenameDataPath::DataID_t
{
  DataArrayID30 = 30,
  DataArrayID31 = 31,
};

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
FindTriangleGeomNeighbors::FindTriangleGeomNeighbors()
{
  m_NeighborList = NeighborList<int32_t>::NullPointer();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
FindTriangleGeomNeighbors::~FindTriangleGeomNeighbors() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindTriangleGeomNeighbors::setupFilterParameters()
{
  FilterParameterVectorType parameters;
  parameters.push_back(SeparatorFilterParameter::Create("Face Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Int32, 2, AttributeMatrix::Type::Face, IGeometry::Type::Triangle);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Face Labels", FaceLabelsArrayPath, FilterParameter::Category::RequiredArray, FindTriangleGeomNeighbors, req));
  }
  parameters.push_back(SeparatorFilterParameter::Create("Face Feature Data", FilterParameter::Category::RequiredArray));
  {
    AttributeMatrixSelectionFilterParameter::RequirementType req = AttributeMatrixSelectionFilterParameter::CreateRequirement(AttributeMatrix::Type::FaceFeature, IGeometry::Type::Triangle);
    parameters.push_back(SIMPL_NEW_AM_SELECTION_FP("Face Feature Attribute Matrix", FeatureAttributeMatrixPath, FilterParameter::Category::RequiredArray, FindTriangleGeomNeighbors, req));
  }
  parameters.push_back(SeparatorFilterParameter::Create("Face Feature Data", FilterParameter::Category::CreatedArray));
  parameters.push_back(SIMPL_NEW_DA_WITH_LINKED_AM_FP("Number of Neighbors", NumNeighborsArrayName, FeatureAttributeMatrixPath, FeatureAttributeMatrixPath, FilterParameter::Category::CreatedArray,
                                                      FindTriangleGeomNeighbors));
  parameters.push_back(SIMPL_NEW_DA_WITH_LINKED_AM_FP("Neighbor List", NeighborListArrayName, FeatureAttributeMatrixPath, FeatureAttributeMatrixPath, FilterParameter::Category::CreatedArray,
                                                      FindTriangleGeomNeighbors));
  setFilterParameters(parameters);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindTriangleGeomNeighbors::initialize()
{
  m_NeighborList = NeighborList<int32_t>::NullPointer();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindTriangleGeomNeighbors::dataCheck()
{
  clearErrorCode();
  clearWarningCode();
  initialize();
  DataArrayPath tempPath;

  getDataContainerArray()->getPrereqGeometryFromDataContainer<TriangleGeom>(this, getFaceLabelsArrayPath().getDataContainerName());

  std::vector<size_t> cDims(1, 2);
  m_FaceLabelsPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getFaceLabelsArrayPath(), cDims);
  if(nullptr != m_FaceLabelsPtr.lock())
  {
    m_FaceLabels = m_FaceLabelsPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  getDataContainerArray()->getPrereqAttributeMatrixFromPath(this, getFeatureAttributeMatrixPath(), -301);

  cDims[0] = 1;

  tempPath.update(getFeatureAttributeMatrixPath().getDataContainerName(), getFeatureAttributeMatrixPath().getAttributeMatrixName(), getNumNeighborsArrayName());
  m_NumNeighborsPtr = getDataContainerArray()->createNonPrereqArrayFromPath<DataArray<int32_t>>(this, tempPath, 0, cDims);
  if(nullptr != m_NumNeighborsPtr.lock())
  {
    m_NumNeighbors = m_NumNeighborsPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Feature Data
  // Do this whole block FIRST otherwise the side effect is that a call to m->getNumCellFeatureTuples will = 0
  // because we are just creating an empty NeighborList object.
  // Now we are going to get a "Pointer" to the NeighborList object out of the DataContainer
  tempPath.update(getFeatureAttributeMatrixPath().getDataContainerName(), getFeatureAttributeMatrixPath().getAttributeMatrixName(), getNeighborListArrayName());
  m_NeighborList = getDataContainerArray()->createNonPrereqArrayFromPath<NeighborList<int32_t>>(this, tempPath, 0, cDims, "", DataArrayID31);
  if(getErrorCode() < 0)
  {
    return;
  }
  m_NeighborList.lock()->setNumNeighborsArrayName(getNumNeighborsArrayName());
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindTriangleGeomNeighbors::execute()
{
  dataCheck();
  if(getErrorCode() < 0)
  {
    return;
  }

  DataContainer::Pointer m = getDataContainerArray()->getDataContainer(m_FaceLabelsArrayPath.getDataContainerName());
  size_t totalFaces = m_FaceLabelsPtr.lock()->getNumberOfTuples();
  size_t totalFeatures = m_NumNeighborsPtr.lock()->getNumberOfTuples();

  int32_t feature1 = 0;
  int32_t feature2 = 0;
  int32_t nnum = 0;

  std::vector<std::vector<int32_t>> neighborlist;

  int32_t nListSize = 100;
  neighborlist.resize(totalFeatures);

  uint64_t millis = QDateTime::currentMSecsSinceEpoch();
  uint64_t currentMillis = millis;

  for(size_t i = 1; i < totalFeatures; i++)
  {
    currentMillis = QDateTime::currentMSecsSinceEpoch();
    if(currentMillis - millis > 1000)
    {
      QString ss = QObject::tr("Finding Neighbors || Initializing Neighbor Lists || %1% Complete").arg((static_cast<float>(i) / totalFeatures) * 100);
      notifyStatusMessage(ss);
      millis = QDateTime::currentMSecsSinceEpoch();
    }

    if(getCancel())
    {
      return;
    }

    m_NumNeighbors[i] = 0;
    neighborlist[i].resize(nListSize);
  }

  for(size_t j = 0; j < totalFaces; j++)
  {
    currentMillis = QDateTime::currentMSecsSinceEpoch();
    if(currentMillis - millis > 1000)
    {
      QString ss = QObject::tr("Finding Neighbors || Determining Neighbor Lists || %1% Complete").arg((static_cast<float>(j) / totalFaces) * 100);
      notifyStatusMessage(ss);
      millis = QDateTime::currentMSecsSinceEpoch();
    }

    if(getCancel())
    {
      return;
    }

    feature1 = m_FaceLabels[2 * j];
    feature2 = m_FaceLabels[2 * j + 1];
    if(feature1 > 0)
    {
      if(feature2 > 0)
      {
        nnum = m_NumNeighbors[feature1];
        neighborlist[feature1].push_back(feature2);
        nnum++;
        m_NumNeighbors[feature1] = nnum;
      }
    }
    if(feature2 > 0)
    {
      if(feature1 > 0)
      {
        nnum = m_NumNeighbors[feature2];
        neighborlist[feature2].push_back(feature1);
        nnum++;
        m_NumNeighbors[feature2] = nnum;
      }
    }
  }

  // We do this to create new set of NeighborList objects
  for(size_t i = 1; i < totalFeatures; i++)
  {
    currentMillis = QDateTime::currentMSecsSinceEpoch();
    if(currentMillis - millis > 1000)
    {
      QString ss = QObject::tr("Finding Neighbors || Calculating Surface Areas || %1% Complete").arg(((float)i / totalFeatures) * 100);
      notifyStatusMessage(ss);
      millis = QDateTime::currentMSecsSinceEpoch();
    }

    if(getCancel())
    {
      return;
    }

    QMap<int32_t, int32_t> neighToCount;
    int32_t numneighs = static_cast<int32_t>(neighborlist[i].size());

    // this increments the voxel counts for each feature
    for(int32_t j = 0; j < numneighs; j++)
    {
      neighToCount[neighborlist[i][j]]++;
    }

    QMap<int32_t, int32_t>::Iterator neighiter = neighToCount.find(0);
    neighToCount.erase(neighiter);
    neighiter = neighToCount.find(-1);
    neighToCount.erase(neighiter);
    // Resize the features neighbor list to zero
    neighborlist[i].resize(0);

    for(QMap<int32_t, int32_t>::iterator iter = neighToCount.begin(); iter != neighToCount.end(); ++iter)
    {
      int32_t neigh = iter.key(); // get the neighbor feature

      // Push the neighbor feature id back onto the list so we stay synced up
      neighborlist[i].push_back(neigh);
    }
    m_NumNeighbors[i] = int32_t(neighborlist[i].size());

    // Set the vector for each list into the NeighborList Object
    NeighborList<int32_t>::SharedVectorType sharedNeiLst(new std::vector<int32_t>);
    sharedNeiLst->assign(neighborlist[i].begin(), neighborlist[i].end());
    m_NeighborList.lock()->setList(static_cast<int32_t>(i), sharedNeiLst);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbstractFilter::Pointer FindTriangleGeomNeighbors::newFilterInstance(bool copyFilterParameters) const
{
  FindTriangleGeomNeighbors::Pointer filter = FindTriangleGeomNeighbors::New();
  if(copyFilterParameters)
  {
    copyFilterParameterInstanceVariables(filter.get());
  }
  return filter;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindTriangleGeomNeighbors::getCompiledLibraryName() const
{
  return SurfaceMeshingConstants::SurfaceMeshingBaseName;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindTriangleGeomNeighbors::getBrandingString() const
{
  return "Statistics";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindTriangleGeomNeighbors::getFilterVersion() const
{
  QString version;
  QTextStream vStream(&version);
  vStream << SurfaceMeshing::Version::Major() << "." << SurfaceMeshing::Version::Minor() << "." << SurfaceMeshing::Version::Patch();
  return version;
}
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindTriangleGeomNeighbors::getGroupName() const
{
  return SIMPL::FilterGroups::StatisticsFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QUuid FindTriangleGeomNeighbors::getUuid() const
{
  return QUuid("{749dc8ae-a402-5ee7-bbca-28d5734c60df}");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindTriangleGeomNeighbors::getSubGroupName() const
{
  return SIMPL::FilterSubGroups::MorphologicalFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindTriangleGeomNeighbors::getHumanLabel() const
{
  return "Find Feature Neighbors from Triangle Geometry";
}

// -----------------------------------------------------------------------------
FindTriangleGeomNeighbors::Pointer FindTriangleGeomNeighbors::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::shared_ptr<FindTriangleGeomNeighbors> FindTriangleGeomNeighbors::New()
{
  struct make_shared_enabler : public FindTriangleGeomNeighbors
  {
  };
  std::shared_ptr<make_shared_enabler> val = std::make_shared<make_shared_enabler>();
  val->setupFilterParameters();
  return val;
}

// -----------------------------------------------------------------------------
QString FindTriangleGeomNeighbors::getNameOfClass() const
{
  return QString("FindTriangleGeomNeighbors");
}

// -----------------------------------------------------------------------------
QString FindTriangleGeomNeighbors::ClassName()
{
  return QString("FindTriangleGeomNeighbors");
}

// -----------------------------------------------------------------------------
void FindTriangleGeomNeighbors::setFeatureAttributeMatrixPath(const DataArrayPath& value)
{
  m_FeatureAttributeMatrixPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath FindTriangleGeomNeighbors::getFeatureAttributeMatrixPath() const
{
  return m_FeatureAttributeMatrixPath;
}

// -----------------------------------------------------------------------------
void FindTriangleGeomNeighbors::setNeighborListArrayName(const QString& value)
{
  m_NeighborListArrayName = value;
}

// -----------------------------------------------------------------------------
QString FindTriangleGeomNeighbors::getNeighborListArrayName() const
{
  return m_NeighborListArrayName;
}

// -----------------------------------------------------------------------------
void FindTriangleGeomNeighbors::setFaceLabelsArrayPath(const DataArrayPath& value)
{
  m_FaceLabelsArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath FindTriangleGeomNeighbors::getFaceLabelsArrayPath() const
{
  return m_FaceLabelsArrayPath;
}

// -----------------------------------------------------------------------------
void FindTriangleGeomNeighbors::setNumNeighborsArrayName(const QString& value)
{
  m_NumNeighborsArrayName = value;
}

// -----------------------------------------------------------------------------
QString FindTriangleGeomNeighbors::getNumNeighborsArrayName() const
{
  return m_NumNeighborsArrayName;
}
