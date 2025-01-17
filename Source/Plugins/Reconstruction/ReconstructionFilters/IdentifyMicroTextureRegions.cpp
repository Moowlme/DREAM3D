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
#include "IdentifyMicroTextureRegions.h"

#include <chrono>

#include <QtCore/QTextStream>

#include "SIMPLib/Common/Constants.h"
#include "SIMPLib/DataContainers/DataContainer.h"
#include "SIMPLib/DataContainers/DataContainerArray.h"
#include "SIMPLib/FilterParameters/AbstractFilterParametersReader.h"
#include "SIMPLib/FilterParameters/DataArraySelectionFilterParameter.h"
#include "SIMPLib/FilterParameters/FloatFilterParameter.h"
#include "SIMPLib/FilterParameters/SeparatorFilterParameter.h"
#include "SIMPLib/FilterParameters/StringFilterParameter.h"
#include "SIMPLib/Geometry/ImageGeom.h"
#include "SIMPLib/Math/GeometryMath.h"
#include "SIMPLib/Math/MatrixMath.h"
#include "SIMPLib/Math/SIMPLibMath.h"

#include "EbsdLib/Core/EbsdLibConstants.h"
#include "EbsdLib/LaueOps/LaueOps.h"

// included so we can call under the hood to segment the patches found in this filter
#include "Reconstruction/ReconstructionConstants.h"
#include "Reconstruction/ReconstructionFilters/VectorSegmentFeatures.h"
#include "Reconstruction/ReconstructionVersion.h"

#ifdef SIMPL_USE_PARALLEL_ALGORITHMS

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/task_group.h>
#include <tbb/tick_count.h>
#endif

enum createdPathID : RenameDataPath::DataID_t
{
  AttributeMatrixID21 = 21,
  AttributeMatrixID22 = 22,

  DataArrayID31 = 31,
  DataArrayID32 = 32,
  DataArrayID33 = 33,
  DataArrayID34 = 34,
  DataArrayID35 = 35,

  DataContainerID = 1
};

/**
 * @brief The FindPatchMisalignmentsImpl class implements a threaded algorithm that determines the misorientations
 * between for all cell faces in the structure
 */
class FindPatchMisalignmentsImpl
{
public:
  FindPatchMisalignmentsImpl(int64_t* newDims, int64_t* origDims, float* caxisLocs, int32_t* phases, uint32_t* crystructs, float* volFrac, float* avgCAxis, bool* inMTR, int64_t* critDim,
                             float minVolFrac, float caxisTol)
  : m_DicDims(newDims)
  , m_VolDims(origDims)
  , m_CAxisLocations(caxisLocs)
  , m_CellPhases(phases)
  , m_CrystalStructures(crystructs)
  , m_InMTR(inMTR)
  , m_VolFrac(volFrac)
  , m_AvgCAxis(avgCAxis)
  , m_CritDim(critDim)
  , m_MinVolFrac(minVolFrac)
  , m_CAxisTolerance(caxisTol)
  {
  }

  // -----------------------------------------------------------------------------
  //
  // -----------------------------------------------------------------------------
  virtual ~FindPatchMisalignmentsImpl() = default;

