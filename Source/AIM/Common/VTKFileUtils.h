/* ============================================================================
 * Copyright (c) 2010, Michael A. Jackson (BlueQuartz Software)
 * All rights reserved.
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
 * Neither the name of Michael A. Jackson nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
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
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#ifndef _VTKFileUtils_h_
#define _VTKFileUtils_h_

#include <string>
#include <map>
#include <fstream>

#include <boost/shared_ptr.hpp>

#include "AIM/Common/AIMCommonConfiguration.h"

#define kBufferSize 1024

class SurfaceMeshFunc;

/**
 * @class VTKFileUtils VTKFileUtils.h application/VTKFileUtils.h
 * @brief This class contains some useful functions to read from and write to
 * VTK legacy style files.
 * @author Michael A. Jackson for BlueQuartz Software
 * @date Aug 13, 2010
 * @version 1.0
 */
class AIMCOMMON_EXPORT VTKFileUtils
{
  public:
    VTKFileUtils();
    virtual ~VTKFileUtils();

    /**
     * @brief
     * @param m
     * @param file
     * @param slice
     * @return
     */
    int readHeader(SurfaceMeshFunc* m, const std::string &file);


    int readZSlice(SurfaceMeshFunc* m, int zID);

//    int readNextZSlice(SurfaceMeshFunc* m, int zID);

    /**
     * @brief Reads a specific subset of VTK legacy ASCII or BINARY files
     * @param inputs The M3CInputs pointer
     * @return
     */
 //   int readVtkFile(SurfaceMeshFunc* func, const std::string &file);

/**
 * @brief Writes a VTK POLYDATA legacy ASCII file
 * @param m
 * @param nNodes
 * @param nTriangles
 * @param VisualizationFile
 * @param NodesFile
 * @param TrianglesFile
 * @param binaryFile
 * @return
 */
    int writeVTKFile(SurfaceMeshFunc* m,
                      int nNodes, int nTriangles,
                      const std::string &VisualizationFile,
                      const std::string &NodesFile,
                      const std::string &TrianglesFile,
                      bool binaryFile);

    /**
     * @brief This function parses 3 floating point values from a comma delimited string
     * @param input
     * @param output
     * @param defaultValue The value to set if the parsing fails
     * @return
     */
    int parseFloat3V(const char* input, float* output, float defaultValue);

    int readLine(std::istream &in, char* buf, int bufSize);


  private:

    std::ifstream m_InputFile;
    bool          m_fileIsBinary;
    bool          m_HeaderComplete;
  //  int           m_CurrentSlice;
    size_t        m_IntByteSize;

    VTKFileUtils(const VTKFileUtils&); // Copy Constructor Not Implemented
    void operator=(const VTKFileUtils&); // Operator '=' Not Implemented

};


#endif /* _VTKFileUtils.h_  */
