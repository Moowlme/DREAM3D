/* ============================================================================
 * This filter has been created by Krzysztof Glowinski (kglowinski at ymail.com).
 * It adapts the algorithm described in K.Glowinski, A.Morawiec, "Analysis of
 * experimental grain boundary distributions based on boundary-space metrics",
 * Metall. Mater. Trans. A 45, 3189-3194 (2014).
 * Besides the algorithm itself, many parts of the code come from
 * the sources of other filters, mainly "Find GBCD" and "Write GBCD Pole Figure (GMT5)".
 * Therefore, the below copyright notice applies.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

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

#include "FindGBPDMetricBased.h"

#include <QtCore/QDir>
#include <QtCore/QTextStream>

#include "SIMPLib/Common/Constants.h"
#include "SIMPLib/DataContainers/DataContainer.h"
#include "SIMPLib/DataContainers/DataContainerArray.h"
#include "SIMPLib/FilterParameters/AbstractFilterParametersReader.h"
#include "SIMPLib/FilterParameters/BooleanFilterParameter.h"
#include "SIMPLib/FilterParameters/DataArraySelectionFilterParameter.h"
#include "SIMPLib/FilterParameters/DataContainerSelectionFilterParameter.h"
#include "SIMPLib/FilterParameters/FloatFilterParameter.h"
#include "SIMPLib/FilterParameters/IntFilterParameter.h"
#include "SIMPLib/FilterParameters/OutputFileFilterParameter.h"
#include "SIMPLib/FilterParameters/SeparatorFilterParameter.h"
#include "SIMPLib/Geometry/TriangleGeom.h"
#include "SIMPLib/Math/MatrixMath.h"
#include "SIMPLib/Utilities/FileSystemPathHelper.h"

#include "EbsdLib/LaueOps/LaueOps.h"

#include "OrientationAnalysis/OrientationAnalysisConstants.h"
#include "OrientationAnalysis/OrientationAnalysisVersion.h"

#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/partitioner.h>
#include "tbb/concurrent_vector.h"
#endif

using LaueOpsShPtrType = std::shared_ptr<LaueOps>;
using LaueOpsContainer = std::vector<LaueOpsShPtrType>;

namespace GBPDMetricBased
{

/**
 * @brief The TriAreaAndNormals class defines a container that stores the area of a given triangle
 * and the two normals for grains on either side of the triangle
 */
class TriAreaAndNormals
{
public:
  double area;
  float normal_grain1_x;
  float normal_grain1_y;
  float normal_grain1_z;
  float normal_grain2_x;
  float normal_grain2_y;
  float normal_grain2_z;

  TriAreaAndNormals(double __area, float n1x, float n1y, float n1z, float n2x, float n2y, float n2z)
  : area(__area)
  , normal_grain1_x(n1x)
  , normal_grain1_y(n1y)
  , normal_grain1_z(n1z)
  , normal_grain2_x(n2x)
  , normal_grain2_y(n2y)
  , normal_grain2_z(n2z)
  {
  }

  bool operator<(const TriAreaAndNormals& other)
  {
    return area < other.area;
  }

  TriAreaAndNormals()
  {
    TriAreaAndNormals(0.0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  }
};

/**
 * @brief The TrisSelector class implements a threaded algorithm that determines which triangles to
 * include in the GBPD calculation
 */
class TrisSelector
{
  // corresponding to Phase of Interest
  bool m_ExcludeTripleLines;
  MeshIndexType* m_Triangles = nullptr;
  int8_t* m_NodeTypes = nullptr;
#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
  tbb::concurrent_vector<TriAreaAndNormals>* selectedTris;
#else
  QVector<TriAreaAndNormals>* selectedTris;
#endif
  int32_t m_PhaseOfInterest;
  LaueOpsContainer m_OrientationOps;
  uint32_t cryst;
  int32_t nsym;
  float* m_Eulers = nullptr;
  int32_t* m_Phases = nullptr;
  int32_t* m_FaceLabels = nullptr;
  double* m_FaceNormals = nullptr;
  double* m_FaceAreas = nullptr;

public:
  TrisSelector(bool __m_ExcludeTripleLines, MeshIndexType* __m_Triangles, int8_t* __m_NodeTypes,

#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
               tbb::concurrent_vector<TriAreaAndNormals>* __selectedTris,
#else
               QVector<TriAreaAndNormals>* __selectedTris,
#endif
               int32_t __m_PhaseOfInterest, uint32_t* __m_CrystalStructures, float* __m_Eulers, int32_t* __m_Phases, int32_t* __m_FaceLabels, double* __m_FaceNormals, double* __m_FaceAreas)
  : m_ExcludeTripleLines(__m_ExcludeTripleLines)
  , m_Triangles(__m_Triangles)
  , m_NodeTypes(__m_NodeTypes)
  , selectedTris(__selectedTris)
  , m_PhaseOfInterest(__m_PhaseOfInterest)
  , m_Eulers(__m_Eulers)
  , m_Phases(__m_Phases)
  , m_FaceLabels(__m_FaceLabels)
  , m_FaceNormals(__m_FaceNormals)
  , m_FaceAreas(__m_FaceAreas)
  {
    m_OrientationOps = LaueOps::GetAllOrientationOps();
    cryst = __m_CrystalStructures[__m_PhaseOfInterest];
    nsym = m_OrientationOps[cryst]->getNumSymOps();
  }

  virtual ~TrisSelector() = default;

