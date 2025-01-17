#--////////////////////////////////////////////////////////////////////////////
#-- Your License or copyright can go here
#--////////////////////////////////////////////////////////////////////////////

set(_filterGroupName SurfaceMeshingFilters)
set(${_filterGroupName}_FILTERS_HDRS "")

#--------
# This macro must come first before we start adding any filters
SIMPL_START_FILTER_GROUP(
  ALL_FILTERS_HEADERFILE ${AllFiltersHeaderFile}
  REGISTER_KNOWN_FILTERS_FILE ${RegisterKnownFiltersFile}
  FILTER_GROUP "${_filterGroupName}"
  BINARY_DIR ${${PLUGIN_NAME}_BINARY_DIR}
  )

#---------
# List your public filters here

set(_PublicFilters
  FindTriangleGeomCentroids
  FindTriangleGeomNeighbors
  FindTriangleGeomShapes
  FindTriangleGeomSizes
  LaplacianSmoothing
  QuickSurfaceMesh
  ReverseTriangleWinding
  SharedFeatureFaceFilter
  TriangleAreaFilter
  TriangleCentroidFilter
  TriangleDihedralAngleFilter
  TriangleNormalFilter
  GenerateGeometryConnectivity
)

if(SIMPL_USE_EIGEN)
  set(_PublicFilters ${_PublicFilters} FeatureFaceCurvatureFilter)

  ADD_SIMPL_SUPPORT_HEADER(${SurfaceMeshing_SOURCE_DIR} ${_filterGroupName} CalculateTriangleGroupCurvatures.h)
  ADD_SIMPL_SUPPORT_SOURCE(${SurfaceMeshing_SOURCE_DIR} ${_filterGroupName} CalculateTriangleGroupCurvatures.cpp)
endif()


list(LENGTH _PublicFilters PluginNumFilters)
set_property(GLOBAL PROPERTY PluginNumFilters ${PluginNumFilters})

#--------------
# Loop on all the filters adding each one. In this loop we default to making each filter exposed in the user
# interface in DREAM3D. If you want to have the filter compiled but NOT exposed to the user then use the next loop
foreach(f ${_PublicFilters} )
  ADD_SIMPL_FILTER(  "SurfaceMeshing" "SurfaceMeshing"
                        ${_filterGroupName} ${f}
                        ${SurfaceMeshing_SOURCE_DIR}/Documentation/${_filterGroupName}/${f}.md TRUE)
endforeach()


#---------------
# This is the list of Private Filters. These filters are available from other filters but the user will not
# be able to use them from the DREAM3D user interface.
set(_PrivateFilters

)

#-----------------
# Loop on the Private Filters adding each one to the DREAM3DLib project so that it gets compiled.
foreach(f ${_PrivateFilters} )
  ADD_SIMPL_FILTER(  "SurfaceMeshing" "SurfaceMeshing"
                        ${_filterGroupName} ${f}
                        ${${PLUGIN_NAME}_SOURCE_DIR}/Documentation/${_filterGroupName}/${f}.md FALSE)
endforeach()


#-------------
# These are files that need to be compiled into DREAM3DLib but are NOT filters
ADD_SIMPL_SUPPORT_MOC_HEADER(${SurfaceMeshing_SOURCE_DIR} ${_filterGroupName} BinaryNodesTrianglesReader.h)
ADD_SIMPL_SUPPORT_SOURCE(${SurfaceMeshing_SOURCE_DIR} ${_filterGroupName} BinaryNodesTrianglesReader.cpp)

ADD_SIMPL_SUPPORT_HEADER(${SurfaceMeshing_SOURCE_DIR} ${_filterGroupName} MeshFunctions.h)
ADD_SIMPL_SUPPORT_HEADER(${SurfaceMeshing_SOURCE_DIR} ${_filterGroupName} MeshLinearAlgebra.h)

ADD_SIMPL_SUPPORT_HEADER(${SurfaceMeshing_SOURCE_DIR} ${_filterGroupName} FindNRingNeighbors.h)
ADD_SIMPL_SUPPORT_SOURCE(${SurfaceMeshing_SOURCE_DIR} ${_filterGroupName} FindNRingNeighbors.cpp)

ADD_SIMPL_SUPPORT_HEADER(${SurfaceMeshing_SOURCE_DIR} ${_filterGroupName} util/TriangleOps.h)
ADD_SIMPL_SUPPORT_SOURCE(${SurfaceMeshing_SOURCE_DIR} ${_filterGroupName} util/TriangleOps.cpp)


SIMPL_END_FILTER_GROUP(${SurfaceMeshing_BINARY_DIR} "${_filterGroupName}" "Surface Meshing Filters")

