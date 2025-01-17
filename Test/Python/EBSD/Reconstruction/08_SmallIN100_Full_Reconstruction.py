# Pipeline : (08) SmallIN100 Full Reconstruction (from EBSD Reconstruction)

import simpl
import simplpy as d3d
import simpl_helpers as sh
import simpl_test_dirs as sd
import orientationanalysispy as orientation_analysis
import reconstructionpy as reconstruction
import processingpy as processing
import genericpy as generic
import statstoolboxpy

def small_in100_full_reconstruction():
    # Create Data Container Array
    dca = simpl.DataContainerArray()

    # Read H5EBSD File
    err = orientation_analysis.read_h5_ebsd(dca, 'Small IN100', 'Phase Data', 'EBSD Scan Data',
                                            sd.GetBuildDirectory() +
                                            '/Data/Output/Reconstruction/SmallIN100.h5ebsd',
                                            1, 117, True, sh.AngleRepresentation.Radians,
                                            simpl.StringSet({'Fit', 'Image Quality', 'EulerAngles',
                                                             'SEM Signal', 'Confidence Index', 'Phases'}))
    assert err == 0, f'ReadH5Ebsd ErrorCondition {err}'

    # Threshold Objects
    # Create the selected thresholds / comparison inputs for MultiThresholdObjects filter
    selectedThresholds = simpl.ComparisonInputs()
    selectedThresholds.addInput('Small IN100', 'EBSD Scan Data', 'Confidence Index',
                                simpl.ComparisonOperators.GreaterThan, 0.1)
    selectedThresholds.addInput('Small IN100', 'EBSD Scan Data', 'Image Quality', simpl.ComparisonOperators.GreaterThan,
                                120)

    err = d3d.multi_threshold_objects(dca, 'Mask', selectedThresholds)
    assert err == 0, f'MultiThresholdObjects ErrorCondition: {err}'

    # Convert Orientation Representation
    err = orientation_analysis.convert_orientations(dca, 0, 2,
                                                    simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'EulerAngles'),
                                                    'Quats')
    assert err == 0, f'ConvertOrientation ErrorCondition: {err}'

    # Align Sections (Misorientation)
    err = reconstruction.align_sections_misorientation(dca, 5, True,
                                                       simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Quats'),
                                                       simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Phases'),
                                                       simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Mask'),
                                                       simpl.DataArrayPath('Small IN100', 'Phase Data',
                                                                           'CrystalStructures'))
    assert err == 0, f'AlignSectionsMisorientation ErrorCondition: {err}'

    # Isolate Largest Feature (Identify Sample)
    err = processing.identify_sample(dca, False, simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Mask'))
    assert err == 0, f'IsolateLargestFeature ErrorCondition: {err}'

    # Align Sections (Feature Centroid)
    err = reconstruction.align_sections_feature_centroid(dca,
                                                         0, True,
                                                         simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Mask'))
    assert err == 0, f'AlignSectionsFeatureCentroid {err}'

    # Neighbor Orientation Comparison (Bad Data)
    err = orientation_analysis.bad_data_neighbor_orientation_check(dca, 5, 4,
                                                                   simpl.DataArrayPath('Small IN100', 'EBSD Scan Data',
                                                                                       'Mask'),
                                                                   simpl.DataArrayPath('Small IN100', 'EBSD Scan Data',
                                                                                       'Phases'),
                                                                   simpl.DataArrayPath('Small IN100', 'Phase Data',
                                                                                       'CrystalStructures'),
                                                                   simpl.DataArrayPath('Small IN100', 'EBSD Scan Data',
                                                                                       'Quats'))
    assert err == 0, f'NeighborOrientationComparison ErrorCondition {err}'

    # Neighbor Orientation Correlation
    err = orientation_analysis.neighbor_orientation_correlation(dca, 5, 0.2, 2,
                                                                simpl.DataArrayPath('Small IN100', 'EBSD Scan Data',
                                                                                    'Confidence Index'),
                                                                simpl.DataArrayPath('Small IN100', 'EBSD Scan Data',
                                                                                    'Phases'),
                                                                simpl.DataArrayPath('Small IN100', 'Phase Data',
                                                                                    'CrystalStructures'),
                                                                simpl.DataArrayPath('Small IN100', 'EBSD Scan Data',
                                                                                    'Quats'))
    assert err == 0, f'NeighborOrientationCorrelation ErrorCondition {err}'

    # Segment Features (Misorientation)
    err = reconstruction.ebsd_segment_features(dca, 'Grain Data', 5, True,
                                               simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Mask'),
                                               simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Phases'),
                                               simpl.DataArrayPath('Small IN100', 'Phase Data', 'CrystalStructures'),
                                               simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Quats'),
                                               'FeatureIds', 'Active')
    assert err == 0, f'SegmentFeatures ErrorCondition {err}'

    # Find Feature Phases
    err = generic.find_feature_phases(dca, simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'),
                                      simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Phases'),
                                      simpl.DataArrayPath('Small IN100', 'Grain Data', 'Phases'))
    assert err == 0, f'FindFeaturePhases ErrorCondition {err}'

    # Find Feature Average Orientations
    err = orientation_analysis.find_avg_orientations(dca,
                                                     simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'),
                                                     simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Phases'),
                                                     simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'Quats'),
                                                     simpl.DataArrayPath('Small IN100', 'Phase Data',
                                                                         'CrystalStructures'),
                                                     simpl.DataArrayPath('Small IN100', 'Grain Data', 'AvgQuats'),
                                                     simpl.DataArrayPath('Small IN100', 'Grain Data', 'AvgEuler'))
    assert err == 0, f'FindAvgOrientations ErrorCondition {err}'

    # Find Feature Neighbors #1
    err = statstoolboxpy.find_neighbors(dca, simpl.DataArrayPath('Small IN100', 'Grain Data', ''),
                                    'SharedSurfaceAreaList2', 'NeighborList2',
                                    simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'),
                                    '', 'NumNeighbors2', '', False, False)
    assert err == 0, f'FindNeighbors #1 ErrorCondition {err}'

    # Merge Twins
    err = reconstruction.merge_twins(dca, 'NewGrain Data', 3, 2,
                                     simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'),
                                     simpl.DataArrayPath('Small IN100', 'Grain Data', 'Phases'),
                                     simpl.DataArrayPath('Small IN100', 'Grain Data', 'AvgQuats'),
                                     simpl.DataArrayPath('Small IN100', 'Phase Data', 'CrystalStructures'),
                                     'ParentIds', 'ParentIds', 'Active',
                                     simpl.DataArrayPath('Small IN100', 'Grain Data', 'NeighborList2'),
                                     simpl.DataArrayPath('', '', ''),
                                     False)
    assert err == 0, f'MergeTwins ErrorCondition {err}'

    # Find Feature Sizes
    err = statstoolboxpy.find_sizes(dca, simpl.DataArrayPath('Small IN100', 'Grain Data', ''),
                                simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'),
                                'Volumes', 'EquivalentDiameters', 'NumElements', False)
    assert err == 0, f'FindSizes ErrorCondition {err}'

    # Minimum Size
    err = processing.min_size(dca, 16, False, 0,
                              simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'),
                              simpl.DataArrayPath('Small IN100', 'Grain Data', 'Phases'),
                              simpl.DataArrayPath('Small IN100', 'Grain Data', 'NumElements'))
    assert err == 0, f'MinSize ErrorCondition {err}'

    # Find Feature Neighbors #2
    err = statstoolboxpy.find_neighbors(dca, simpl.DataArrayPath('Small IN100', 'Grain Data', ''),
                                    'SharedSurfaceAreaList', 'NeighborList',
                                    simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'),
                                    '', 'NumNeighbors', '', False, False)
    assert err == 0, f'FindNeighbors #2 ErrorCondition {err}'

    # Minimum Number of Neighbors
    err = processing.min_neighbors(dca, 2, False, 0, simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'),
                                   simpl.DataArrayPath('Small IN100', 'Grain Data', 'Phases'),
                                   simpl.DataArrayPath('Small IN100', 'Grain Data', 'NumNeighbors'))
    assert err == 0, f'MinNeighbors ErrorCondition {err}'

    # Fill Bad Data
    err = processing.fill_bad_data(dca, False, 1000,
                                   simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'),
                                   simpl.DataArrayPath('', '', ''))
    assert err == 0, f'FillBadData ErrorCondition {err}'

    # Erode / Dilate Bad Data #1
    err = processing.erode_dilate_bad_data(dca, sh.BadDataOperation.Erode, 2, True, True, True,
                                           simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'))
    assert err == 0, f'ErodeDilateBadData #1 ErrorCondition {err}'

    # Erode / Dilate Bad Data #2
    err = processing.erode_dilate_bad_data(dca, sh.BadDataOperation.Dilate, 2, True, True, True,
                                           simpl.DataArrayPath('Small IN100', 'EBSD Scan Data', 'FeatureIds'))
    assert err == 0, f'ErodeDilateBadData #1 ErrorCondition {err}'

    # Write to DREAM3D file
    err = sh.WriteDREAM3DFile(sd.GetBuildDirectory() + '/Data/Output/Reconstruction/SmallIN100_Final.dream3d',
                              dca)
    assert err == 0, f'WriteDREAM3DFile ErrorCondition: {err}'

if __name__ == '__main__':
    small_in100_full_reconstruction()