  void select(size_t start, size_t end) const
  {
    float g1ea[3] = {0.0f, 0.0f, 0.0f};
    float g2ea[3] = {0.0f, 0.0f, 0.0f};

    float g1[3][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    float g2[3][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

    float normal_lab[3] = {0.0f, 0.0f, 0.0f};
    float normal_grain1[3] = {0.0f, 0.0f, 0.0f};
    float normal_grain2[3] = {0.0f, 0.0f, 0.0f};

    for(size_t triIdx = start; triIdx < end; triIdx++)
    {
      int32_t feature1 = m_FaceLabels[2 * triIdx];
      int32_t feature2 = m_FaceLabels[2 * triIdx + 1];

      if(feature1 < 1 || feature2 < 1)
      {
        continue;
      }
      if(m_Phases[feature1] != m_Phases[feature2])
      {
        continue;
      }
      if(m_Phases[feature1] != m_PhaseOfInterest || m_Phases[feature2] != m_PhaseOfInterest)
      {
        continue;
      }

      if(m_ExcludeTripleLines)
      {
        int64_t node1 = m_Triangles[triIdx * 3];
        int64_t node2 = m_Triangles[triIdx * 3 + 1];
        int64_t node3 = m_Triangles[triIdx * 3 + 2];

        if(m_NodeTypes[node1] != 2 || m_NodeTypes[node2] != 2 || m_NodeTypes[node3] != 2)
        {
          continue;
        }
      }

      normal_lab[0] = static_cast<float>(m_FaceNormals[3 * triIdx]);
      normal_lab[1] = static_cast<float>(m_FaceNormals[3 * triIdx + 1]);
      normal_lab[2] = static_cast<float>(m_FaceNormals[3 * triIdx + 2]);

      for(int whichEa = 0; whichEa < 3; whichEa++)
      {
        g1ea[whichEa] = m_Eulers[3 * feature1 + whichEa];
        g2ea[whichEa] = m_Eulers[3 * feature2 + whichEa];
      }

      OrientationTransformation::eu2om<OrientationF, OrientationF>(OrientationF(g1ea, 3)).toGMatrix(g1);
      OrientationTransformation::eu2om<OrientationF, OrientationF>(OrientationF(g2ea, 3)).toGMatrix(g2);

      MatrixMath::Multiply3x3with3x1(g1, normal_lab, normal_grain1);
      MatrixMath::Multiply3x3with3x1(g2, normal_lab, normal_grain2);

      (*selectedTris).push_back(TriAreaAndNormals(m_FaceAreas[triIdx], normal_grain1[0], normal_grain1[1], normal_grain1[2], -normal_grain2[0], -normal_grain2[1], -normal_grain2[2]));
    }
  }

#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    select(r.begin(), r.end());
  }
#endif
};

/**
 * @brief The ProbeDistrib class implements a threaded algorithm that determines the distribution values
 * for the GBPD
 */
class ProbeDistrib
{
  QVector<double>* distribValues;
  QVector<double>* errorValues;
  QVector<float>* samplPtsX;
  QVector<float>* samplPtsY;
  QVector<float>* samplPtsZ;
#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
  tbb::concurrent_vector<TriAreaAndNormals> selectedTris;
#else
  QVector<TriAreaAndNormals> selectedTris;
#endif
  float limitDist;
  double totalFaceArea;
  int numDistinctGBs;
  double ballVolume;
  LaueOpsContainer m_OrientationOps;
  uint32_t cryst;
  int32_t nsym;

public:
  ProbeDistrib(QVector<double>* __distribValues, QVector<double>* __errorValues, QVector<float>* __samplPtsX, QVector<float>* __samplPtsY, QVector<float>* __samplPtsZ,
#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
               tbb::concurrent_vector<TriAreaAndNormals> __selectedTris,
#else
               QVector<TriAreaAndNormals> __selectedTris,
#endif
               float __limitDist, double __totalFaceArea, int __numDistinctGBs, double __ballVolume, int32_t __cryst)
  : distribValues(__distribValues)
  , errorValues(__errorValues)
  , samplPtsX(__samplPtsX)
  , samplPtsY(__samplPtsY)
  , samplPtsZ(__samplPtsZ)
  , selectedTris(__selectedTris)
  , limitDist(__limitDist)
  , totalFaceArea(__totalFaceArea)
  , numDistinctGBs(__numDistinctGBs)
  , ballVolume(__ballVolume)
  , cryst(__cryst)
  {
    m_OrientationOps = LaueOps::GetAllOrientationOps();
    nsym = m_OrientationOps[__cryst]->getNumSymOps();
  }

  virtual ~ProbeDistrib() = default;

