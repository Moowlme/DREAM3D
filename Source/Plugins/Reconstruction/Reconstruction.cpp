/* ============================================================================
 * Copyright (c) 2010, Michael A. Jackson (BlueQuartz Software)
 * Copyright (c) 2010, Dr. Michael A. Grober (US Air Force Research Laboratories
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

#include "Reconstruction.h"

#include "MXA/MXATypes.h"
#include "MXA/Common/LogTime.h"
#include "MXA/Utilities/MXADir.h"


#include "EbsdLib/H5EbsdVolumeInfo.h"

#include "EbsdLib/TSL/AngDirectoryPatterns.h"
#include "EbsdLib/TSL/AngReader.h"
#include "EbsdLib/TSL/AngPhase.h"

#include "DREAM3D/DREAM3DConfiguration.h"
#include "DREAM3D/Common/Constants.h"
#include "DREAM3D/Common/OIMColoring.hpp"
#include "DREAM3D/Common/VTKUtils/VTKFileWriters.hpp"
#include "DREAM3D/Common/DxGrainIdWriter.h"
#include "DREAM3D/Common/PhWriter.hpp"
#include "DREAM3D/HDF5/H5VoxelWriter.h"
#include "DREAM3D/HDF5/H5GrainWriter.hpp"


#include "Reconstruction/EbsdSupport/H5EbsdVolumeReader.h"
#include "Reconstruction/EbsdSupport/H5AngVolumeReader.h"
#include "Reconstruction/EbsdSupport/H5CtfVolumeReader.h"



// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
Reconstruction::Reconstruction() :
m_H5AngFile(""),
m_OutputDirectory("."),
m_OutputFilePrefix("Reconstruction_"),
m_MergeTwins(false),
m_MergeColonies(false),
m_FillinSample(false),
m_MinAllowedGrainSize(0),
m_MinSeedConfidence(0.0),
m_MinSeedImageQuality(0.0),
m_MisorientationTolerance(0.0),
m_Orientation(Ebsd::NoOrientation),
m_WriteBinaryVTKFiles(true),
m_WriteVtkFile(true),
m_WritePhaseId(true),
m_WriteImageQuality(true),
m_WriteIPFColor(true),
m_WriteDownSampledFile(false),
m_WriteHDF5GrainFile(false),
m_WriteDxFile(false)
{

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
Reconstruction::~Reconstruction()
{
}

#define MAKE_OUTPUT_FILE_PATH(outpath, filename)\
    std::string outpath = m_OutputDirectory + MXADir::Separator + m_OutputFilePrefix + filename;



// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void Reconstruction::execute()
{
  int err = -1;

  // Create the ReconstructionFunc object
  m = ReconstructionFunc::New();

 // updateProgressAndMessage(("Gathering Size and Resolution Information from OIM Data"), 1);
  std::string manufacturer;
  // Get the Size and Resolution of the Volume
  {
    H5EbsdVolumeInfo::Pointer volumeInfoReader = H5EbsdVolumeInfo::New();
    volumeInfoReader->setFilename(m_H5AngFile);
    err = volumeInfoReader->readVolumeInfo();
    if (err < 0)
    {
      updateProgressAndMessage("Error reading Volume Information from File.", 100);
      return;
    }
    volumeInfoReader->getDimsAndResolution(m->xpoints, m->ypoints, m->zpoints, m->resx, m->resy, m->resz);
    //Now Calculate our "subvolume" of slices, ie, those start and end values that the user selected from the GUI
    // The GUI code has already added 1 to the end index so nothing special needs to be done
    // for this calculation
    m->zpoints = getZEndIndex() - getZStartIndex();
    manufacturer = volumeInfoReader->getManufacturer();
    volumeInfoReader = H5EbsdVolumeInfo::NullPointer();
  }
  H5EbsdVolumeReader::Pointer ebsdReader;
  std::vector<float> precipFractions;
  std::vector<Ebsd::CrystalStructure> crystalStructures;
  if (manufacturer.compare(Ebsd::Ang::Manufacturer) == 0)
  {
    ebsdReader = H5AngVolumeReader::New();
    if (NULL == ebsdReader)
    {
      updateProgressAndMessage("Error Creating the proper Ebsd Volume Reader.", 100);
      return;
    }
    H5AngVolumeReader* angReader = dynamic_cast<H5AngVolumeReader*>(ebsdReader.get());
    err = loadInfo<H5AngVolumeReader, AngPhase>(angReader, precipFractions, crystalStructures );
  }
  else if (manufacturer.compare(Ebsd::Ctf::Manufacturer) == 0)
  {
    ebsdReader = H5CtfVolumeReader::New();
    if (NULL == ebsdReader)
    {
      updateProgressAndMessage("Error Creating the proper Ebsd Volume Reader.", 100);
      return;
    }
    H5CtfVolumeReader* ctfReader = dynamic_cast<H5CtfVolumeReader*>(ebsdReader.get());
    err = loadInfo<H5CtfVolumeReader, CtfPhase>(ctfReader, precipFractions, crystalStructures );
  }
  else
  {
    updateProgressAndMessage("ReconstructionFunc Error: No Manufacturer Set for EBSD data", 100);
    return;
  }

  if (err < 0)
  {
    updateProgressAndMessage("ReconstructionFunc Error: Phase and Crystal Structure Information", 100);
     return;
  }


  m->initialize(m->xpoints, m->ypoints, m->zpoints,
                m->resx, m->resy, m->resz, m_MergeTwins, m_MergeColonies, m_MinAllowedGrainSize,
                m_MinSeedConfidence, m_DownSampleFactor, m_MinSeedImageQuality,
                m_MisorientationTolerance, crystalStructures, m_PhaseTypes, precipFractions,
                m_AlignmentMethod);
  m_OutputDirectory = MXADir::toNativeSeparators(m_OutputDirectory);

  START_CLOCK()

  updateProgressAndMessage(("Loading Slices"), 4);
  ebsdReader->setSliceStart(m_ZStartIndex);
  ebsdReader->setSliceEnd(m_ZEndIndex);
  err = ebsdReader->loadData(m.get());
  CHECK_FOR_ERROR(ReconstructionFunc, "Reconstruction was canceled", err)

  m->initializeQuats();
  CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", loadData)

  updateProgressAndMessage(("Finding Border"), 8);
  m->find_border();
  CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", find_border)

  if (m_AlignmentMethod == AIM::Reconstruction::MutualInformation)
  {
      updateProgressAndMessage(("Aligning Slices"), 10);
      m->form_grains_sections();
      CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", form_grains_sections)
  }

  updateProgressAndMessage(("Aligning Slices"), 12);
  m->align_sections();
  CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", align_sections)

  updateProgressAndMessage(("Cleaning Data"), 16);
  m->cleanup_data();
  CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", cleanup_data)

  if (m_AlignmentMethod == AIM::Reconstruction::MutualInformation)
  {
      updateProgressAndMessage(("Redefining Border"), 18);
      m->find_border();
      CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", find_border)
  }

  updateProgressAndMessage(("Forming Macro-Grains"), 20);
  m->form_grains();
  CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", form_grains)

  updateProgressAndMessage(("Finding Neighbors"), 24);
  m->find_neighbors();
  CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", find_neighbors)

  updateProgressAndMessage(("Reordering Grains"), 28);
  m->reorder_grains();
  CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", reorder_grains)

  if(m_FillinSample == true)
  {
    updateProgressAndMessage(("Creating Smooth Rectangular Sample"), 36);
    m->fillin_sample();
    CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", fillin_sample)
  }

  if (m_MergeTwins == true)
  {
    updateProgressAndMessage(("Merging Twins"), 40);
    m->merge_twins();
    CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", merge_twins)

    updateProgressAndMessage(("Characterizing Twins"), 40);
    m->characterize_twins();
    CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", characterize_twins)

    updateProgressAndMessage(("Renumbering Grains"), 40);
    m->renumber_grains3();
    CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", renumber_grains3)
  }

  if (m_MergeColonies == true)
  {
    updateProgressAndMessage(("Merging Colonies"), 44);
    m->merge_colonies();
    CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", merge_colonies)

    updateProgressAndMessage(("Renumbering Grains"), 44);
    m->characterize_colonies();
    CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", characterize_colonies)
  }


  /** ********** This section writes the Voxel Data for the Stats Module *** */
  // Create a new HDF5 Volume file by overwriting any HDF5 file that may be in the way
  MAKE_OUTPUT_FILE_PATH ( hdf5VolumeFile, AIM::Reconstruction::H5VoxelFile)
  H5VoxelWriter::Pointer h5VolWriter = H5VoxelWriter::New();
  if (h5VolWriter.get() == NULL)
  {
    updateProgressAndMessage("The HDF5 Voxel file could not be created. Does the path exist and do you have write access to the output directory.", 100);
    m = ReconstructionFunc::NullPointer();  // Clean up the memory
    return;
  }
  h5VolWriter->setFilename(hdf5VolumeFile);
  updateProgressAndMessage(("Writing HDF5 Voxel Data File"), 83);
  err = h5VolWriter->writeVoxelData<ReconstructionFunc>(m.get());
  CHECK_FOR_ERROR(ReconstructionFunc, "The HDF5 Voxel file could not be written to. Does the path exist and do you have write access to the output directory.", err);


  /** ********** This section writes the VTK files for visualization *** */
  updateProgressAndMessage(("Writing VTK Visualization File"), 93);
  if (m_WriteVtkFile)
  {
    MAKE_OUTPUT_FILE_PATH ( reconVisFile, AIM::Reconstruction::VisualizationVizFile);

    // Setup all the classes that will help us write the Scalars to the VTK File
    std::vector<VtkScalarWriter*> scalarsToWrite;
    {
      VtkScalarWriter* w0 =
          static_cast<VtkScalarWriter*>(new VoxelGrainIdScalarWriter<ReconstructionFunc>(m.get()));
      w0->m_WriteBinaryFiles = m_WriteBinaryVTKFiles;
      scalarsToWrite.push_back(w0);
    }

    if (m_WritePhaseId == true){
      VtkScalarWriter* w0 =
        static_cast<VtkScalarWriter*>(new VoxelPhaseIdScalarWriter<ReconstructionFunc>(m.get()));
      w0->m_WriteBinaryFiles = m_WriteBinaryVTKFiles;
      scalarsToWrite.push_back(w0);
    }

    if (m_WriteImageQuality == true) {
      VtkScalarWriter* w0 =
        static_cast<VtkScalarWriter*>(new VoxelImageQualityScalarWriter<ReconstructionFunc>(m.get()));
      w0->m_WriteBinaryFiles = m_WriteBinaryVTKFiles;
      scalarsToWrite.push_back(w0);
    }

    if (m_WriteIPFColor == true) {
      VtkScalarWriter* w0 =
        static_cast<VtkScalarWriter*>(new VoxelIPFColorScalarWriter<ReconstructionFunc>(m.get()));
      w0->m_WriteBinaryFiles = m_WriteBinaryVTKFiles;
      scalarsToWrite.push_back(w0);
    }

    // Create our File Output Writer Object. This will handle all the File Output duties
    VTKRectilinearGridFileWriter vtkWriter;
    vtkWriter.setWriteBinaryFiles(m_WriteBinaryVTKFiles);
    err = vtkWriter.write<ReconstructionFunc>(reconVisFile, m.get(), scalarsToWrite);

    // Now Delete all the Scalar Helpers that we just created and used.
    for (std::vector<VtkScalarWriter*>::iterator iter = scalarsToWrite.begin(); iter != scalarsToWrite.end(); ++iter )
    {
      delete (*iter);
    }

    CHECK_FOR_ERROR(ReconstructionFunc, "The VTK file could not be written to. Does the path exist and do you have write access to the output directory.", err);
  }


  if (m_WriteDownSampledFile) {
    MAKE_OUTPUT_FILE_PATH ( reconDSVisFile, AIM::Reconstruction::DownSampledVizFile);
    updateProgressAndMessage(("Writing VTK Down Sampled File"), 98);
    VtkMiscFileWriter::Pointer vtkWriter = VtkMiscFileWriter::New();
    err = vtkWriter->writeDownSampledVizFile(m.get(), reconDSVisFile);
    CHECK_FOR_ERROR(ReconstructionFunc, "The VTK Downsampled file could not be written to. Does the path exist and do you have write access to the output directory.", err);
  }

  /** ******* End VTK Visualization File Writing Section ****** */

  CHECK_FOR_CANCELED(ReconstructionFunc, "Reconstruction was canceled", vtk_viz_files)

  /* **********   This is CMU's ph format */
  updateProgressAndMessage(("Writing Ph Voxel File"), 95);
  if (m_WritePhFile) {
    MAKE_OUTPUT_FILE_PATH ( phFile, AIM::Reconstruction::PhFile);
    PhWriter phWriter;
    err = phWriter.writeGrainPhFile(phFile, m->grain_indicies, m->xpoints, m->ypoints, m->zpoints);
    CHECK_FOR_ERROR(ReconstructionFunc, "The Reconstruction threw an Error writing the Ph file format.", err);
  }


  /* ********** Optionally write the .h5grain file */
  if (m_WriteHDF5GrainFile)
  {
    updateProgressAndMessage(("Writing Out HDF5 Grain File. This may take a few minutes to complete."), 99);
    MAKE_OUTPUT_FILE_PATH( hdf5GrainFile, AIM::Reconstruction::HDF5GrainFile);
    H5GrainWriter::Pointer h5GrainWriter = H5GrainWriter::New();
    err = h5GrainWriter->writeHDF5GrainsFile<ReconstructionFunc>(m.get(), hdf5GrainFile);
    CHECK_FOR_ERROR(ReconstructionFunc, "The HDF5 Grain file could not be written to. Does the path exist and do you have write access to the output directory.", err);
  }

  /* **********   This is IBM's dx format */
  if (m_WriteDxFile)
  {
    updateProgressAndMessage(("Writing Out Dx Grain File."), 99);
    MAKE_OUTPUT_FILE_PATH( dxGrainFile, AIM::Reconstruction::DxFile);
    DxGrainIdWriter::Pointer dxWriter = DxGrainIdWriter::New();
//    err = dxWriter->writeGrainFile(dxGrainFile, m->voxels.get(), m->xpoints, m->ypoints, m->zpoints);
    CHECK_FOR_ERROR(ReconstructionFunc, "The Dx Grain file could not be written to. Does the path exist and do you have write access to the output directory.", err);
  }

  updateProgressAndMessage(("Reconstruction Complete"), 100);

  // Clean up all the memory by forcibly setting a NULL pointer to the Shared
  // pointer object.
  m = ReconstructionFunc::NullPointer();  // Clean up the memory
  //std::cout << "Reconstruction::compute Complete" << std::endl;

}



