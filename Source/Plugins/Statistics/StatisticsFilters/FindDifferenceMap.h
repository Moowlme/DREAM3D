/*
 * Your License or Copyright can go here
 */

#ifndef _finddifferencemap_h_
#define _finddifferencemap_h_

#include "SIMPLib/Common/SIMPLibSetGetMacros.h"
#include "SIMPLib/Filtering/AbstractFilter.h"
#include "SIMPLib/SIMPLib.h"

/**
 * @brief The FindDifferenceMap class. See [Filter documentation](@ref finddifferencemap) for details.
 */
class FindDifferenceMap : public AbstractFilter
{
  Q_OBJECT

public:
  SIMPL_SHARED_POINTERS(FindDifferenceMap)
  SIMPL_STATIC_NEW_MACRO(FindDifferenceMap)
   SIMPL_TYPE_MACRO_SUPER_OVERRIDE(FindDifferenceMap, AbstractFilter)

  virtual ~FindDifferenceMap();

  SIMPL_FILTER_PARAMETER(DataArrayPath, FirstInputArrayPath)
  Q_PROPERTY(DataArrayPath FirstInputArrayPath READ getFirstInputArrayPath WRITE setFirstInputArrayPath)

  SIMPL_FILTER_PARAMETER(DataArrayPath, SecondInputArrayPath)
  Q_PROPERTY(DataArrayPath SecondInputArrayPath READ getSecondInputArrayPath WRITE setSecondInputArrayPath)

  SIMPL_FILTER_PARAMETER(DataArrayPath, DifferenceMapArrayPath)
  Q_PROPERTY(DataArrayPath DifferenceMapArrayPath READ getDifferenceMapArrayPath WRITE setDifferenceMapArrayPath)

  /**
   * @brief getCompiledLibraryName Reimplemented from @see AbstractFilter class
   */
  virtual const QString getCompiledLibraryName() const override;

  /**
   * @brief getBrandingString Returns the branding string for the filter, which is a tag
   * used to denote the filter's association with specific plugins
   * @return Branding string
  */
  virtual const QString getBrandingString() const override;

  /**
   * @brief getFilterVersion Returns a version string for this filter. Default
   * value is an empty string.
   * @return
   */
  virtual const QString getFilterVersion() const override;

  /**
   * @brief newFilterInstance Reimplemented from @see AbstractFilter class
   */
  virtual AbstractFilter::Pointer newFilterInstance(bool copyFilterParameters) const override;

  /**
   * @brief getGroupName Reimplemented from @see AbstractFilter class
   */
  virtual const QString getGroupName() const override;

  /**
   * @brief getSubGroupName Reimplemented from @see AbstractFilter class
   */
  virtual const QString getSubGroupName() const override;

  /**
   * @brief getUuid Return the unique identifier for this filter.
   * @return A QUuid object.
   */
  virtual const QUuid getUuid() override;

  /**
   * @brief getHumanLabel Reimplemented from @see AbstractFilter class
   */
  virtual const QString getHumanLabel() const override;

  /**
   * @brief setupFilterParameters Reimplemented from @see AbstractFilter class
   */
  virtual void setupFilterParameters() override;

  /**
   * @brief readFilterParameters Reimplemented from @see AbstractFilter class
   */
  virtual void readFilterParameters(AbstractFilterParametersReader* reader, int index);

  /**
   * @brief execute Reimplemented from @see AbstractFilter class
   */
  virtual void execute() override;

  /**
  * @brief preflight Reimplemented from @see AbstractFilter class
  */
  virtual void preflight() override;

signals:
  /**
   * @brief updateFilterParameters Emitted when the Filter requests all the latest Filter parameters
   * be pushed from a user-facing control (such as a widget)
   * @param filter Filter instance pointer
   */
  void updateFilterParameters(AbstractFilter* filter);

  /**
   * @brief parametersChanged Emitted when any Filter parameter is changed internally
   */
  void parametersChanged();

  /**
   * @brief preflightAboutToExecute Emitted just before calling dataCheck()
   */
  void preflightAboutToExecute();

  /**
   * @brief preflightExecuted Emitted just after calling dataCheck()
   */
  void preflightExecuted();

protected:
  FindDifferenceMap();
  /**
   * @brief dataCheck Checks for the appropriate parameter values and availability of arrays
   */
  void dataCheck();

  /**
   * @brief Initializes all the private instance variables.
   */
  void initialize();

private:
  DEFINE_IDATAARRAY_VARIABLE(FirstInputArray)
  DEFINE_IDATAARRAY_VARIABLE(SecondInputArray)
  DEFINE_IDATAARRAY_VARIABLE(DifferenceMap)

  FindDifferenceMap(const FindDifferenceMap&) = delete; // Copy Constructor Not Implemented
  void operator=(const FindDifferenceMap&);    // Operator '=' Not Implemented
};

#endif /* _FindDifferenceMap_H_ */