  void probe(size_t start, size_t end) const
  {
    for(size_t ptIdx = start; ptIdx < end; ptIdx++)
    {
      double __c = 0.0;

      float probeNormal[3] = {(*samplPtsX).at(ptIdx), (*samplPtsY).at(ptIdx), (*samplPtsZ).at(ptIdx)};

      for(int triRepresIdx = 0; triRepresIdx < static_cast<int>(selectedTris.size()); triRepresIdx++)
      {
        float normal1[3] = {selectedTris[triRepresIdx].normal_grain1_x, selectedTris[triRepresIdx].normal_grain1_y, selectedTris[triRepresIdx].normal_grain1_z};

        float normal2[3] = {selectedTris[triRepresIdx].normal_grain2_x, selectedTris[triRepresIdx].normal_grain2_y, selectedTris[triRepresIdx].normal_grain2_z};

        float sym[3][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

        for(int j = 0; j < nsym; j++)
        {
          m_OrientationOps[cryst]->getMatSymOp(j, sym);

          float sym_normal1[3] = {0.0f, 0.0f, 0.0f};
          float sym_normal2[3] = {0.0f, 0.0f, 0.0f};

          MatrixMath::Multiply3x3with3x1(sym, normal1, sym_normal1);
          MatrixMath::Multiply3x3with3x1(sym, normal2, sym_normal2);

          for(int inversion = 0; inversion <= 1; inversion++)
          {
            float sign = 1.0f;
            if(inversion == 1)
            {
              sign = -1.0f;
            }

            float gamma1 = acosf(sign * (probeNormal[0] * sym_normal1[0] + probeNormal[1] * sym_normal1[1] + probeNormal[2] * sym_normal1[2]));

            float gamma2 = acosf(sign * (probeNormal[0] * sym_normal2[0] + probeNormal[1] * sym_normal2[1] + probeNormal[2] * sym_normal2[2]));

            if(gamma1 < limitDist)
            {
              // Kahan summation algorithm
              double __y = selectedTris[triRepresIdx].area - __c;
              double __t = (*distribValues)[ptIdx] + __y;
              __c = (__t - (*distribValues)[ptIdx]);
              __c -= __y;
              (*distribValues)[ptIdx] = __t;
            }
            if(gamma2 < limitDist)
            {
              double __y = selectedTris[triRepresIdx].area - __c;
              double __t = (*distribValues)[ptIdx] + __y;
              __c = (__t - (*distribValues)[ptIdx]);
              __c -= __y;
              (*distribValues)[ptIdx] = __t;
            }
          }
        }
      }
      (*errorValues)[ptIdx] = sqrt((*distribValues)[ptIdx] / totalFaceArea / double(numDistinctGBs)) / ballVolume;
      (*distribValues)[ptIdx] /= totalFaceArea;
      (*distribValues)[ptIdx] /= ballVolume;
    }
  }

#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
  void operator()(const tbb::blocked_range<size_t>& r) const
  {
    probe(r.begin(), r.end());
  }
#endif
};

} // namespace GBPDMetricBased

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
FindGBPDMetricBased::FindGBPDMetricBased() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
FindGBPDMetricBased::~FindGBPDMetricBased() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setupFilterParameters()
{
  FilterParameterVectorType parameters;
  parameters.push_back(SIMPL_NEW_INTEGER_FP("Phase of Interest", PhaseOfInterest, FilterParameter::Category::Parameter, FindGBPDMetricBased));
  parameters.push_back(SIMPL_NEW_FLOAT_FP("Limiting Distance [deg.]", LimitDist, FilterParameter::Category::Parameter, FindGBPDMetricBased));
  parameters.push_back(SIMPL_NEW_INTEGER_FP("Number of Sampling Points (on a Hemisphere)", NumSamplPts, FilterParameter::Category::Parameter, FindGBPDMetricBased));
  parameters.push_back(SIMPL_NEW_BOOL_FP("Exclude Triangles Directly Neighboring Triple Lines", ExcludeTripleLines, FilterParameter::Category::Parameter, FindGBPDMetricBased));
  parameters.push_back(SIMPL_NEW_OUTPUT_FILE_FP("Output Distribution File", DistOutputFile, FilterParameter::Category::Parameter, FindGBPDMetricBased));
  parameters.push_back(SIMPL_NEW_OUTPUT_FILE_FP("Output Distribution Errors File", ErrOutputFile, FilterParameter::Category::Parameter, FindGBPDMetricBased));
  parameters.push_back(SIMPL_NEW_BOOL_FP("Save Relative Errors Instead of Their Absolute Values", SaveRelativeErr, FilterParameter::Category::Parameter, FindGBPDMetricBased));
  parameters.push_back(SeparatorFilterParameter::Create("Vertex Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Int8, 1, AttributeMatrix::Type::Face, IGeometry::Type::Triangle);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Node Types", NodeTypesArrayPath, FilterParameter::Category::RequiredArray, FindGBPDMetricBased, req));
  }
  parameters.push_back(SeparatorFilterParameter::Create("Face Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Int32, 2, AttributeMatrix::Type::Face, IGeometry::Type::Triangle);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Face Labels", SurfaceMeshFaceLabelsArrayPath, FilterParameter::Category::RequiredArray, FindGBPDMetricBased, req));
  }
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Double, 3, AttributeMatrix::Type::Face, IGeometry::Type::Triangle);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Face Normals", SurfaceMeshFaceNormalsArrayPath, FilterParameter::Category::RequiredArray, FindGBPDMetricBased, req));
  }
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Double, 1, AttributeMatrix::Type::Face, IGeometry::Type::Triangle);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Face Areas", SurfaceMeshFaceAreasArrayPath, FilterParameter::Category::RequiredArray, FindGBPDMetricBased, req));
  }
  parameters.push_back(SeparatorFilterParameter::Create("Face Feature Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req =
        DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Int32, 2, AttributeMatrix::Type::FaceFeature, IGeometry::Type::Triangle);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Feature Face Labels", SurfaceMeshFeatureFaceLabelsArrayPath, FilterParameter::Category::RequiredArray, FindGBPDMetricBased, req));
  }
  parameters.push_back(SeparatorFilterParameter::Create("Cell Feature Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req =
        DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Float, 3, AttributeMatrix::Type::CellFeature, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Average Euler Angles", FeatureEulerAnglesArrayPath, FilterParameter::Category::RequiredArray, FindGBPDMetricBased, req));
  }
  {
    DataArraySelectionFilterParameter::RequirementType req =
        DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Int32, 1, AttributeMatrix::Type::CellFeature, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Phases", FeaturePhasesArrayPath, FilterParameter::Category::RequiredArray, FindGBPDMetricBased, req));
  }
  parameters.push_back(SeparatorFilterParameter::Create("Cell Ensemble Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req =
        DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::UInt32, 1, AttributeMatrix::Type::CellEnsemble, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Crystal Structures", CrystalStructuresArrayPath, FilterParameter::Category::RequiredArray, FindGBPDMetricBased, req));
  }
  setFilterParameters(parameters);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindGBPDMetricBased::readFilterParameters(AbstractFilterParametersReader* reader, int index)
{
  reader->openFilterGroup(this, index);
  setPhaseOfInterest(reader->readValue("PhaseOfInterest", getPhaseOfInterest()));
  setLimitDist(reader->readValue("LimitDist", getLimitDist()));
  setNumSamplPts(reader->readValue("NumSamplPts", getNumSamplPts()));
  setExcludeTripleLines(reader->readValue("ExcludeTripleLines", getExcludeTripleLines()));
  setDistOutputFile(reader->readString("DistOutputFile", getDistOutputFile()));
  setErrOutputFile(reader->readString("ErrOutputFile", getErrOutputFile()));
  setSaveRelativeErr(reader->readValue("SaveRelativeErr", getSaveRelativeErr()));
  setCrystalStructuresArrayPath(reader->readDataArrayPath("CrystalStructures", getCrystalStructuresArrayPath()));
  setFeatureEulerAnglesArrayPath(reader->readDataArrayPath("FeatureEulerAngles", getFeatureEulerAnglesArrayPath()));
  setFeaturePhasesArrayPath(reader->readDataArrayPath("FeaturePhases", getFeaturePhasesArrayPath()));
  setSurfaceMeshFaceLabelsArrayPath(reader->readDataArrayPath("SurfaceMeshFaceLabels", getSurfaceMeshFaceLabelsArrayPath()));
  setSurfaceMeshFaceNormalsArrayPath(reader->readDataArrayPath("SurfaceMeshFaceNormals", getSurfaceMeshFaceNormalsArrayPath()));
  setSurfaceMeshFeatureFaceLabelsArrayPath(reader->readDataArrayPath("SurfaceMeshFeatureFaceLabels", getSurfaceMeshFeatureFaceLabelsArrayPath()));
  setSurfaceMeshFaceAreasArrayPath(reader->readDataArrayPath("SurfaceMeshFaceAreas", getSurfaceMeshFaceAreasArrayPath()));
  setNodeTypesArrayPath(reader->readDataArrayPath("NodeTypes", getNodeTypesArrayPath()));
  reader->closeFilterGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindGBPDMetricBased::initialize()
{
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindGBPDMetricBased::dataCheck()
{
  clearErrorCode();
  clearWarningCode();

  // Number of Sampling Points (filter params.)
  if(getNumSamplPts() < 1)
  {
    QString ss = QObject::tr("The number of sampling points must be greater than zero");
    setErrorCondition(-1000, ss);
  }

  // Set some reasonable value, but allow user to use more if he/she knows what he/she does
  if(getNumSamplPts() > 5000)
  {
    QString ss = QObject::tr("Most likely, you do not need to use that many sampling points");
    setWarningCondition(-1001, ss);
  }

  // Output files (filter params.)
  FileSystemPathHelper::CheckOutputFile(this, "Output Distribution File", getDistOutputFile(), true);
  FileSystemPathHelper::CheckOutputFile(this, "Output Error File", getErrOutputFile(), true);

  if(getErrorCode() < 0)
  {
    return;
  }

  QFileInfo distOutFileInfo(getDistOutputFile());
  QFileInfo errOutFileInfo(getErrOutputFile());
  if(distOutFileInfo.suffix().compare("") == 0)
  {
    setDistOutputFile(getDistOutputFile().append(".dat"));
  }
  if(errOutFileInfo.suffix().compare("") == 0)
  {
    setErrOutputFile(getErrOutputFile().append(".dat"));
  }

  // Make sure the file name ends with _1 so the GMT scripts work correctly
  QString distFName = distOutFileInfo.baseName();
  if(!distFName.endsWith("_1"))
  {
    distFName = distFName + "_1";
    QString absPath = distOutFileInfo.absolutePath() + "/" + distFName + ".dat";
    setDistOutputFile(absPath);
  }

  QString errFName = errOutFileInfo.baseName();
  if(!errFName.endsWith("_1"))
  {
    errFName = errFName + "_1";
    QString absPath = errOutFileInfo.absolutePath() + "/" + errFName + ".dat";
    setErrOutputFile(absPath);
  }

  if(!getDistOutputFile().isEmpty() && getDistOutputFile() == getErrOutputFile())
  {
    QString ss = QObject::tr("The output files must be different");
    setErrorCondition(-1006, ss);
  }

  // Crystal Structures
  std::vector<size_t> cDims(1, 1);
  m_CrystalStructuresPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<unsigned int>>(this, getCrystalStructuresArrayPath(), cDims);
  if(nullptr != m_CrystalStructuresPtr.lock())
  {
    m_CrystalStructures = m_CrystalStructuresPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Phase of Interest
  if(nullptr != m_CrystalStructuresPtr.lock())
  {
    if(getPhaseOfInterest() >= static_cast<int>(m_CrystalStructuresPtr.lock()->getNumberOfTuples()) || getPhaseOfInterest() <= 0)
    {
      QString ss = QObject::tr("The phase index is either larger than the number of Ensembles or smaller than 1");
      setErrorCondition(-1007, ss);
    }
  }

  // Euler Angels
  cDims[0] = 3;
  m_FeatureEulerAnglesPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<float>>(this, getFeatureEulerAnglesArrayPath(), cDims);
  if(nullptr != m_FeatureEulerAnglesPtr.lock())
  {
    m_FeatureEulerAngles = m_FeatureEulerAnglesPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Phases
  cDims[0] = 1;
  m_FeaturePhasesPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getFeaturePhasesArrayPath(), cDims);
  if(nullptr != m_FeaturePhasesPtr.lock())
  {
    m_FeaturePhases = m_FeaturePhasesPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Face Labels
  cDims[0] = 2;
  m_SurfaceMeshFaceLabelsPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getSurfaceMeshFaceLabelsArrayPath(), cDims);
  if(nullptr != m_SurfaceMeshFaceLabelsPtr.lock())
  {
    m_SurfaceMeshFaceLabels = m_SurfaceMeshFaceLabelsPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Face Normals
  cDims[0] = 3;
  m_SurfaceMeshFaceNormalsPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<double>>(this, getSurfaceMeshFaceNormalsArrayPath(), cDims);
  if(nullptr != m_SurfaceMeshFaceNormalsPtr.lock())
  {
    m_SurfaceMeshFaceNormals = m_SurfaceMeshFaceNormalsPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Face Areas
  cDims[0] = 1;
  m_SurfaceMeshFaceAreasPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<double>>(this, getSurfaceMeshFaceAreasArrayPath(), cDims);
  if(nullptr != m_SurfaceMeshFaceAreasPtr.lock())
  {
    m_SurfaceMeshFaceAreas = m_SurfaceMeshFaceAreasPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Feature Face Labels
  cDims[0] = 2;
  m_SurfaceMeshFeatureFaceLabelsPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int32_t>>(this, getSurfaceMeshFeatureFaceLabelsArrayPath(), cDims);
  if(nullptr != m_SurfaceMeshFeatureFaceLabelsPtr.lock())
  {
    m_SurfaceMeshFeatureFaceLabels = m_SurfaceMeshFeatureFaceLabelsPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */

  // Node Types
  cDims[0] = 1;
  m_NodeTypesPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<int8_t>>(this, getNodeTypesArrayPath(), cDims);
  if(nullptr != m_NodeTypesPtr.lock())
  {
    m_NodeTypes = m_NodeTypesPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindGBPDMetricBased::appendSamplPtsFixedZenith(QVector<float>* xVec, QVector<float>* yVec, QVector<float>* zVec, double theta, double minPhi, double maxPhi, double step)
{
  for(double phi = minPhi; phi <= maxPhi; phi += step)
  {
    (*xVec).push_back(sinf(static_cast<float>(theta)) * cosf(static_cast<float>(phi)));
    (*yVec).push_back(sinf(static_cast<float>(theta)) * sinf(static_cast<float>(phi)));
    (*zVec).push_back(cosf(static_cast<float>(theta)));
  }
  (*xVec).push_back(sinf(static_cast<float>(theta)) * cosf(static_cast<float>(maxPhi)));
  (*yVec).push_back(sinf(static_cast<float>(theta)) * sinf(static_cast<float>(maxPhi)));
  (*zVec).push_back(cosf(static_cast<float>(theta)));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindGBPDMetricBased::appendSamplPtsFixedAzimuth(QVector<float>* xVec, QVector<float>* yVec, QVector<float>* zVec, double phi, double minTheta, double maxTheta, double step)
{
  for(double theta = minTheta; theta <= maxTheta; theta += step)
  {
    (*xVec).push_back(sinf(static_cast<float>(theta)) * cosf(static_cast<float>(phi)));
    (*yVec).push_back(sinf(static_cast<float>(theta)) * sinf(static_cast<float>(phi)));
    (*zVec).push_back(cosf(static_cast<float>(theta)));
  }
  (*xVec).push_back(sinf(static_cast<float>(maxTheta)) * cosf(static_cast<float>(phi)));
  (*yVec).push_back(sinf(static_cast<float>(maxTheta)) * sinf(static_cast<float>(phi)));
  (*zVec).push_back(cosf(static_cast<float>(maxTheta)));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void FindGBPDMetricBased::execute()
{
  dataCheck();
  if(getErrorCode() < 0)
  {
    return;
  }

  m_LimitDist *= SIMPLib::Constants::k_PiOver180D;

  // We want to work with the raw pointers for speed so get those pointers.
  uint32_t* m_CrystalStructures = m_CrystalStructuresPtr.lock()->getPointer(0);
  float* m_Eulers = m_FeatureEulerAnglesPtr.lock()->getPointer(0);
  int32_t* m_Phases = m_FeaturePhasesPtr.lock()->getPointer(0);
  int32_t* m_FaceLabels = m_SurfaceMeshFaceLabelsPtr.lock()->getPointer(0);
  double* m_FaceNormals = m_SurfaceMeshFaceNormalsPtr.lock()->getPointer(0);
  double* m_FaceAreas = m_SurfaceMeshFaceAreasPtr.lock()->getPointer(0);
  int32_t* m_FeatureFaceLabels = m_SurfaceMeshFeatureFaceLabelsPtr.lock()->getPointer(0);
  int8_t* m_NodeTypes = m_NodeTypesPtr.lock()->getPointer(0);

  if(m_CrystalStructures[m_PhaseOfInterest] > 10)
  {
    QString ss = QObject::tr("Unsupported CrystalStructure");
    setErrorCondition(-1, ss);
    return;
  }

  DataContainer::Pointer sm = getDataContainerArray()->getDataContainer(getSurfaceMeshFaceAreasArrayPath().getDataContainerName());
  TriangleGeom::Pointer triangleGeom = sm->getGeometryAs<TriangleGeom>();
  SharedTriList::Pointer m_TrianglesPtr = triangleGeom->getTriangles();
  MeshIndexType* m_Triangles = m_TrianglesPtr->getPointer(0);

  // -------------------- check if directiories are ok and if output files can be opened -----------

  // Make sure any directory path is also available as the user may have just typed
  // in a path without actually creating the full path
  QFileInfo distOutFileInfo(getDistOutputFile());

  QDir distOutFileDir(distOutFileInfo.path());
  if(!distOutFileDir.mkpath("."))
  {
    QString ss;
    ss = QObject::tr("Error creating parent path '%1'").arg(distOutFileDir.path());
    setErrorCondition(-1, ss);
    return;
  }

  QFile distOutFile(getDistOutputFile());
  if(!distOutFile.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QString ss = QObject::tr("Error opening output file '%1'").arg(getDistOutputFile());
    setErrorCondition(-100, ss);
    return;
  }

  QFileInfo errOutFileInfo(getDistOutputFile());

  QDir errOutFileDir(errOutFileInfo.path());
  if(!errOutFileDir.mkpath("."))
  {
    QString ss;
    ss = QObject::tr("Error creating parent path '%1'").arg(errOutFileDir.path());
    setErrorCondition(-1, ss);
    return;
  }

  QFile errOutFile(getDistOutputFile());
  if(!errOutFile.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QString ss = QObject::tr("Error opening output file '%1'").arg(getDistOutputFile());
    setErrorCondition(-100, ss);
    return;
  }

  // Open the output files, should be opened and checked before starting computations
  FILE* fDist = nullptr;
  fDist = fopen(m_DistOutputFile.toLatin1().data(), "wb");
  if(nullptr == fDist)
  {
    QString ss = QObject::tr("Error opening distribution output file '%1'").arg(m_DistOutputFile);
    setErrorCondition(-1, ss);
    return;
  }

  FILE* fErr = nullptr;
  fErr = fopen(m_ErrOutputFile.toLatin1().data(), "wb");
  if(nullptr == fErr)
  {
    QString ss = QObject::tr("Error opening distribution errors output file '%1'").arg(m_ErrOutputFile);
    setErrorCondition(-1, ss);
    return;
  }

  // ------------------- before computing the distribution, we must find normalization factors -----
  std::vector<LaueOps::Pointer> m_OrientationOps = LaueOps::GetAllOrientationOps();
  int32_t cryst = m_CrystalStructures[m_PhaseOfInterest];
  int32_t nsym = m_OrientationOps[cryst]->getNumSymOps();
  double ballVolume = double(nsym) * 2.0 * (1.0 - cos(m_LimitDist));

  // ------------------------------ generation of sampling points ----------------------------------

  QString ss = QObject::tr("--> Generating sampling points");
  notifyStatusMessage(ss);

  // generate "Golden Section Spiral", see http://www.softimageblog.com/archives/115
  int numSamplPts_WholeSph = 2 * m_NumSamplPts; // here we generate points on the whole sphere
  QVector<float> samplPtsX_HemiSph(0);
  QVector<float> samplPtsY_HemiSph(0);
  QVector<float> samplPtsZ_HemiSph(0);

  QVector<float> samplPtsX(0);
  QVector<float> samplPtsY(0);
  QVector<float> samplPtsZ(0);

  float _inc = 2.3999632f; // = pi * (3 - sqrt(5))
  float _off = 2.0f / float(numSamplPts_WholeSph);

  for(int ptIdx_WholeSph = 0; ptIdx_WholeSph < numSamplPts_WholeSph; ptIdx_WholeSph++)
  {
    if(getCancel())
    {
      return;
    }

    float _y = (float(ptIdx_WholeSph) * _off) - 1.0f + (0.5f * _off);
    float _r = sqrtf(fmaxf(1.0f - _y * _y, 0.0f));
    float _phi = float(ptIdx_WholeSph) * _inc;

    float z = sinf(_phi) * _r;

    if(z >= 0.0f)
    {
      samplPtsX_HemiSph.push_back(cosf(_phi) * _r);
      samplPtsY_HemiSph.push_back(_y);
      samplPtsZ_HemiSph.push_back(z);
    }
  }

  // now, select the points from the SST
  for(int ptIdx_HemiSph = 0; ptIdx_HemiSph < samplPtsX_HemiSph.size(); ptIdx_HemiSph++)
  {
    if(getCancel())
    {
      return;
    }

    float x = samplPtsX_HemiSph[ptIdx_HemiSph];
    float y = samplPtsY_HemiSph[ptIdx_HemiSph];
    float z = samplPtsZ_HemiSph[ptIdx_HemiSph];

    if(cryst == 0) // 6/mmm
    {
      if(x < 0.0f || y < 0.0f || y > x * SIMPLib::Constants::k_1OverRoot3D)
      {
        continue;
      }
    }
    if(cryst == 1) // m-3m
    {
      if(y < 0.0f || x < y || z < x)
      {
        continue;
      }
    }
    if(cryst == 2 || cryst == 10) // 6/m || -3m
    {
      if(x < 0.0f || y < 0.0f || y > x * SIMPLib::Constants::k_Sqrt3D)
      {
        continue;
      }
    }
    if(cryst == 3) // m-3
    {
      if(x < 0.0f || y < 0.0f || z < x || z < y)
      {
        continue;
      }
    }
    // cryst = 4  =>  -1
    if(cryst == 5) // 2/m
    {
      if(y < 0.0f)
      {
        continue;
      }
    }
    if(cryst == 6 || cryst == 7) // mmm || 4/m
    {
      if(x < 0.0f || y < 0.0f)
      {
        continue;
      }
    }
    if(cryst == 8) // 4/mmm
    {
      if(x < 0.0f || y < 0.0f || y > x)
      {
        continue;
      }
    }
    if(cryst == 9) // -3
    {
      if(y < 0.0f || x < -y * SIMPLib::Constants::k_1OverRoot3D)
      {
        continue;
      }
    }

    samplPtsX.push_back(x);
    samplPtsY.push_back(y);
    samplPtsZ.push_back(z);
  }

  // Add points at the edges and vertices of a fundamental region
  const double deg = SIMPLib::Constants::k_PiOver180D;
  const double density = m_LimitDist;

  if(cryst == 0) // 6/mmm
  {
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 0.0, 0.0, 90.0 * deg, density);
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 30.0 * deg, 0.0, 90.0 * deg, density);
    appendSamplPtsFixedZenith(&samplPtsX, &samplPtsY, &samplPtsZ, 90.0 * deg, 0.0, 30.0 * deg, density);
  }
  if(cryst == 1) // m-3m
  {
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 0.0, 0.0, 45.0 * deg, density);
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 45.0 * deg, 0.0, acos(SIMPLib::Constants::k_1OverRoot3D), density);

    for(double phi = 0; phi <= 45.0f * deg; phi += density)
    {
      double cosPhi = cos(phi);
      double sinPhi = sin(phi);
      double atan1OverCosPhi = atan(1.0 / cosPhi);
      double sinAtan1OverCosPhi = sin(atan1OverCosPhi);
      double cosAtan1OverCosPhi = cos(atan1OverCosPhi);

      samplPtsX.push_back(static_cast<float>(sinAtan1OverCosPhi * cosPhi));
      samplPtsY.push_back(static_cast<float>(sinAtan1OverCosPhi * sinPhi));
      samplPtsZ.push_back(static_cast<float>(cosAtan1OverCosPhi));
    }
  }
  if(cryst == 2 || cryst == 10) // 6/m ||  -3m
  {
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 0.0, 0.0, 90.0 * deg, density);
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 60.0 * deg, 0.0, 90.0 * deg, density);
    appendSamplPtsFixedZenith(&samplPtsX, &samplPtsY, &samplPtsZ, 90.0 * deg, 0.0, 60.0 * deg, density);
  }
  if(cryst == 3) // m-3
  {
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 0.0, 0.0, 45.0 * deg, density);
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 90.0 * deg, 0.0, 45.0 * deg, density);
    for(double phi = 0; phi <= 45.0f * deg; phi += density)
    {
      double cosPhi = cos(phi);
      double sinPhi = sin(phi);
      double atan1OverCosPhi = atan(1.0 / cosPhi);
      double sinAtan1OverCosPhi = sin(atan1OverCosPhi);
      double cosAtan1OverCosPhi = cos(atan1OverCosPhi);

      samplPtsX.push_back(static_cast<float>(sinAtan1OverCosPhi * cosPhi));
      samplPtsY.push_back(static_cast<float>(sinAtan1OverCosPhi * sinPhi));
      samplPtsZ.push_back(static_cast<float>(cosAtan1OverCosPhi));

      samplPtsX.push_back(static_cast<float>(sinAtan1OverCosPhi * sinPhi));
      samplPtsY.push_back(static_cast<float>(sinAtan1OverCosPhi * cosPhi));
      samplPtsZ.push_back(static_cast<float>(cosAtan1OverCosPhi));
    }
  }
  if(cryst == 4) // -1
  {
    appendSamplPtsFixedZenith(&samplPtsX, &samplPtsY, &samplPtsZ, 90.0 * deg, 0.0, 360.0 * deg, density);
  }
  if(cryst == 5) // 2/m
  {
    appendSamplPtsFixedZenith(&samplPtsX, &samplPtsY, &samplPtsZ, 90.0 * deg, 0.0, 180.0 * deg, density);
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 0.0, -90.0 * deg, 90.0 * deg, density);
  }
  if(cryst == 6 || cryst == 7) // mmm || 4/m
  {
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 0.0, 0.0, 90.0 * deg, density);
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 90.0 * deg, 0.0, 90.0 * deg, density);
    appendSamplPtsFixedZenith(&samplPtsX, &samplPtsY, &samplPtsZ, 90.0 * deg, 0.0, 90.0 * deg, density);
  }
  if(cryst == 8) // 4/mmm
  {
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 0.0, 0.0, 90.0 * deg, density);
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 45.0 * deg, 0.0, 90.0 * deg, density);
    appendSamplPtsFixedZenith(&samplPtsX, &samplPtsY, &samplPtsZ, 90.0 * deg, 0.0, 45.0 * deg, density);
  }
  if(cryst == 9) // -3
  {
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 0.0, 0.0, 90.0 * deg, density);
    appendSamplPtsFixedAzimuth(&samplPtsX, &samplPtsY, &samplPtsZ, 120.0 * deg, 0.0, 90.0 * deg, density);
    appendSamplPtsFixedZenith(&samplPtsX, &samplPtsY, &samplPtsZ, 90.0 * deg, 0.0, 120.0 * deg, density);
  }

  // ---------  find triangles corresponding to Phase of Interests, and their normals in crystal reference frames ---------
  size_t numMeshTris = m_SurfaceMeshFaceAreasPtr.lock()->getNumberOfTuples();

