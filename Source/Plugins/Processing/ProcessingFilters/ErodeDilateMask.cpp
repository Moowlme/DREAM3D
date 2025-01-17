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
#include "ErodeDilateMask.h"

#include <QtCore/QTextStream>

#include "SIMPLib/Common/Constants.h"
#include "SIMPLib/DataContainers/DataContainer.h"
#include "SIMPLib/DataContainers/DataContainerArray.h"
#include "SIMPLib/FilterParameters/AbstractFilterParametersReader.h"
#include "SIMPLib/FilterParameters/BooleanFilterParameter.h"
#include "SIMPLib/FilterParameters/ChoiceFilterParameter.h"
#include "SIMPLib/FilterParameters/DataArraySelectionFilterParameter.h"
#include "SIMPLib/FilterParameters/IntFilterParameter.h"
#include "SIMPLib/FilterParameters/SeparatorFilterParameter.h"
#include "SIMPLib/Geometry/ImageGeom.h"

#include "Processing/ProcessingConstants.h"
#include "Processing/ProcessingVersion.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
ErodeDilateMask::ErodeDilateMask() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
ErodeDilateMask::~ErodeDilateMask() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ErodeDilateMask::setupFilterParameters()
{
  FilterParameterVectorType parameters;

  {
    ChoiceFilterParameter::Pointer parameter = ChoiceFilterParameter::New();
    parameter->setHumanLabel("Operation");
    parameter->setPropertyName("Direction");
    parameter->setSetterCallback(SIMPL_BIND_SETTER(ErodeDilateMask, this, Direction));
    parameter->setGetterCallback(SIMPL_BIND_GETTER(ErodeDilateMask, this, Direction));

    std::vector<QString> choices;
    choices.push_back("Dilate");
    choices.push_back("Erode");
    parameter->setChoices(choices);
    parameter->setCategory(FilterParameter::Category::Parameter);
    parameters.push_back(parameter);
  }
  parameters.push_back(SIMPL_NEW_INTEGER_FP("Number of Iterations", NumIterations, FilterParameter::Category::Parameter, ErodeDilateMask));
  parameters.push_back(SIMPL_NEW_BOOL_FP("X Direction", XDirOn, FilterParameter::Category::Parameter, ErodeDilateMask));
  parameters.push_back(SIMPL_NEW_BOOL_FP("Y Direction", YDirOn, FilterParameter::Category::Parameter, ErodeDilateMask));
  parameters.push_back(SIMPL_NEW_BOOL_FP("Z Direction", ZDirOn, FilterParameter::Category::Parameter, ErodeDilateMask));
  parameters.push_back(SeparatorFilterParameter::Create("Cell Data", FilterParameter::Category::RequiredArray));
  {
    DataArraySelectionFilterParameter::RequirementType req = DataArraySelectionFilterParameter::CreateRequirement(SIMPL::TypeNames::Bool, 1, AttributeMatrix::Type::Cell, IGeometry::Type::Image);
    parameters.push_back(SIMPL_NEW_DA_SELECTION_FP("Mask", MaskArrayPath, FilterParameter::Category::RequiredArray, ErodeDilateMask, req));
  }
  setFilterParameters(parameters);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ErodeDilateMask::readFilterParameters(AbstractFilterParametersReader* reader, int index)
{
  reader->openFilterGroup(this, index);
  setMaskArrayPath(reader->readDataArrayPath("MaskArrayPath", getMaskArrayPath()));
  setDirection(reader->readValue("Direction", getDirection()));
  setNumIterations(reader->readValue("NumIterations", getNumIterations()));
  setXDirOn(reader->readValue("XDirOn", getXDirOn()));
  setYDirOn(reader->readValue("YDirOn", getYDirOn()));
  setZDirOn(reader->readValue("ZDirOn", getZDirOn()));
  reader->closeFilterGroup();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ErodeDilateMask::initialize()
{
  m_MaskCopy = nullptr;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ErodeDilateMask::dataCheck()
{
  clearErrorCode();
  clearWarningCode();
  initialize();

  getDataContainerArray()->getPrereqGeometryFromDataContainer<ImageGeom>(this, getMaskArrayPath().getDataContainerName());

  if(getNumIterations() <= 0)
  {
    QString ss = QObject::tr("The number of iterations (%1) must be positive").arg(getNumIterations());
    setErrorCondition(-5555, ss);
  }

  std::vector<size_t> cDims(1, 1);
  m_MaskPtr = getDataContainerArray()->getPrereqArrayFromPath<DataArray<bool>>(this, getMaskArrayPath(), cDims);
  if(nullptr != m_MaskPtr.lock())
  {
    m_Mask = m_MaskPtr.lock()->getPointer(0);
  } /* Now assign the raw pointer to data from the DataArray<T> object */
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void ErodeDilateMask::execute()
{
  dataCheck();
  if(getErrorCode() < 0)
  {
    return;
  }

  DataContainer::Pointer m = getDataContainerArray()->getDataContainer(m_MaskArrayPath.getDataContainerName());
  size_t totalPoints = m_MaskPtr.lock()->getNumberOfTuples();

  BoolArrayType::Pointer maskCopyPtr = BoolArrayType::CreateArray(totalPoints, std::string("_INTERNAL_USE_ONLY_MaskCopy"), true);
  m_MaskCopy = maskCopyPtr->getPointer(0);
  maskCopyPtr->initializeWithValue(false);

  SizeVec3Type udims = m->getGeometryAs<ImageGeom>()->getDimensions();

  int64_t dims[3] = {
      static_cast<int64_t>(udims[0]),
      static_cast<int64_t>(udims[1]),
      static_cast<int64_t>(udims[2]),
  };

  int32_t good = 1;
  int64_t count = 0;
  int64_t kstride = 0, jstride = 0;
  int64_t neighpoint = 0;

  int64_t neighpoints[6] = {0, 0, 0, 0, 0, 0};
  neighpoints[0] = -dims[0] * dims[1];
  neighpoints[1] = -dims[0];
  neighpoints[2] = -1;
  neighpoints[3] = 1;
  neighpoints[4] = dims[0];
  neighpoints[5] = dims[0] * dims[1];

  for(int32_t iteration = 0; iteration < m_NumIterations; iteration++)
  {
    for(size_t j = 0; j < totalPoints; j++)
    {
      m_MaskCopy[j] = m_Mask[j];
    }
    for(int64_t k = 0; k < dims[2]; k++)
    {
      kstride = dims[0] * dims[1] * k;
      for(int64_t j = 0; j < dims[1]; j++)
      {
        jstride = dims[0] * j;
        for(int64_t i = 0; i < dims[0]; i++)
        {
          count = kstride + jstride + i;
          if(!m_Mask[count])
          {
            for(int32_t l = 0; l < 6; l++)
            {
              good = 1;
              neighpoint = count + neighpoints[l];
              if(l == 0 && (k == 0 || !m_ZDirOn))
              {
                good = 0;
              }
              else if(l == 5 && (k == (dims[2] - 1) || !m_ZDirOn))
              {
                good = 0;
              }
              else if(l == 1 && (j == 0 || !m_YDirOn))
              {
                good = 0;
              }
              else if(l == 4 && (j == (dims[1] - 1) || !m_YDirOn))
              {
                good = 0;
              }
              else if(l == 2 && (i == 0 || !m_XDirOn))
              {
                good = 0;
              }
              else if(l == 3 && (i == (dims[0] - 1) || !m_XDirOn))
              {
                good = 0;
              }
              if(good == 1)
              {
                if(m_Direction == 0 && m_Mask[neighpoint])
                {
                  m_MaskCopy[count] = true;
                }
                if(m_Direction == 1 && m_Mask[neighpoint])
                {
                  m_MaskCopy[neighpoint] = false;
                }
              }
            }
          }
        }
      }
    }
    for(size_t j = 0; j < totalPoints; j++)
    {
      m_Mask[j] = m_MaskCopy[j];
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
AbstractFilter::Pointer ErodeDilateMask::newFilterInstance(bool copyFilterParameters) const
{
  ErodeDilateMask::Pointer filter = ErodeDilateMask::New();
  if(copyFilterParameters)
  {
    copyFilterParameterInstanceVariables(filter.get());
  }
  return filter;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString ErodeDilateMask::getCompiledLibraryName() const
{
  return ProcessingConstants::ProcessingBaseName;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString ErodeDilateMask::getBrandingString() const
{
  return "Processing";
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString ErodeDilateMask::getFilterVersion() const
{
  QString version;
  QTextStream vStream(&version);
  vStream << Processing::Version::Major() << "." << Processing::Version::Minor() << "." << Processing::Version::Patch();
  return version;
}
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString ErodeDilateMask::getGroupName() const
{
  return SIMPL::FilterGroups::ProcessingFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QUuid ErodeDilateMask::getUuid() const
{
  return QUuid("{4fff1aa6-4f62-56c4-8ee9-8e28ec2fcbba}");
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString ErodeDilateMask::getSubGroupName() const
{
  return SIMPL::FilterSubGroups::CleanupFilters;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString ErodeDilateMask::getHumanLabel() const
{
  return "Erode/Dilate Mask";
}

// -----------------------------------------------------------------------------
ErodeDilateMask::Pointer ErodeDilateMask::NullPointer()
{
  return Pointer(static_cast<Self*>(nullptr));
}

// -----------------------------------------------------------------------------
std::shared_ptr<ErodeDilateMask> ErodeDilateMask::New()
{
  struct make_shared_enabler : public ErodeDilateMask
  {
  };
  std::shared_ptr<make_shared_enabler> val = std::make_shared<make_shared_enabler>();
  val->setupFilterParameters();
  return val;
}

// -----------------------------------------------------------------------------
QString ErodeDilateMask::getNameOfClass() const
{
  return QString("ErodeDilateMask");
}

// -----------------------------------------------------------------------------
QString ErodeDilateMask::ClassName()
{
  return QString("ErodeDilateMask");
}

// -----------------------------------------------------------------------------
void ErodeDilateMask::setDirection(unsigned int value)
{
  m_Direction = value;
}

// -----------------------------------------------------------------------------
unsigned int ErodeDilateMask::getDirection() const
{
  return m_Direction;
}

// -----------------------------------------------------------------------------
void ErodeDilateMask::setNumIterations(int value)
{
  m_NumIterations = value;
}

// -----------------------------------------------------------------------------
int ErodeDilateMask::getNumIterations() const
{
  return m_NumIterations;
}

// -----------------------------------------------------------------------------
void ErodeDilateMask::setXDirOn(bool value)
{
  m_XDirOn = value;
}

// -----------------------------------------------------------------------------
bool ErodeDilateMask::getXDirOn() const
{
  return m_XDirOn;
}

// -----------------------------------------------------------------------------
void ErodeDilateMask::setYDirOn(bool value)
{
  m_YDirOn = value;
}

// -----------------------------------------------------------------------------
bool ErodeDilateMask::getYDirOn() const
{
  return m_YDirOn;
}

// -----------------------------------------------------------------------------
void ErodeDilateMask::setZDirOn(bool value)
{
  m_ZDirOn = value;
}

// -----------------------------------------------------------------------------
bool ErodeDilateMask::getZDirOn() const
{
  return m_ZDirOn;
}

// -----------------------------------------------------------------------------
void ErodeDilateMask::setMaskArrayPath(const DataArrayPath& value)
{
  m_MaskArrayPath = value;
}

// -----------------------------------------------------------------------------
DataArrayPath ErodeDilateMask::getMaskArrayPath() const
{
  return m_MaskArrayPath;
}