  void convert(size_t start, size_t end) const
  {
    int64_t xDim = (2 * m_CritDim[0]) + 1;
    int64_t yDim = (2 * m_CritDim[1]) + 1;
    int64_t zDim = (2 * m_CritDim[2]) + 1;
    std::vector<size_t> tDims(1, xDim * yDim * zDim);
    std::vector<size_t> cDims(1, 3);
    FloatArrayType::Pointer cAxisLocsPtr = FloatArrayType::CreateArray(tDims, cDims, "_INTERNAL_USE_ONLY_cAxisLocs", true);
    cAxisLocsPtr->initializeWithValue(0);
    float* cAxisLocs = cAxisLocsPtr->getPointer(0);
    std::vector<int64_t> goodCounts;

    int64_t xc = 0, yc = 0, zc = 0;
    for(size_t iter = start; iter < end; iter++)
    {
      int64_t zStride = 0, yStride = 0;
      int64_t count = 0;
      xc = ((iter % m_DicDims[0]) * m_CritDim[0]) + (m_CritDim[0] / 2);
      yc = (((iter / m_DicDims[0]) % m_DicDims[1]) * m_CritDim[1]) + (m_CritDim[1] / 2);
      zc = ((iter / (m_DicDims[0] * m_DicDims[1])) * m_CritDim[2]) + (m_CritDim[2] / 2);
      for(int64_t k = -m_CritDim[2]; k <= m_CritDim[2]; k++)
      {
        if((zc + k) >= 0 && (zc + k) < m_VolDims[2])
        {
          zStride = ((zc + k) * m_VolDims[0] * m_VolDims[1]);
          for(int64_t j = -m_CritDim[1]; j <= m_CritDim[1]; j++)
          {
            if((yc + j) >= 0 && (yc + j) < m_VolDims[1])
            {
              yStride = ((yc + j) * m_VolDims[0]);
              for(int64_t i = -m_CritDim[0]; i <= m_CritDim[0]; i++)
              {
                if((xc + i) >= 0 && (xc + i) < m_VolDims[0])
                {
                  if(m_CrystalStructures[m_CellPhases[(zStride + yStride + xc + i)]] == EbsdLib::CrystalStructure::Hexagonal_High)
                  {
                    cAxisLocs[3 * count + 0] = m_CAxisLocations[3 * (zStride + yStride + xc + i) + 0];
                    cAxisLocs[3 * count + 1] = m_CAxisLocations[3 * (zStride + yStride + xc + i) + 1];
                    cAxisLocs[3 * count + 2] = m_CAxisLocations[3 * (zStride + yStride + xc + i) + 2];
                    count++;
                  }
                }
              }
            }
          }
        }
      }
      float angle = 0.0f;
      goodCounts.resize(count);
      goodCounts.assign(count, 0);
      for(int64_t i = 0; i < count; i++)
      {
        for(int64_t j = i; j < count; j++)
        {
          angle = GeometryMath::AngleBetweenVectors(cAxisLocsPtr->getPointer(3 * i), cAxisLocsPtr->getPointer(3 * j));
          if(angle <= m_CAxisTolerance || (SIMPLib::Constants::k_PiD - angle) <= m_CAxisTolerance)
          {
            goodCounts[i]++;
            goodCounts[j]++;
          }
        }
      }
      int64_t goodPointCount = 0;
      for(int64_t i = 0; i < count; i++)
      {
        if(float(goodCounts[i]) / float(count) > m_MinVolFrac)
        {
          goodPointCount++;
        }
      }
      float avgCAxis[3] = {0.0f, 0.0f, 0.0f};
      float frac = float(goodPointCount) / float(count);
      m_VolFrac[iter] = frac;
      if(frac > m_MinVolFrac)
      {
        m_InMTR[iter] = true;
        for(int64_t i = 0; i < count; i++)
        {
          if(float(goodCounts[i]) / float(count) >= m_MinVolFrac)
          {
            if(MatrixMath::DotProduct3x1(avgCAxis, cAxisLocsPtr->getPointer(3 * i)) < 0)
            {
              avgCAxis[0] -= cAxisLocs[3 * i];
              avgCAxis[1] -= cAxisLocs[3 * i + 1];
              avgCAxis[2] -= cAxisLocs[3 * i + 2];
            }
            else
            {
              avgCAxis[0] += cAxisLocs[3 * i];
              avgCAxis[1] += cAxisLocs[3 * i + 1];
              avgCAxis[2] += cAxisLocs[3 * i + 2];
            }
          }
        }
        MatrixMath::Normalize3x1(avgCAxis);
        if(avgCAxis[2] < 0)
        {
          MatrixMath::Multiply3x1withConstant(avgCAxis, -1.0f);
        }
        m_AvgCAxis[3 * iter] = avgCAxis[0];
        m_AvgCAxis[3 * iter + 1] = avgCAxis[1];
        m_AvgCAxis[3 * iter + 2] = avgCAxis[2];
      }
    }
  }

#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    convert(r.begin(), r.end());
  }