#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
  bool doParallel = true;
  tbb::concurrent_vector<GBPDMetricBased::TriAreaAndNormals> selectedTris(0);
#else
  QVector<GBPDMetricBased::TriAreaAndNormals> selectedTris(0);
#endif

  size_t trisChunkSize = 50000;
  if(numMeshTris < trisChunkSize)
  {
    trisChunkSize = numMeshTris;
  }

  for(size_t i = 0; i < numMeshTris; i = i + trisChunkSize)
  {
    if(getCancel())
    {
      return;
    }
    ss = QObject::tr("--> Selecting triangles corresponding to Phase Of Interest");
    notifyStatusMessage(ss);
    if(i + trisChunkSize >= numMeshTris)
    {
      trisChunkSize = numMeshTris - i;
    }

#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
    if(doParallel)
    {
      tbb::parallel_for(tbb::blocked_range<size_t>(i, i + trisChunkSize),
                        GBPDMetricBased::TrisSelector(m_ExcludeTripleLines, m_Triangles, m_NodeTypes, &selectedTris, m_PhaseOfInterest, m_CrystalStructures, m_Eulers, m_Phases, m_FaceLabels,
                                                      m_FaceNormals, m_FaceAreas),
                        tbb::auto_partitioner());
    }
    else
#endif
    {
      GBPDMetricBased::TrisSelector serial(m_ExcludeTripleLines, m_Triangles, m_NodeTypes, &selectedTris, m_PhaseOfInterest, m_CrystalStructures, m_Eulers, m_Phases, m_FaceLabels, m_FaceNormals,
                                           m_FaceAreas);
      serial.select(i, i + trisChunkSize);
    }
  }

  // ------------------------  find the number of distinct boundaries ------------------------------
  int32_t numDistinctGBs = 0;
  int32_t numFaceFeatures = m_SurfaceMeshFeatureFaceLabelsPtr.lock()->getNumberOfTuples();

  for(int featureFaceIdx = 0; featureFaceIdx < numFaceFeatures; featureFaceIdx++)
  {
    int32_t feature1 = m_FeatureFaceLabels[2 * featureFaceIdx];
    int32_t feature2 = m_FeatureFaceLabels[2 * featureFaceIdx + 1];

    if(feature1 < 1 || feature2 < 1)
    {
      continue;
    }
    if(m_FeaturePhases[feature1] != m_FeaturePhases[feature2])
    {
      continue;
    }
    if(m_FeaturePhases[feature1] != m_PhaseOfInterest || m_FeaturePhases[feature2] != m_PhaseOfInterest)
    {
      continue;
    }

    numDistinctGBs++;
  }

  // ----------------- determining distribution values at the sampling points (and their errors) ---
  double totalFaceArea = 0.0;
  for(int i = 0; i < static_cast<int>(selectedTris.size()); i++)
  {
    totalFaceArea += selectedTris.at(i).area;
  }

  QVector<double> distribValues(samplPtsX.size(), 0.0);
  QVector<double> errorValues(samplPtsX.size(), 0.0);

  int32_t pointsChunkSize = 20;
  if(samplPtsX.size() < pointsChunkSize)
  {
    pointsChunkSize = samplPtsX.size();
  }

  for(int32_t i = 0; i < samplPtsX.size(); i = i + pointsChunkSize)
  {
    if(getCancel())
    {
      return;
    }
    ss = QObject::tr("--> Determining GBPD values (%1%)").arg(int(100.0 * float(i) / float(samplPtsX.size())));
    notifyStatusMessage(ss);
    if(i + pointsChunkSize >= samplPtsX.size())
    {
      pointsChunkSize = samplPtsX.size() - i;
    }

#ifdef SIMPL_USE_PARALLEL_ALGORITHMS
    if(doParallel)
    {
      tbb::parallel_for(tbb::blocked_range<size_t>(i, i + pointsChunkSize),
                        GBPDMetricBased::ProbeDistrib(&distribValues, &errorValues, &samplPtsX, &samplPtsY, &samplPtsZ, selectedTris, m_LimitDist, totalFaceArea, numDistinctGBs, ballVolume, cryst),
                        tbb::auto_partitioner());
    }
    else
#endif
    {
      GBPDMetricBased::ProbeDistrib serial(&distribValues, &errorValues, &samplPtsX, &samplPtsY, &samplPtsZ, selectedTris, m_LimitDist, totalFaceArea, numDistinctGBs, ballVolume, cryst);
      serial.probe(i, i + pointsChunkSize);
    }
  }

  // ------------------------------------------- writing the output --------------------------------
  fprintf(fDist, "%.1f %.1f %.1f %.1f\n", 0.0f, 0.0f, 0.0f, 0.0f);
  fprintf(fErr, "%.1f %.1f %.1f %.1f\n", 0.0f, 0.0f, 0.0f, 0.0f);

  for(int ptIdx = 0; ptIdx < samplPtsX.size(); ptIdx++)
  {
    float point[3] = {samplPtsX.at(ptIdx), samplPtsY.at(ptIdx), samplPtsZ.at(ptIdx)};
    float sym[3][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

    for(int j = 0; j < nsym; j++)
    {
      m_OrientationOps[cryst]->getMatSymOp(j, sym);
      float sym_point[3] = {0.0f, 0.0f, 0.0f};
      MatrixMath::Multiply3x3with3x1(sym, point, sym_point);

      if(sym_point[2] < 0.0f)
      {
        sym_point[0] = -sym_point[0];
        sym_point[1] = -sym_point[1];
        sym_point[2] = -sym_point[2];
      }

      float zenith = acosf(sym_point[2]);
      float azimuth = atan2f(sym_point[1], sym_point[0]);

      float zenithDeg = static_cast<float>(SIMPLib::Constants::k_180OverPiD * zenith);
      float azimuthDeg = static_cast<float>(SIMPLib::Constants::k_180OverPiD * azimuth);

      fprintf(fDist, "%.2f %.2f %.4f\n", azimuthDeg, 90.0f - zenithDeg, distribValues[ptIdx]);

      if(!m_SaveRelativeErr)
      {
        fprintf(fErr, "%.2f %.2f %.4f\n", azimuthDeg, 90.0f - zenithDeg, errorValues[ptIdx]);
      }
      else
      {
        double saneErr = 100.0;
        if(distribValues[ptIdx] > 1e-10)
        {
          saneErr = fmin(100.0, 100.0 * errorValues[ptIdx] / distribValues[ptIdx]);
        }
        fprintf(fErr, "%.2f %.2f %.2f\n", azimuthDeg, 90.0f - zenithDeg, saneErr);
      }
    }
  }
  fclose(fDist);
  fclose(fErr);

  if(getErrorCode() < 0)
  {
    QString ss = QObject::tr("Something went wrong");
    setErrorCondition(-1, ss);
    return;
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbstractFilter::Pointer FindGBPDMetricBased::newFilterInstance(bool copyFilterParameters) const
{
  FindGBPDMetricBased::Pointer filter = FindGBPDMetricBased::New();
  if(copyFilterParameters)
  {
    copyFilterParameterInstanceVariables(filter.get());
  }
  return filter;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindGBPDMetricBased::getCompiledLibraryName() const
{
  return OrientationAnalysisConstants::OrientationAnalysisBaseName;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindGBPDMetricBased::getBrandingString() const
{
  return "OrientationAnalysis";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindGBPDMetricBased::getFilterVersion() const
{
  QString version;
  QTextStream vStream(&version);
  vStream << OrientationAnalysis::Version::Major() << "." << OrientationAnalysis::Version::Minor() << "." << OrientationAnalysis::Version::Patch();
  return version;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindGBPDMetricBased::getGroupName() const
{
  return SIMPL::FilterGroups::StatisticsFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QUuid FindGBPDMetricBased::getUuid() const
{
  return QUuid("{00d20627-5b88-56ba-ac7a-fc2a4b337903}");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindGBPDMetricBased::getSubGroupName() const
{
  return SIMPL::FilterSubGroups::CrystallographyFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString FindGBPDMetricBased::getHumanLabel() const
{
  return "Find GBPD (Metric-Based Approach)";
}

// -----------------------------------------------------------------------------
FindGBPDMetricBased::Pointer FindGBPDMetricBased::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::shared_ptr<FindGBPDMetricBased> FindGBPDMetricBased::New()
{
  struct make_shared_enabler : public FindGBPDMetricBased
  {
  };
  std::shared_ptr<make_shared_enabler> val = std::make_shared<make_shared_enabler>();
  val->setupFilterParameters();
  return val;
}

// -----------------------------------------------------------------------------
QString FindGBPDMetricBased::getNameOfClass() const
{
  return QString("FindGBPDMetricBased");
}

// -----------------------------------------------------------------------------
QString FindGBPDMetricBased::ClassName()
{
  return QString("FindGBPDMetricBased");
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setPhaseOfInterest(int value)
{
  m_PhaseOfInterest = value;
}

// -----------------------------------------------------------------------------
int FindGBPDMetricBased::getPhaseOfInterest() const
{
  return m_PhaseOfInterest;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setLimitDist(float value)
{
  m_LimitDist = value;
}

// -----------------------------------------------------------------------------
float FindGBPDMetricBased::getLimitDist() const
{
  return m_LimitDist;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setNumSamplPts(int value)
{
  m_NumSamplPts = value;
}

// -----------------------------------------------------------------------------
int FindGBPDMetricBased::getNumSamplPts() const
{
  return m_NumSamplPts;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setExcludeTripleLines(bool value)
{
  m_ExcludeTripleLines = value;
}

// -----------------------------------------------------------------------------
bool FindGBPDMetricBased::getExcludeTripleLines() const
{
  return m_ExcludeTripleLines;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setDistOutputFile(const QString& value)
{
  m_DistOutputFile = value;
}

// -----------------------------------------------------------------------------
QString FindGBPDMetricBased::getDistOutputFile() const
{
  return m_DistOutputFile;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setErrOutputFile(const QString& value)
{
  m_ErrOutputFile = value;
}

// -----------------------------------------------------------------------------
QString FindGBPDMetricBased::getErrOutputFile() const
{
  return m_ErrOutputFile;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setSaveRelativeErr(bool value)
{
  m_SaveRelativeErr = value;
}

// -----------------------------------------------------------------------------
bool FindGBPDMetricBased::getSaveRelativeErr() const
{
  return m_SaveRelativeErr;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setCrystalStructuresArrayPath(const DataArrayPath& value)
{
  m_CrystalStructuresArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath FindGBPDMetricBased::getCrystalStructuresArrayPath() const
{
  return m_CrystalStructuresArrayPath;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setFeatureEulerAnglesArrayPath(const DataArrayPath& value)
{
  m_FeatureEulerAnglesArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath FindGBPDMetricBased::getFeatureEulerAnglesArrayPath() const
{
  return m_FeatureEulerAnglesArrayPath;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setFeaturePhasesArrayPath(const DataArrayPath& value)
{
  m_FeaturePhasesArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath FindGBPDMetricBased::getFeaturePhasesArrayPath() const
{
  return m_FeaturePhasesArrayPath;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setSurfaceMeshFaceLabelsArrayPath(const DataArrayPath& value)
{
  m_SurfaceMeshFaceLabelsArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath FindGBPDMetricBased::getSurfaceMeshFaceLabelsArrayPath() const
{
  return m_SurfaceMeshFaceLabelsArrayPath;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setSurfaceMeshFaceNormalsArrayPath(const DataArrayPath& value)
{
  m_SurfaceMeshFaceNormalsArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath FindGBPDMetricBased::getSurfaceMeshFaceNormalsArrayPath() const
{
  return m_SurfaceMeshFaceNormalsArrayPath;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setSurfaceMeshFaceAreasArrayPath(const DataArrayPath& value)
{
  m_SurfaceMeshFaceAreasArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath FindGBPDMetricBased::getSurfaceMeshFaceAreasArrayPath() const
{
  return m_SurfaceMeshFaceAreasArrayPath;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setSurfaceMeshFeatureFaceLabelsArrayPath(const DataArrayPath& value)
{
  m_SurfaceMeshFeatureFaceLabelsArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath FindGBPDMetricBased::getSurfaceMeshFeatureFaceLabelsArrayPath() const
{
  return m_SurfaceMeshFeatureFaceLabelsArrayPath;
}

// -----------------------------------------------------------------------------
void FindGBPDMetricBased::setNodeTypesArrayPath(const DataArrayPath& value)
{
  m_NodeTypesArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath FindGBPDMetricBased::getNodeTypesArrayPath() const
{
  return m_NodeTypesArrayPath;
}
