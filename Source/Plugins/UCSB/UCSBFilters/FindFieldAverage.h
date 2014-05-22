/* ============================================================================
*
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#ifndef _FindFieldAverage_H_
#define _FindFieldAverage_H_

#include <string>

#include "DREAM3DLib/DREAM3DLib.h"
#include "DREAM3DLib/Common/DREAM3DSetGetMacros.h"
#include "DREAM3DLib/Common/AbstractFilter.h"

/**
* @class FindFieldAverage FindFieldAverage.h DREAM3DLib/Plugins/UCSB/UCSBFilters/FindFieldAverage.h
* @brief This filter creates a field array from a cell array taking an arithmatic average on a per component basis
* @author William Lenthe UCSB
* @date May 22, 2014
* @version 1.0
*/
class DREAM3DLib_EXPORT FindFieldAverage : public AbstractFilter
{
  public:
    DREAM3D_SHARED_POINTERS(FindFieldAverage)
    DREAM3D_STATIC_NEW_MACRO(FindFieldAverage)
    DREAM3D_TYPE_MACRO_SUPER(FindFieldAverage, AbstractFilter)
    virtual ~FindFieldAverage();

    DREAM3D_INSTANCE_STRING_PROPERTY(GrainIdsArrayName)
    DREAM3D_INSTANCE_STRING_PROPERTY(SelectedVoxelArrayName)

    virtual const std::string getGroupName() { return "UCSB"; }
    virtual const std::string getSubGroupName() { return DREAM3D::FilterSubGroups::MiscFilters; }
    virtual const std::string getHumanLabel() { return "Find Field Average Value"; }

    virtual void setupFilterParameters();
    /**
    * @brief This method will write the options to a file
    * @param writer The writer that is used to write the options to a file
    */
    virtual int writeFilterParameters(AbstractFilterParametersWriter* writer, int index);

    /**
    * @brief This method will read the options from a file
    * @param reader The reader that is used to read the options from a file
    */
    virtual void readFilterParameters(AbstractFilterParametersReader* reader, int index);

    /**
    * @brief Reimplemented from @see AbstractFilter class
    */
    virtual void preflight();
    virtual void execute();

  protected:
    FindFieldAverage();

    void dataCheck(bool preflight, size_t voxels, size_t fields, size_t ensembles);

  private:
    int32_t*  m_GrainIds;

    FindFieldAverage(const FindFieldAverage&); // Copy Constructor Not Implemented
    void operator=(const FindFieldAverage&); // Operator '=' Not Implemented
};

#endif /* RotateEulerRefFrame_H_ */