#endif
private:
  int64_t* m_DicDims;
  int64_t* m_VolDims;
  float* m_CAxisLocations;
  int32_t* m_CellPhases;
  uint32_t* m_CrystalStructures;
  bool* m_InMTR;
  float* m_VolFrac;
  float* m_AvgCAxis;
  int64_t* m_CritDim;
  float m_MinVolFrac;
  float m_CAxisTolerance;
};

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IdentifyMicroTextureRegions::IdentifyMicroTextureRegions()
{
  m_CAxisToleranceRad = 0.0f;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
IdentifyMicroTextureRegions::~IdentifyMicroTextureRegions() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setupFilterParameters()
{
  FilterParameterVectorType parameters;

  parameters.push_back(SIMPL_NEW_FLOAT_FP("C-Axis Alignment Tolerance (Degrees)", CAxisTolerance, FilterParameter::Category::Parameter, IdentifyMicroTextureRegions));
  parameters.push_back(SIMPL_NEW_FLOAT_FP("Minimum MicroTextured Region Size (Diameter)", MinMTRSize, FilterParameter::Category::Parameter, IdentifyMicroTextureRegions));
  parameters.push_back(SIMPL_NEW_FLOAT_FP("Minimum Volume Fraction in MTR", MinVolFrac, FilterParameter::Category::Parameter, IdentifyMicroTextureRegions));

  {
    DataArraySelectionFilterParameter::RequirementType req;
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("C-Axis Locations", CAxisLocationsArrayPath, FilterParameter::Category::RequiredArray, IdentifyMicroTextureRegions, req));
  }
  {
    DataArraySelectionFilterParameter::RequirementType req;
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Cell Phases", CellPhasesArrayPath, FilterParameter::Category::RequiredArray, IdentifyMicroTextureRegions, req));
  }
  {
    DataArraySelectionFilterParameter::RequirementType req;
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Crystal Structures", CrystalStructuresArrayPath, FilterParameter::Category::RequiredArray, IdentifyMicroTextureRegions, req));
  }

  parameters.push_back(SIMPL_NEW_STRING_FP("MTR Ids", MTRIdsArrayName, FilterParameter::Category::CreatedArray, IdentifyMicroTextureRegions));
  parameters.push_back(SIMPL_NEW_STRING_FP("New Cell Feature Attribute Matrix Name", NewCellFeatureAttributeMatrixName, FilterParameter::Category::CreatedArray, IdentifyMicroTextureRegions));
  parameters.push_back(SIMPL_NEW_STRING_FP("Active", ActiveArrayName, FilterParameter::Category::CreatedArray, IdentifyMicroTextureRegions));

  setFilterParameters(parameters);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::readFilterParameters(AbstractFilterParametersReader* reader, int index)
{
  reader->openFilterGroup(this, index);
  setActiveArrayName(reader->readString("ActiveArrayName", getActiveArrayName()));
  setNewCellFeatureAttributeMatrixName(reader->readString("NewCellFeatureAttributeMatrixName", getNewCellFeatureAttributeMatrixName()));
  setMTRIdsArrayName(reader->readString("MTRIdsArrayName", getMTRIdsArrayName()));
  setCAxisLocationsArrayPath(reader->readDataArrayPath("CAxisLocationsArrayPath", getCAxisLocationsArrayPath()));
  setCellPhasesArrayPath(reader->readDataArrayPath("CellPhasesArrayPath", getCellPhasesArrayPath()));
  setCrystalStructuresArrayPath(reader->readDataArrayPath("CrystalStructuresArrayPath", getCrystalStructuresArrayPath()));
  setCAxisTolerance(reader->readValue("CAxisTolerance", getCAxisTolerance()));
  setMinMTRSize(reader->readValue("MinMTRSize", getMinMTRSize()));
  setMinVolFrac(reader->readValue("MinVolFrac", getMinVolFrac()));
  reader->closeFilterGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::updateFeatureInstancePointers()
{
  clearErrorCode();
  clearWarningCode();

  if(nullptr != m_ActivePtr.lock())
  {
    m_Active = m_ActivePtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::initialize()
{
  m_CAxisToleranceRad = 0.0f;
  m_TotalRandomNumbersGenerated = 0;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::dataCheck()
{
  clearErrorCode();
  clearWarningCode();
  initialize();

  DataArrayPath tempPath;

  getDataContainerArray()->getPrereqGeometryFromDataContainer<ImageGeom>(this, getCAxisLocationsArrayPath().getDataContainerName());

  DataContainer::Pointer m = getDataContainerArray()->getPrereqDataContainer(this, getCAxisLocationsArrayPath().getDataContainerName(), false);
  if(getErrorCode() < 0 || nullptr == m.get())
  {
    return;
  }

  std::vector<size_t> tDims(1, 0);
  m->createNonPrereqAttributeMatrix(this, getNewCellFeatureAttributeMatrixName(), tDims, AttributeMatrix::Type::CellFeature, AttributeMatrixID21);

  std::vector<size_t> cDims(1, 3);

  QVector<DataArrayPath> dataArrayPaths;

  // Cell Data
  m_CAxisLocationsPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<float>>(this, getCAxisLocationsArrayPath(), cDims);
  if(nullptr != m_CAxisLocationsPtr.lock())
  {
    m_CAxisLocations = m_CAxisLocationsPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
  if(getErrorCode() >= 0)
  {
    dataArrayPaths.push_back(getCAxisLocationsArrayPath());
  }

  cDims[0] = 1;
  m_CellPhasesPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getCellPhasesArrayPath(), cDims);
  if(nullptr != m_CellPhasesPtr.lock())
  {
    m_CellPhases = m_CellPhasesPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
  if(getErrorCode() >= 0)
  {
    dataArrayPaths.push_back(getCellPhasesArrayPath());
  }

  tempPath.update(m_CAxisLocationsArrayPath.getDataContainerName(), getCAxisLocationsArrayPath().getAttributeMatrixName(), getMTRIdsArrayName());
  m_MTRIdsPtr = getDataContainerArray()->createNonPrereqArrayFromPath<DataArray<int32_t>>(this, tempPath, 0, cDims, "", DataArrayID31);
  if(nullptr != m_MTRIdsPtr.lock())
  {
    m_MTRIds = m_MTRIdsPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // New Feature Data
  tempPath.update(m_CAxisLocationsArrayPath.getDataContainerName(), getNewCellFeatureAttributeMatrixName(), getActiveArrayName());
  m_ActivePtr = getDataContainerArray()->createNonPrereqArrayFromPath<DataArray<bool>>(this, tempPath, true, cDims, "", DataArrayID32);
  if(nullptr != m_ActivePtr.lock())
  {
    m_Active = m_ActivePtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Ensemble Data
  m_CrystalStructuresPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<uint32_t>>(this, getCrystalStructuresArrayPath(), cDims);
  if(nullptr != m_CrystalStructuresPtr.lock())
  {
    m_CrystalStructures = m_CrystalStructuresPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  getDataContainerArray()->validateNumberOfTuples(this, dataArrayPaths);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::randomizeFeatureIds(int64_t totalPoints, int64_t totalFeatures)
{
  notifyStatusMessage("Randomizing Feature Ids");
  // Generate an even distribution of numbers between the min and max range
  const int32_t rangeMin = 1;
  const int32_t rangeMax = totalFeatures - 1;
  initializeVoxelSeedGenerator(rangeMin, rangeMax);

  DataArray<int32_t>::Pointer rndNumbers = DataArray<int32_t>::CreateArray(totalFeatures, std::string("_INTERNAL_USE_ONLY_NewFeatureIds"), true);

  int32_t* gid = rndNumbers->getPointer(0);
  gid[0] = 0;
  for(int64_t i = 1; i < totalFeatures; ++i)
  {
    gid[i] = i;
  }

  int32_t r = 0;
  int32_t temp = 0;

  //--- Shuffle elements by randomly exchanging each with one other.
  for(int64_t i = 1; i < totalFeatures; i++)
  {
    r = m_Distribution(m_Generator); // Random remaining position.
    if(r >= totalFeatures)
    {
      continue;
    }
    temp = gid[i];
    gid[i] = gid[r];
    gid[r] = temp;
  }

  // Now adjust all the Grain Id values for each Voxel
  for(int64_t i = 0; i < totalPoints; ++i)
  {
    // m_MTRIds[i] = gid[ m_MTRIds[i] ];
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::findMTRregions()
{
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::initializeVoxelSeedGenerator(const int32_t rangeMin, const int32_t rangeMax)
{

  std::mt19937_64::result_type seed = static_cast<std::mt19937_64::result_type>(std::chrono::steady_clock::now().time_since_epoch().count());
  m_Generator.seed(seed);
  m_Distribution = std::uniform_int_distribution<int64_t>(rangeMin, rangeMax);
  m_Distribution = std::uniform_int_distribution<int64_t>(rangeMin, rangeMax);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::execute()
{
  dataCheck();
  if(getErrorCode() < 0)
  {
    return;
  }

  DataContainer::Pointer m = getDataContainerArray()->getDataContainer(getCAxisLocationsArrayPath().getDataContainerName());
  int64_t totalPoints = static_cast<int64_t>(m_MTRIdsPtr.lock()->getNumberOfTuples());

  // calculate dimensions of DIC-like grid
  SizeVec3Type dcDims = m->getGeometryAs<ImageGeom>()->getDimensions();
  FloatVec3Type spacing = m->getGeometryAs<ImageGeom>()->getSpacing();
  FloatVec3Type origin = m->getGeometryAs<ImageGeom>()->getOrigin();

  // Find number of original cells in radius of patch
  int64_t critDim[3] = {0, 0, 0};
  critDim[0] = static_cast<int64_t>(m_MinMTRSize / (4.0f * spacing[0]));
  critDim[1] = static_cast<int64_t>(m_MinMTRSize / (4.0f * spacing[1]));
  critDim[2] = static_cast<int64_t>(m_MinMTRSize / (4.0f * spacing[2]));

  // Find physical distance of patch steps
  FloatVec3Type critRes;
  critRes[0] = static_cast<float>(critDim[0]) * spacing[0];
  critRes[1] = static_cast<float>(critDim[1]) * spacing[1];
  critRes[2] = static_cast<float>(critDim[2]) * spacing[2];

  // Find number of patch steps in each dimension
  using Int64Vec3Type = IVec3<int64_t>;
  Int64Vec3Type newDim(static_cast<int64_t>(dcDims[0] / critDim[0]), static_cast<int64_t>(dcDims[1] / critDim[1]), static_cast<int64_t>(dcDims[2] / critDim[2]));

  if(dcDims[0] == 1)
  {
    newDim[0] = 1, critDim[0] = 0;
  }
  if(dcDims[1] == 1)
  {
    newDim[1] = 1, critDim[1] = 0;
  }
  if(dcDims[2] == 1)
  {
    newDim[2] = 1, critDim[2] = 0;
  }

  // Store the original and patch dimensions for passing into the parallel algo below
  Int64Vec3Type origDims(dcDims[0], dcDims[1], dcDims[2]);
  Int64Vec3Type newDims = newDim;
  size_t totalPatches = static_cast<size_t>(newDim[0] * newDim[1] * newDim[2]);

  // Create temporary DataContainer and AttributeMatrix for holding the patch data
  DataContainer::Pointer tmpDC = getDataContainerArray()->createNonPrereqDataContainer(this, "_INTERNAL_USE_ONLY_PatchDataContainer(Temp)", DataContainerID);
  if(getErrorCode() < 0)
  {
    return;
  }
  tmpDC->getGeometryAs<ImageGeom>()->setDimensions(SizeVec3Type(static_cast<size_t>(newDim[0]), static_cast<size_t>(newDim[1]), static_cast<size_t>(newDim[2])));
  tmpDC->getGeometryAs<ImageGeom>()->setSpacing(critRes);
  tmpDC->getGeometryAs<ImageGeom>()->setOrigin(origin);

  std::vector<size_t> tDims;
  tDims[0] = newDim[0];
  tDims[1] = newDim[1];
  tDims[2] = newDim[2];
  tmpDC->createNonPrereqAttributeMatrix(this, "_INTERNAL_USE_ONLY_PatchAM(Temp)", tDims, AttributeMatrix::Type::Cell, AttributeMatrixID22);
  if(getErrorCode() < 0)
  {
    return;
  }

  DataArrayPath tempPath;
  tDims[0] = totalPatches;
  std::vector<size_t> cDims(1, 1);
  tempPath.update("_INTERNAL_USE_ONLY_PatchDataContainer(Temp)", "_INTERNAL_USE_ONLY_PatchAM(Temp)", "_INTERNAL_USE_ONLY_InMTR");
  m_InMTRPtr = getDataContainerArray()->createNonPrereqArrayFromPath<DataArray<bool>>(this, tempPath, false, cDims, "", DataArrayID33);
  if(nullptr != m_InMTRPtr.lock())
  {
    m_InMTR = m_InMTRPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  tempPath.update("_INTERNAL_USE_ONLY_PatchDataContainer(Temp)", "_INTERNAL_USE_ONLY_PatchAM(Temp)", "_INTERNAL_USE_ONLY_VolFrac");
  m_VolFracPtr = getDataContainerArray()->createNonPrereqArrayFromPath<DataArray<float>>(this, tempPath, 0, cDims, "", DataArrayID34);
  if(nullptr != m_VolFracPtr.lock())
  {
    m_VolFrac = m_VolFracPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  cDims[0] = 3;
  tempPath.update("_INTERNAL_USE_ONLY_PatchDataContainer(Temp)", "_INTERNAL_USE_ONLY_PatchAM(Temp)", "_INTERNAL_USE_ONLY_AvgCAxis");
  m_AvgCAxisPtr = getDataContainerArray()->createNonPrereqArrayFromPath<DataArray<float>>(this, tempPath, 0, cDims, "", DataArrayID35);
  if(nullptr != m_AvgCAxisPtr.lock())
  {
    m_AvgCAxis = m_AvgCAxisPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Convert user defined tolerance to radians.
  m_CAxisToleranceRad = m_CAxisTolerance * SIMPLib::Constants::k_PiD / 180.0f;

// first determine the misorientation vectors on all the voxel faces
#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
  if(true)
  {
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, totalPatches),
        FindPatchMisalignmentsImpl(newDims.data(), origDims.data(), m_CAxisLocations, m_CellPhases, m_CrystalStructures, m_VolFrac, m_AvgCAxis, m_InMTR, critDim, m_MinVolFrac, m_CAxisToleranceRad),
        tbb::auto_partitioner());
  }
  else
#endif
  {
    FindPatchMisalignmentsImpl serial(newDims.data(), origDims.data(), m_CAxisLocations, m_CellPhases, m_CrystalStructures, m_VolFrac, m_AvgCAxis, m_InMTR, critDim, m_MinVolFrac, m_CAxisToleranceRad);
    serial.convert(0, totalPatches);
  }

  // Call the SegmentFeatures(Vector) filter under the hood to segment the patches based on average c-axis of the patch
  VectorSegmentFeatures::Pointer filter = VectorSegmentFeatures::New();
  filter->setDataContainerArray(getDataContainerArray());
  tempPath.update("_INTERNAL_USE_ONLY_PatchDataContainer(Temp)", "_INTERNAL_USE_ONLY_PatchAM(Temp)", "_INTERNAL_USE_ONLY_AvgCAxis");
  filter->setSelectedVectorArrayPath(tempPath);
  filter->setAngleTolerance(m_CAxisTolerance);
  filter->setUseGoodVoxels(true);
  tempPath.update("_INTERNAL_USE_ONLY_PatchDataContainer(Temp)", "_INTERNAL_USE_ONLY_PatchAM(Temp)", "_INTERNAL_USE_ONLY_InMTR");
  filter->setGoodVoxelsArrayPath(tempPath);
  filter->setFeatureIdsArrayName("PatchFeatureIds");
  filter->setCellFeatureAttributeMatrixName("PatchFeatureData");
  filter->setActiveArrayName("Active");
  filter->execute();

  // get the data created by the SegmentFeatures(Vector) filter
  cDims[0] = 1;
  tempPath.update("_INTERNAL_USE_ONLY_PatchDataContainer(Temp)", "_INTERNAL_USE_ONLY_PatchAM(Temp)", "_INTERNAL_USE_ONLY_PatchFeatureIds");
  m_PatchIdsPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, tempPath, cDims);
  if(nullptr != m_PatchIdsPtr.lock())
  {
    m_PatchIds = m_PatchIdsPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
  tempPath.update("_INTERNAL_USE_ONLY_PatchDataContainer(Temp)", "_INTERNAL_USE_ONLY_PatchFeatureData", "_INTERNAL_USE_ONLY_Active");
  m_PatchActivePtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<bool>>(this, tempPath, cDims);
  if(nullptr != m_PatchActivePtr.lock())
  {
    m_PatchActive = m_PatchActivePtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Remove the small patches-----planning to remove/redsesign this
  size_t numPatchFeatures = m_PatchActivePtr.lock()->getNumberOfTuples();
  QVector<bool> activeObjects(numPatchFeatures, true);
  std::vector<size_t> counters(numPatchFeatures, 0);
  for(size_t iter = 0; iter < totalPatches; ++iter)
  {
    counters[m_PatchIds[iter]]++;
  }
  for(size_t iter = 0; iter < numPatchFeatures; ++iter)
  {
    if(counters[iter] < 4)
    {
      activeObjects[iter] = false;
    }
  }
  tempPath.update("_INTERNAL_USE_ONLY_PatchDataContainer(Temp)", "_INTERNAL_USE_ONLY_PatchFeatureData", "_INTERNAL_USE_ONLY_Active");
  AttributeMatrix::Pointer patchFeatureAttrMat = getDataContainerArray()->getAttributeMatrix(tempPath);
  patchFeatureAttrMat->removeInactiveObjects(activeObjects, m_PatchIdsPtr.lock().get());

  // Resize the feature attribute matrix for the MTRs to the number identified from SegmentFeatures(Vector) after filtering for size
  tDims.resize(1);
  tDims[0] = patchFeatureAttrMat->getNumberOfTuples();
  m->getAttributeMatrix(getNewCellFeatureAttributeMatrixName())->resizeAttributeArrays(tDims);
  updateFeatureInstancePointers();

  int64_t point = 0, patch = 0;
  int64_t zStride = 0, yStride = 0;
  int64_t zStrideP = 0, yStrideP = 0;
  int64_t pCol = 0, pRow = 0, pPlane = 0;

  for(int64_t k = 0; k < origDims[2]; k++)
  {
    if(critDim[2] > 0)
    {
      pPlane = (k / critDim[2]);
    }
    else
    {
      pPlane = 0;
    }
    if(pPlane >= newDims[2])
    {
      pPlane = newDims[2] - 1;
    }
    zStride = (k * origDims[0] * origDims[1]);
    zStrideP = (pPlane * newDims[0] * newDims[1]);
    for(int64_t j = 0; j < origDims[1]; j++)
    {
      if(critDim[1] > 0)
      {
        pRow = (j / critDim[1]);
      }
      else
      {
        pRow = 0;
      }
      if(pRow >= newDims[1])
      {
        pRow = newDims[1] - 1;
      }
      yStride = (j * origDims[0]);
      yStrideP = (pRow * newDims[0]);
      for(int64_t i = 0; i < origDims[0]; i++)
      {
        if(critDim[0] > 0)
        {
          pCol = (i / critDim[0]);
        }
        else
        {
          pCol = 0;
        }
        if(pCol >= newDims[0])
        {
          pCol = newDims[0] - 1;
        }
        point = zStride + yStride + i;
        patch = zStrideP + yStrideP + pCol;
        m_MTRIds[point] = m_PatchIds[patch];
        if(m_PatchIds[patch] > 0)
        {
          m_CAxisLocations[3 * point + 0] = m_AvgCAxis[3 * patch + 0];
          m_CAxisLocations[3 * point + 1] = m_AvgCAxis[3 * patch + 1];
          m_CAxisLocations[3 * point + 2] = m_AvgCAxis[3 * patch + 2];
        }
      }
    }
  }

  // remove the data container temporarily created to hold the patch data
  getDataContainerArray()->removeDataContainer("PatchDataContainer(Temp)");

  findMTRregions();

  int64_t totalFeatures = static_cast<int64_t>(m_AvgCAxisPtr.lock()->getNumberOfTuples());

  // By default we randomize grains
  if(getRandomizeMTRIds() && !getCancel())
  {
    totalPoints = static_cast<int64_t>(m->getGeometryAs<ImageGeom>()->getNumberOfElements());
    randomizeFeatureIds(totalPoints, totalFeatures);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbstractFilter::Pointer IdentifyMicroTextureRegions::newFilterInstance(bool copyFilterParameters) const
{
  IdentifyMicroTextureRegions::Pointer filter = IdentifyMicroTextureRegions::New();
  if(copyFilterParameters)
  {
    filter->setFilterParameters(getFilterParameters());
    copyFilterParameterInstanceVariables(filter.get());
  }
  return filter;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::getCompiledLibraryName() const
{
  return ReconstructionConstants::ReconstructionBaseName;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::getBrandingString() const
{
  return "Reconstruction";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::getFilterVersion() const
{
  QString version;
  QTextStream vStream(&version);
  vStream << Reconstruction::Version::Major() << "." << Reconstruction::Version::Minor() << "." << Reconstruction::Version::Patch();
  return version;
}
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::getGroupName() const
{
  return SIMPL::FilterGroups::ReconstructionFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QUuid IdentifyMicroTextureRegions::getUuid() const
{
  return QUuid("{00717d6b-004e-5e1f-9acc-ee2920ddc29b}");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::getSubGroupName() const
{
  return SIMPL::FilterSubGroups::GroupingFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::getHumanLabel() const
{
  return "Identify MicroTexture Patches (C-Axis Misalignment)";
}

// -----------------------------------------------------------------------------
IdentifyMicroTextureRegions::Pointer IdentifyMicroTextureRegions::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::shared_ptr<IdentifyMicroTextureRegions> IdentifyMicroTextureRegions::New()
{
  struct make_shared_enabler : public IdentifyMicroTextureRegions
  {
  };
  std::shared_ptr<make_shared_enabler> val = std::make_shared<make_shared_enabler>();
  val->setupFilterParameters();
  return val;
}

// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::getNameOfClass() const
{
  return QString("IdentifyMicroTextureRegions");
}

// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::ClassName()
{
  return QString("IdentifyMicroTextureRegions");
}

// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setNewCellFeatureAttributeMatrixName(const QString& value)
{
  m_NewCellFeatureAttributeMatrixName = value;
}

// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::getNewCellFeatureAttributeMatrixName() const
{
  return m_NewCellFeatureAttributeMatrixName;
}

// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setCAxisTolerance(float value)
{
  m_CAxisTolerance = value;
}

// -----------------------------------------------------------------------------
float IdentifyMicroTextureRegions::getCAxisTolerance() const
{
  return m_CAxisTolerance;
}

// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setMinMTRSize(float value)
{
  m_MinMTRSize = value;
}

// -----------------------------------------------------------------------------
float IdentifyMicroTextureRegions::getMinMTRSize() const
{
  return m_MinMTRSize;
}

// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setMinVolFrac(float value)
{
  m_MinVolFrac = value;
}

// -----------------------------------------------------------------------------
float IdentifyMicroTextureRegions::getMinVolFrac() const
{
  return m_MinVolFrac;
}

// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setRandomizeMTRIds(bool value)
{
  m_RandomizeMTRIds = value;
}

// -----------------------------------------------------------------------------
bool IdentifyMicroTextureRegions::getRandomizeMTRIds() const
{
  return m_RandomizeMTRIds;
}

// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setCAxisLocationsArrayPath(const DataArrayPath& value)
{
  m_CAxisLocationsArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath IdentifyMicroTextureRegions::getCAxisLocationsArrayPath() const
{
  return m_CAxisLocationsArrayPath;
}

// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setCellPhasesArrayPath(const DataArrayPath& value)
{
  m_CellPhasesArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath IdentifyMicroTextureRegions::getCellPhasesArrayPath() const
{
  return m_CellPhasesArrayPath;
}

// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setCrystalStructuresArrayPath(const DataArrayPath& value)
{
  m_CrystalStructuresArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath IdentifyMicroTextureRegions::getCrystalStructuresArrayPath() const
{
  return m_CrystalStructuresArrayPath;
}

// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setMTRIdsArrayName(const QString& value)
{
  m_MTRIdsArrayName = value;
}

// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::getMTRIdsArrayName() const
{
  return m_MTRIdsArrayName;
}

// -----------------------------------------------------------------------------
void IdentifyMicroTextureRegions::setActiveArrayName(const QString& value)
{
  m_ActiveArrayName = value;
}

// -----------------------------------------------------------------------------
QString IdentifyMicroTextureRegions::getActiveArrayName() const
{
  return m_ActiveArrayName;
}