#define PRINT_PROPERTY( out, var)\
  out << #var << ": " << m_##var << std::endl;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void Reconstruction::printSettings(std::ostream &ostream)
{
  ostream << "Reconstruction Settings Being Used" << std::endl;
    PRINT_PROPERTY(ostream, H5AngFile)
    PRINT_PROPERTY(ostream, ZStartIndex)
    PRINT_PROPERTY(ostream, ZEndIndex)
    PRINT_PROPERTY(ostream, OutputDirectory)
    PRINT_PROPERTY(ostream, MergeTwins)
    PRINT_PROPERTY(ostream, MergeColonies)
    PRINT_PROPERTY(ostream, FillinSample)
    PRINT_PROPERTY(ostream, MinAllowedGrainSize)
    PRINT_PROPERTY(ostream, MinSeedConfidence)
    PRINT_PROPERTY(ostream, DownSampleFactor)
    PRINT_PROPERTY(ostream, MinSeedImageQuality)
    PRINT_PROPERTY(ostream, MisorientationTolerance)
    PRINT_PROPERTY(ostream, AlignmentMethod)
    PRINT_PROPERTY(ostream, Orientation)

    PRINT_PROPERTY(ostream, WriteVtkFile)
    PRINT_PROPERTY(ostream, WritePhaseId)
    PRINT_PROPERTY(ostream, WriteImageQuality)
    PRINT_PROPERTY(ostream, WriteIPFColor)
    PRINT_PROPERTY(ostream, WriteDownSampledFile)
    PRINT_PROPERTY(ostream, WriteHDF5GrainFile)
}
