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

#pragma once

#include <memory>

#include "SIMPLib/SIMPLib.h"
#include "SIMPLib/DataArrays/DataArray.hpp"
#include "SIMPLib/Filtering/AbstractFilter.h"

#include "Processing/ProcessingDLLExport.h"

/**
 * @brief The FillBadData class. See [Filter documentation](@ref fillbaddata) for details.
 */
class Processing_EXPORT FillBadData : public AbstractFilter
{
  Q_OBJECT

  // Start Python bindings declarations
  PYB11_BEGIN_BINDINGS(FillBadData SUPERCLASS AbstractFilter)
  PYB11_FILTER()
  PYB11_SHARED_POINTERS(FillBadData)
  PYB11_FILTER_NEW_MACRO(FillBadData)
  PYB11_PROPERTY(bool StoreAsNewPhase READ getStoreAsNewPhase WRITE setStoreAsNewPhase)
  PYB11_PROPERTY(int MinAllowedDefectSize READ getMinAllowedDefectSize WRITE setMinAllowedDefectSize)
  PYB11_PROPERTY(DataArrayPath FeatureIdsArrayPath READ getFeatureIdsArrayPath WRITE setFeatureIdsArrayPath)
  PYB11_PROPERTY(DataArrayPath CellPhasesArrayPath READ getCellPhasesArrayPath WRITE setCellPhasesArrayPath)
  PYB11_END_BINDINGS()
  // End Python bindings declarations

public:
  using Self = FillBadData;
  using Pointer = std::shared_ptr<Self>;
  using ConstPointer = std::shared_ptr<const Self>;
  using WeakPointer = std::weak_ptr<Self>;
  using ConstWeakPointer = std::weak_ptr<const Self>;

  /**
   * @brief Returns a NullPointer wrapped by a shared_ptr<>
   * @return
   */
  static Pointer NullPointer();

  /**
   * @brief Creates a new object wrapped in a shared_ptr<>
   * @return
   */
  static Pointer New();

  /**
   * @brief Returns the name of the class for FillBadData
   */
  QString getNameOfClass() const override;
  /**
   * @brief Returns the name of the class for FillBadData
   */
  static QString ClassName();

  ~FillBadData() override;

  /**
   * @brief Setter property for StoreAsNewPhase
   */
  void setStoreAsNewPhase(bool value);
  /**
   * @brief Getter property for StoreAsNewPhase
   * @return Value of StoreAsNewPhase
   */
  bool getStoreAsNewPhase() const;
  Q_PROPERTY(bool StoreAsNewPhase READ getStoreAsNewPhase WRITE setStoreAsNewPhase)

  /**
   * @brief Setter property for MinAllowedDefectSize
   */
  void setMinAllowedDefectSize(int value);
  /**
   * @brief Getter property for MinAllowedDefectSize
   * @return Value of MinAllowedDefectSize
   */
  int getMinAllowedDefectSize() const;
  Q_PROPERTY(int MinAllowedDefectSize READ getMinAllowedDefectSize WRITE setMinAllowedDefectSize)

  /**
   * @brief Setter property for FeatureIdsArrayPath
   */
  void setFeatureIdsArrayPath(const DataArrayPath& value);
  /**
   * @brief Getter property for FeatureIdsArrayPath
   * @return Value of FeatureIdsArrayPath
   */
  DataArrayPath getFeatureIdsArrayPath() const;
  Q_PROPERTY(DataArrayPath FeatureIdsArrayPath READ getFeatureIdsArrayPath WRITE setFeatureIdsArrayPath)

  /**
   * @brief Setter property for CellPhasesArrayPath
   */
  void setCellPhasesArrayPath(const DataArrayPath& value);
  /**
   * @brief Getter property for CellPhasesArrayPath
   * @return Value of CellPhasesArrayPath
   */
  DataArrayPath getCellPhasesArrayPath() const;
  Q_PROPERTY(DataArrayPath CellPhasesArrayPath READ getCellPhasesArrayPath WRITE setCellPhasesArrayPath)

  /**
   * @brief Setter property for IgnoredDataArrayPaths
   */
  void setIgnoredDataArrayPaths(const std::vector<DataArrayPath>& value);
  /**
   * @brief Getter property for IgnoredDataArrayPaths
   * @return Value of IgnoredDataArrayPaths
   */
  std::vector<DataArrayPath> getIgnoredDataArrayPaths() const;
  Q_PROPERTY(DataArrayPathVec IgnoredDataArrayPaths READ getIgnoredDataArrayPaths WRITE setIgnoredDataArrayPaths)

  /**
   * @brief getCompiledLibraryName Reimplemented from @see AbstractFilter class
   */
  QString getCompiledLibraryName() const override;

  /**
   * @brief getBrandingString Returns the branding string for the filter, which is a tag
   * used to denote the filter's association with specific plugins
   * @return Branding string
   */
  QString getBrandingString() const override;

  /**
   * @brief getFilterVersion Returns a version string for this filter. Default
   * value is an empty string.
   * @return
   */
  QString getFilterVersion() const override;

  /**
   * @brief newFilterInstance Reimplemented from @see AbstractFilter class
   */
  AbstractFilter::Pointer newFilterInstance(bool copyFilterParameters) const override;

  /**
   * @brief getGroupName Reimplemented from @see AbstractFilter class
   */
  QString getGroupName() const override;

  /**
   * @brief getSubGroupName Reimplemented from @see AbstractFilter class
   */
  QString getSubGroupName() const override;

  /**
   * @brief getUuid Return the unique identifier for this filter.
   * @return A QUuid object.
   */
  QUuid getUuid() const override;

  /**
   * @brief getHumanLabel Reimplemented from @see AbstractFilter class
   */
  QString getHumanLabel() const override;

  /**
   * @brief setupFilterParameters Reimplemented from @see AbstractFilter class
   */
  void setupFilterParameters() override;

  /**
   * @brief execute Reimplemented from @see AbstractFilter class
   */
  void execute() override;

protected:
  FillBadData();
  /**
   * @brief dataCheck Checks for the appropriate parameter values and availability of arrays
   */
  void dataCheck() override;

  /**
   * @brief Initializes all the private instance variables.
   */
  void initialize();

private:
  std::weak_ptr<DataArray<int32_t>> m_FeatureIdsPtr;
  int32_t* m_FeatureIds = nullptr;
  std::weak_ptr<DataArray<int32_t>> m_CellPhasesPtr;
  int32_t* m_CellPhases = nullptr;

  bool m_StoreAsNewPhase = {false};
  int m_MinAllowedDefectSize = {1};
  DataArrayPath m_FeatureIdsArrayPath = {SIMPL::Defaults::ImageDataContainerName, SIMPL::Defaults::CellAttributeMatrixName, SIMPL::CellData::FeatureIds};
  DataArrayPath m_CellPhasesArrayPath = {SIMPL::Defaults::ImageDataContainerName, SIMPL::Defaults::CellAttributeMatrixName, SIMPL::CellData::Phases};
  std::vector<DataArrayPath> m_IgnoredDataArrayPaths = {};

  bool* m_AlreadyChecked;
  int32_t* m_Neighbors;

public:
  FillBadData(const FillBadData&) = delete;            // Copy Constructor Not Implemented
  FillBadData(FillBadData&&) = delete;                 // Move Constructor Not Implemented
  FillBadData& operator=(const FillBadData&) = delete; // Copy Assignment Not Implemented
  FillBadData& operator=(FillBadData&&) = delete;      // Move Assignment Not Implemented
};
