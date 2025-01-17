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

#include "PrimaryPhaseWidget.h"

#include <iostream>
#include <limits>

#include <QtCore/QDebug>

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>

// Needed for AxisAngle_t and Crystal Symmetry constants
#include "EbsdLib/Core/EbsdLibConstants.h"

#include "SIMPLib/SIMPLib.h"
#include "SIMPLib/Common/Constants.h"
#include "SIMPLib/DataArrays/StatsDataArray.h"
#include "SIMPLib/DataArrays/StringDataArray.h"
#include "SIMPLib/Filtering/AbstractFilter.h"
#include "SIMPLib/Math/SIMPLibMath.h"
#include "SIMPLib/StatsData/PrimaryStatsData.h"
#include "SIMPLib/StatsData/StatsData.h"
#include "SIMPLib/Utilities/ColorUtilities.h"

#include "EbsdLib/Texture/StatsGen.hpp"

#include "SyntheticBuilding/Gui/Widgets/Presets/Dialogs/PrimaryRecrystallizedPresetDialog.h"
#include "SyntheticBuilding/Gui/Widgets/Presets/Dialogs/RolledPresetDialog.h"
#include "SyntheticBuilding/Gui/Widgets/TableModels/SGAbstractTableModel.h"
#include "SyntheticBuilding/Gui/Widgets/TableModels/SGMDFTableModel.h"
#include "SyntheticBuilding/Gui/Widgets/TableModels/SGODFTableModel.h"
#include "SyntheticBuilding/SyntheticBuildingConstants.h"
#include "SyntheticBuilding/SyntheticBuildingFilters/Presets/PrecipitateRolledPreset.h"
#include "SyntheticBuilding/SyntheticBuildingFilters/Presets/PrimaryEquiaxedPreset.h"
#include "SyntheticBuilding/SyntheticBuildingFilters/Presets/PrimaryRecrystallizedPreset.h"
#include "SyntheticBuilding/SyntheticBuildingFilters/Presets/PrimaryRolledPreset.h"

//-- Qwt Includes AFTER SIMPLib Math due to improper defines in qwt_plot_curve.h
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
PrimaryPhaseWidget::PrimaryPhaseWidget(QWidget* parent)
: StatsGenWidget(parent)
{
  setTabTitle("Primary");
  setupUi(this);
  setupGui();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
PrimaryPhaseWidget::~PrimaryPhaseWidget() = default;

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QComboBox* PrimaryPhaseWidget::getMicrostructurePresetCombo()
{
  return microstructurePresetCombo;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::on_microstructurePresetCombo_currentIndexChanged(int index)
{
  Q_UNUSED(index);
  // qDebug() << "on_microstructurePresetCombo_currentIndexChanged" << "\n";
  QString presetName = microstructurePresetCombo->currentText();

  // Factory Method to get an instantiated object of the correct type?
  MicrostructurePresetManager::Pointer manager = MicrostructurePresetManager::instance();
  setMicroPreset(manager->createNewPreset(presetName));

  AbstractMicrostructurePreset::Pointer absPreset = getMicroPreset();

  if(std::dynamic_pointer_cast<PrimaryRolledPreset>(absPreset) || std::dynamic_pointer_cast<PrecipitateRolledPreset>(absPreset))
  {
    RolledPresetDialog d(nullptr);
    bool keepGoing = true;
    while(keepGoing)
    {
      int ret = d.exec();
      if(ret == QDialog::Accepted)
      {
        float a = d.getA();
        float b = d.getB();
        float c = d.getC();
        if(a >= b && b >= c)
        {
          // The user clicked the OK button so transfer the values from the dialog into this class
          QString presetName = absPreset->getName();
          if(presetName.compare("Primary Rolled") == 0)
          {
            PrimaryRolledPreset::Pointer presetPtr = std::dynamic_pointer_cast<PrimaryRolledPreset>(absPreset);
            presetPtr->setAspectRatio1(d.getA() / d.getB());
            presetPtr->setAspectRatio2(d.getA() / d.getC());
            keepGoing = false;
          }
          if(presetName.compare("Precipitate Rolled") == 0)
          {
            PrecipitateRolledPreset::Pointer presetPtr = std::dynamic_pointer_cast<PrecipitateRolledPreset>(absPreset);
            presetPtr->setAspectRatio1(d.getA() / d.getB());
            presetPtr->setAspectRatio2(d.getA() / d.getC());
            keepGoing = false;
          }
        }
        else
        {
          QMessageBox::critical(&d, "Rolled Preset Error", "The ratios have been entered incorrectly. The following MUST be true: A >= B >= C", QMessageBox::Default);
        }
      }
      else
      {
        keepGoing = false; // user clicked the Cancel button
      }
    }
  }
  else if(std::dynamic_pointer_cast<PrimaryRecrystallizedPreset>(absPreset))
  {
    PrimaryRecrystallizedPresetDialog d(nullptr);
    int ret = d.exec();
    if(ret == QDialog::Accepted)
    {
      // The user clicked the OK button so transfer the values from the dialog into this class
      PrimaryRecrystallizedPreset::Pointer presetPtr = std::dynamic_pointer_cast<PrimaryRecrystallizedPreset>(absPreset);
      presetPtr->setPercentRecrystallized(d.getPercentRecrystallized());
    }
    else
    {
      // Perform any cancellation actions if the user canceled the dialog box
    }
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setOmega3PlotWidget(StatsGenPlotWidget* w)
{
  m_Omega3Plot = w;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
StatsGenPlotWidget* PrimaryPhaseWidget::getOmega3PlotWidget()
{
  return m_Omega3Plot;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setBOverAPlotPlotWidget(StatsGenPlotWidget* w)
{
  m_BOverAPlot = w;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
StatsGenPlotWidget* PrimaryPhaseWidget::getBOverAPlotPlotWidget()
{
  return m_BOverAPlot;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setCOverAPlotWidget(StatsGenPlotWidget* w)
{
  m_COverAPlot = w;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
StatsGenPlotWidget* PrimaryPhaseWidget::getCOverAPlotWidget()
{
  return m_COverAPlot;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setODFWidget(StatsGenODFWidget* w)
{
  m_ODFWidget = w;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
StatsGenODFWidget* PrimaryPhaseWidget::getODFWidget()
{
  return m_ODFWidget;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setAxisODFWidget(StatsGenAxisODFWidget* w)
{
  m_AxisODFWidget = w;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
StatsGenAxisODFWidget* PrimaryPhaseWidget::getAxisODFWidget()
{
  return m_AxisODFWidget;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setMDFWidget(StatsGenMDFWidget* w)
{
  m_MDFWidget = w;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
StatsGenMDFWidget* PrimaryPhaseWidget::getMDFWidget()
{
  return m_MDFWidget;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setFeatureSizeWidget(StatsGenFeatureSizeWidget* w)
{
  m_FeatureSizeDistWidget = w;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
StatsGenFeatureSizeWidget* PrimaryPhaseWidget::getFeatureSizeWidget()
{
  return m_FeatureSizeDistWidget;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QTabWidget* PrimaryPhaseWidget::getTabWidget()
{
  return statsGenPhaseTabWidget;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QPushButton* PrimaryPhaseWidget::getGenerateDefaultDataBtn()
{
  return m_GenerateDefaultData;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::removeNeighborsPlotWidget()
{
  m_NeighborPlot->setParent(nullptr);
  delete m_NeighborPlot;
  m_NeighborPlot = nullptr;
  statsGenPhaseTabWidget->setTabEnabled(5, false);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setupGui()
{
  // Turn off all the plot widgets
  setTabsPlotTabsEnabled(false);

  QLocale loc = QLocale::system();

  getMicrostructurePresetCombo()->blockSignals(true);
  // Register all of our Microstructure Preset Factories
  AbstractMicrostructurePresetFactory::Pointer presetFactory = AbstractMicrostructurePresetFactory::NullPointer();

  // Register the Equiaxed Preset
  presetFactory = RegisterPresetFactory<PrimaryEquiaxedPresetFactory>(getMicrostructurePresetCombo());
  QString presetName = (presetFactory->displayName());
  MicrostructurePresetManager::Pointer manager = MicrostructurePresetManager::instance();
  setMicroPreset(manager->createNewPreset(presetName));

  // Register the Rolled Preset
  presetFactory = RegisterPresetFactory<PrimaryRolledPresetFactory>(getMicrostructurePresetCombo());

  // Select the first Preset in the list
  microstructurePresetCombo->setCurrentIndex(0);
  microstructurePresetCombo->blockSignals(false);

  float mu = 1.0f;
  float sigma = 0.1f;
  float minCutOff = 5.0f;
  float maxCutOff = 5.0f;
  float binStepSize = 0.5f;

  QList<StatsGenPlotWidget*> plotWidgets;

  plotWidgets.push_back(m_Omega3Plot);
  StatsGenPlotWidget* w = m_Omega3Plot;
  w->setPlotTitle(QString("Omega 3 Probability Density Functions"));
  w->setXAxisName(QString("Omega 3"));
  w->setYAxisName(QString("Frequency"));
  w->setDataTitle(QString("Edit Omega3 Distribution Values"));
  w->setDistributionType(SIMPL::DistributionType::Beta);
  w->setStatisticsType(SIMPL::StatisticsType::Feature_SizeVOmega3);
  w->blockDistributionTypeChanges(true);
  w->setRowOperationEnabled(false);
  w->setMu(mu);
  w->setSigma(sigma);
  w->setMinCutOff(minCutOff);
  w->setMaxCutOff(maxCutOff);
  w->setBinStep(binStepSize);
  connect(w, SIGNAL(dataChanged()), this, SLOT(dataWasEdited()));
  connect(w, SIGNAL(dataChanged()), m_FeatureSizeDistWidget, SLOT(userEditedPlotData()));
  connect(w, SIGNAL(dataChanged()), this, SIGNAL(dataChanged()));
  connect(m_FeatureSizeDistWidget, SIGNAL(binSelected(int)), w, SLOT(highlightCurve(int)));

  plotWidgets.push_back(m_BOverAPlot);
  w = m_BOverAPlot;
  w->setPlotTitle(QString("B/A Shape Distribution"));
  w->setXAxisName(QString("B/A"));
  w->setYAxisName(QString("Frequency"));
  w->setDataTitle(QString("Edit B/A Distribution Values"));
  w->setDistributionType(SIMPL::DistributionType::Beta);
  w->setStatisticsType(SIMPL::StatisticsType::Feature_SizeVBoverA);
  w->blockDistributionTypeChanges(true);
  w->setRowOperationEnabled(false);
  w->setMu(mu);
  w->setSigma(sigma);
  w->setMinCutOff(minCutOff);
  w->setMaxCutOff(maxCutOff);
  w->setBinStep(binStepSize);
  connect(w, SIGNAL(dataChanged()), this, SLOT(dataWasEdited()));
  connect(w, SIGNAL(dataChanged()), m_FeatureSizeDistWidget, SLOT(userEditedPlotData()));
  connect(w, SIGNAL(dataChanged()), this, SIGNAL(dataChanged()));
  connect(m_FeatureSizeDistWidget, SIGNAL(binSelected(int)), w, SLOT(highlightCurve(int)));

  plotWidgets.push_back(m_COverAPlot);
  w = m_COverAPlot;
  w->setPlotTitle(QString("C/A Shape Distribution"));
  w->setXAxisName(QString("C/A"));
  w->setYAxisName(QString("Frequency"));
  w->setDataTitle(QString("Edit C/A Distribution Values"));
  w->setDistributionType(SIMPL::DistributionType::Beta);
  w->setStatisticsType(SIMPL::StatisticsType::Feature_SizeVCoverA);
  w->blockDistributionTypeChanges(true);
  w->setRowOperationEnabled(false);
  w->setMu(mu);
  w->setSigma(sigma);
  w->setMinCutOff(minCutOff);
  w->setMaxCutOff(maxCutOff);
  w->setBinStep(binStepSize);
  connect(w, SIGNAL(dataChanged()), this, SLOT(dataWasEdited()));
  connect(w, SIGNAL(dataChanged()), m_FeatureSizeDistWidget, SLOT(userEditedPlotData()));
  connect(w, SIGNAL(dataChanged()), this, SIGNAL(dataChanged()));
  connect(m_FeatureSizeDistWidget, SIGNAL(binSelected(int)), w, SLOT(highlightCurve(int)));

  plotWidgets.push_back(m_NeighborPlot);
  w = m_NeighborPlot;
  w->setPlotTitle(QString("Neighbors Distributions"));
  w->setXAxisName(QString("Number of Features (within 1 diameter)"));
  w->setYAxisName(QString("Frequency"));
  w->setDataTitle(QString("Edit Neighbor Distribution Values"));
  w->setDistributionType(SIMPL::DistributionType::LogNormal);
  w->setStatisticsType(SIMPL::StatisticsType::Feature_SizeVNeighbors);
  w->blockDistributionTypeChanges(true);
  w->setRowOperationEnabled(false);
  w->setMu(mu);
  w->setSigma(sigma);
  w->setMinCutOff(minCutOff);
  w->setMaxCutOff(maxCutOff);
  w->setBinStep(binStepSize);
  connect(w, SIGNAL(dataChanged()), this, SLOT(dataWasEdited()));
  connect(w, SIGNAL(dataChanged()), m_FeatureSizeDistWidget, SLOT(userEditedPlotData()));
  connect(w, SIGNAL(dataChanged()), this, SIGNAL(dataChanged()));
  connect(m_FeatureSizeDistWidget, SIGNAL(binSelected(int)), w, SLOT(highlightCurve(int)));

  setSGPlotWidgets(plotWidgets);

  SGODFTableModel* odfTableModel = m_ODFWidget->tableModel();
  m_MDFWidget->setODFTableModel(odfTableModel);

  // Remove any Axis Decorations. The plots are explicitly know to have a -1 to 1 axis min/max
  m_ODFWidget->setEnableAxisDecorations(false);

  // Remove any Axis Decorations. The plots are explicitly know to have a -1 to 1 axis min/max
  m_AxisODFWidget->setEnableAxisDecorations(false);

  connect(m_ODFWidget, SIGNAL(dataChanged()), this, SIGNAL(dataChanged()));
  connect(m_ODFWidget, SIGNAL(bulkLoadEvent(bool)), this, SLOT(bulkLoadEvent(bool)));
  connect(m_ODFWidget, &StatsGenODFWidget::odfDataChanged, this, &PrimaryPhaseWidget::dataChanged);
  connect(m_ODFWidget, SIGNAL(odfDataChanged()), m_MDFWidget, SLOT(updatePlots()));

  connect(m_MDFWidget, SIGNAL(dataChanged()), this, SIGNAL(dataChanged()));

  connect(m_AxisODFWidget, SIGNAL(dataChanged()), this, SIGNAL(dataChanged()));

  connect(m_FeatureSizeDistWidget, SIGNAL(dataChanged()), this, SIGNAL(dataChanged()));
  connect(m_FeatureSizeDistWidget, SIGNAL(userEnteredValidData(bool)), m_GenerateDefaultData, SLOT(setEnabled(bool)));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setPhaseIndex(int index)
{
  StatsGenWidget::setPhaseIndex(index);
  m_Omega3Plot->setPhaseIndex(index);
  m_BOverAPlot->setPhaseIndex(index);
  m_COverAPlot->setPhaseIndex(index);
  if(m_NeighborPlot != nullptr)
  {
    m_NeighborPlot->setPhaseIndex(index);
  }
  m_ODFWidget->setPhaseIndex(index);
  m_MDFWidget->setPhaseIndex(index);
  m_AxisODFWidget->setPhaseIndex(index);
  m_FeatureSizeDistWidget->setPhaseIndex(index);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setCrystalStructure(unsigned int xtal)
{
  StatsGenWidget::setCrystalStructure(xtal);
  m_Omega3Plot->setCrystalStructure(xtal);
  m_BOverAPlot->setCrystalStructure(xtal);
  m_COverAPlot->setCrystalStructure(xtal);
  if(m_NeighborPlot != nullptr)
  {
    m_NeighborPlot->setCrystalStructure(xtal);
  }
  m_ODFWidget->setCrystalStructure(xtal);
  m_FeatureSizeDistWidget->setCrystalStructure(xtal);
  m_MDFWidget->setCrystalStructure(xtal);
  /* Note that we do NOT want to set the crystal structure for the AxisODF widget
   * because we need that crystal structure to be OrthoRhombic in order for those
   * calculations to be performed correctly */
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QString PrimaryPhaseWidget::getComboString()
{
  QString s = QString::number(getPhaseIndex());
  s.append(" - ");
  if(EbsdLib::CrystalStructure::Cubic_High == getCrystalStructure())
  {
    s.append("Cubic");
  }
  else if(EbsdLib::CrystalStructure::Hexagonal_High == getCrystalStructure())
  {
    s.append("Hexagonal");
  }
  return s;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setTabsPlotTabsEnabled(bool b)
{
  qint32 count = this->statsGenPhaseTabWidget->count();
  for(qint32 i = 1; i < count; ++i)
  {
    this->statsGenPhaseTabWidget->setTabEnabled(i, b);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::dataWasEdited()
{
  setTabsPlotTabsEnabled(true);
  m_GenerateDefaultData->setEnabled(false);
  // this->tabWidget->setTabEnabled(0, false);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setWidgetListEnabled(bool b)
{
  for(QWidget* w : m_WidgetList)
  {
    w->setEnabled(b);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::updatePlots()
{
  if(getDataHasBeenGenerated())
  {
    QProgressDialog progress("Generating Data ....", "Cancel", 0, 4, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    progress.setValue(1);
    progress.setLabelText("[1/3] Calculating Size Distributions ...");
    // Force Emit the signal at the controller to get the plots to update...
    m_FeatureSizeDistWidget->plotSizeDistribution();

    // Now that we have bins and feature sizes, push those to the other plot widgets
    // Setup Each Plot Widget
    // The MicroPreset class will set the distribution for each of the plots
    QwtArray<float> binSizes = m_FeatureSizeDistWidget->getBinSizes();
    QMap<QString, QVector<float>> data;
    data[AbstractMicrostructurePreset::kBinNumbers] = binSizes;
    QVector<SIMPL::Rgb> colors = ColorUtilities::GenerateColors(binSizes.size(), SyntheticBuildingConstants::k_HSV_Saturation, SyntheticBuildingConstants::k_HSV_Value);

    getMicroPreset()->initializeOmega3TableModel(data);
    m_Omega3Plot->setDistributionType(getMicroPreset()->getDistributionType(AbstractMicrostructurePreset::kOmega3Distribution), false);

    SGAbstractTableModel* tmodel = m_Omega3Plot->tableModel();
    if(tmodel != nullptr)
    {
      QVector<QVector<float>> colData;
      colData.push_back(data[AbstractMicrostructurePreset::kAlpha]);
      colData.push_back(data[AbstractMicrostructurePreset::kBeta]);
      tmodel->setTableData(binSizes, colData, colors);
    }

    getMicroPreset()->initializeBOverATableModel(data);
    m_BOverAPlot->setDistributionType(getMicroPreset()->getDistributionType(AbstractMicrostructurePreset::kBOverADistribution), false);
    tmodel = m_BOverAPlot->tableModel();
    if(tmodel != nullptr)
    {
      QVector<QVector<float>> colData;
      colData.push_back(data[AbstractMicrostructurePreset::kAlpha]);
      colData.push_back(data[AbstractMicrostructurePreset::kBeta]);
      tmodel->setTableData(binSizes, colData, colors);
    }

    getMicroPreset()->initializeCOverATableModel(data);
    m_COverAPlot->setDistributionType(getMicroPreset()->getDistributionType(AbstractMicrostructurePreset::kCOverADistribution), false);
    tmodel = m_COverAPlot->tableModel();
    if(tmodel != nullptr)
    {
      QVector<QVector<float>> colData;
      colData.push_back(data[AbstractMicrostructurePreset::kAlpha]);
      colData.push_back(data[AbstractMicrostructurePreset::kBeta]);
      tmodel->setTableData(binSizes, colData, colors);
    }

    if(m_NeighborPlot != nullptr)
    {
      getMicroPreset()->initializeNeighborTableModel(data);
      m_NeighborPlot->setDistributionType(getMicroPreset()->getDistributionType(AbstractMicrostructurePreset::kNeighborDistribution), false);
      tmodel = m_NeighborPlot->tableModel();
      if(tmodel != nullptr)
      {
        QVector<QVector<float>> colData;
        colData.push_back(data[AbstractMicrostructurePreset::kMu]);
        colData.push_back(data[AbstractMicrostructurePreset::kSigma]);
        tmodel->setTableData(binSizes, colData, colors);
      }
    }

    progress.setValue(2);
    progress.setLabelText("[2/3] Calculating ODF Data ...");

    if(m_ResetData)
    {
      getMicroPreset()->initializeODFTableModel(data);
      SGODFTableModel* model = m_ODFWidget->tableModel();
      if(model != nullptr)
      {
        model->setTableData(data[AbstractMicrostructurePreset::kEuler1], data[AbstractMicrostructurePreset::kEuler2], data[AbstractMicrostructurePreset::kEuler3],
                            data[AbstractMicrostructurePreset::kWeight], data[AbstractMicrostructurePreset::kSigma]);
      }

      // m_MicroPreset->initializeMDFTableModel(m_ODFWidget->getMDFWidget());
      getMicroPreset()->initializeMDFTableModel(data);
      SGMDFTableModel* mdfModel = (m_MDFWidget->tableModel());
      if(mdfModel != nullptr)
      {
        mdfModel->setTableData(data[AbstractMicrostructurePreset::kAngles], data[AbstractMicrostructurePreset::kAxis], data[AbstractMicrostructurePreset::kWeight]);
      }

      progress.setValue(3);
      progress.setLabelText("[3/3] Calculating Axis ODF Data ...");
      // m_AxisODFWidget->updatePlots();
      getMicroPreset()->initializeAxisODFTableModel(data);
      model = m_AxisODFWidget->tableModel();
      if(model != nullptr)
      {
        model->setTableData(data[AbstractMicrostructurePreset::kEuler1], data[AbstractMicrostructurePreset::kEuler2], data[AbstractMicrostructurePreset::kEuler3],
                            data[AbstractMicrostructurePreset::kWeight], data[AbstractMicrostructurePreset::kSigma]);
      }
    }
    else
    {
      m_ODFWidget->updatePlots();
      m_AxisODFWidget->updatePlots();
      m_MDFWidget->updatePlots();
      // Get any presets for the ODF/AxisODF/MDF also
    }
    progress.setValue(4);

    setTabsPlotTabsEnabled(true);
  }
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::generateDefaultData()
{
  on_m_GenerateDefaultData_clicked();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::on_m_GenerateDefaultData_clicked()
{
  setDataHasBeenGenerated(true);
  updatePlots();
  Q_EMIT dataChanged();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::on_m_ResetDataBtn_clicked()
{

  m_FeatureSizeDistWidget->resetUI();
  setupGui();

  setDataHasBeenGenerated(true); // Set this boolean to true so that data generation is triggered
  m_ResetData = true;
  updatePlots(); // Regenerate all the default data
  Q_EMIT dataChanged();
  m_ResetData = false;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::bulkLoadEvent(bool fail)
{
  setBulkLoadFailure(fail);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int PrimaryPhaseWidget::gatherStatsData(AttributeMatrix::Pointer attrMat, bool preflight)
{
  if(getPhaseIndex() < 1)
  {
    QMessageBox::critical(this, tr("StatsGenerator"), tr("The Phase Index is Less than 1. This is not allowed."), QMessageBox::Default);
    return -1;
  }
  int retErr = 0;
  int err = 0;

  // Get pointers
  IDataArray::Pointer iDataArray = attrMat->getAttributeArray(SIMPL::EnsembleData::CrystalStructures);
  unsigned int* crystalStructures = std::dynamic_pointer_cast<UInt32ArrayType>(iDataArray)->getPointer(0);
  iDataArray = attrMat->getAttributeArray(SIMPL::EnsembleData::PhaseTypes);
  PhaseType::EnumType* phaseTypes = std::dynamic_pointer_cast<UInt32ArrayType>(iDataArray)->getPointer(0);

  crystalStructures[getPhaseIndex()] = getCrystalStructure();
  phaseTypes[getPhaseIndex()] = static_cast<PhaseType::EnumType>(getPhaseType());

  iDataArray = attrMat->getAttributeArray(SIMPL::EnsembleData::PhaseName);
  StringDataArray::Pointer phaseNameArray = std::dynamic_pointer_cast<StringDataArray>(iDataArray);
  phaseNameArray->setValue(getPhaseIndex(), getPhaseName());

  StatsDataArray::Pointer statsDataArray = std::dynamic_pointer_cast<StatsDataArray>(attrMat->getAttributeArray(SIMPL::EnsembleData::Statistics));
  if(nullptr != statsDataArray)
  {
    StatsData::Pointer statsData = statsDataArray->getStatsData(getPhaseIndex());
    PrimaryStatsData::Pointer primaryStatsData = std::dynamic_pointer_cast<PrimaryStatsData>(statsData);

    float calcPhaseFraction = getPhaseFraction() / getTotalPhaseFraction();

    primaryStatsData->setPhaseFraction(calcPhaseFraction);
    statsData->setName(getPhaseName());

    err = m_FeatureSizeDistWidget->getStatisticsData(primaryStatsData.get());

    // Now that we have bins and feature sizes, push those to the other plot widgets
    {
      VectorOfFloatArray data = m_Omega3Plot->getStatisticsData();
      primaryStatsData->setFeatureSize_Omegas(data);
      primaryStatsData->setOmegas_DistType(m_Omega3Plot->getDistributionType());
    }
    {
      VectorOfFloatArray data = m_BOverAPlot->getStatisticsData();
      primaryStatsData->setFeatureSize_BOverA(data);
      primaryStatsData->setBOverA_DistType(m_BOverAPlot->getDistributionType());
    }
    {
      VectorOfFloatArray data = m_COverAPlot->getStatisticsData();
      primaryStatsData->setFeatureSize_COverA(data);
      primaryStatsData->setCOverA_DistType(m_COverAPlot->getDistributionType());
    }
    if(m_NeighborPlot != nullptr)
    {
      VectorOfFloatArray data = m_NeighborPlot->getStatisticsData();
      primaryStatsData->setFeatureSize_Neighbors(data);
      primaryStatsData->setNeighbors_DistType(m_NeighborPlot->getDistributionType());
    }

    m_ODFWidget->getOrientationData(primaryStatsData.get(), PhaseType::Type::Primary, preflight);
    m_MDFWidget->getMisorientationData(primaryStatsData.get(), PhaseType::Type::Primary, preflight);

    err = m_AxisODFWidget->getOrientationData(primaryStatsData.get(), PhaseType::Type::Primary, preflight);
  }
  return retErr;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::extractStatsData(AttributeMatrix::Pointer attrMat, int index)
{

  Q_EMIT progressText(QString("Primary Phase extracting statistics..."));
  setWidgetListEnabled(true);
  setPhaseIndex(index);
  qApp->processEvents();

  IDataArray::Pointer iDataArray = attrMat->getAttributeArray(SIMPL::EnsembleData::CrystalStructures);
  PhaseType::EnumType* attributeArray = std::dynamic_pointer_cast<UInt32ArrayType>(iDataArray)->getPointer(0);
  setCrystalStructure(attributeArray[index]);

  iDataArray = attrMat->getAttributeArray(SIMPL::EnsembleData::PhaseTypes);
  attributeArray = std::dynamic_pointer_cast<UInt32ArrayType>(iDataArray)->getPointer(0);
  setPhaseType(static_cast<PhaseType::Type>(attributeArray[index]));

  iDataArray = attrMat->getAttributeArray(SIMPL::EnsembleData::Statistics);
  StatsDataArray::Pointer statsDataArray = std::dynamic_pointer_cast<StatsDataArray>(iDataArray);
  if(statsDataArray == nullptr)
  {
    return;
  }
  StatsData::Pointer statsData = statsDataArray->getStatsData(index);
  PrimaryStatsData::Pointer primaryStatsData = std::dynamic_pointer_cast<PrimaryStatsData>(statsData);

  setPhaseFraction(primaryStatsData->getPhaseFraction());

  QString phaseName = statsData->getName();
  if(phaseName.isEmpty())
  {
    phaseName = QString("Primary Phase (%1)").arg(index);
  }
  setPhaseName(phaseName);
  setTabTitle(phaseName);
  m_FeatureSizeDistWidget->setCrystalStructure(getCrystalStructure());
  for(StatsGenPlotWidget* w : m_SGPlotWidgets)
  {
    w->setCrystalStructure(getCrystalStructure());
  }
  m_ODFWidget->setCrystalStructure(getCrystalStructure());

  /* Set the BinNumbers data set */
  FloatArrayType::Pointer bins = primaryStatsData->getBinNumbers();
  if(bins == nullptr)
  {
    return;
  }

  // Now have each of the plots set it's own data
  QVector<float> qbins(bins->getNumberOfTuples());
  for(int i = 0; i < qbins.size(); ++i)
  {
    qbins[i] = bins->getValue(i);
  }

  m_FeatureSizeDistWidget->extractStatsData(primaryStatsData.get(), index);
  Q_EMIT progressText(QString("Extracting Size Distribution Values"));
  qApp->processEvents();

  float mu = m_FeatureSizeDistWidget->getMu();
  float sigma = m_FeatureSizeDistWidget->getSigma();
  float minCutOff = m_FeatureSizeDistWidget->getMinCutOff();
  float maxCutOff = m_FeatureSizeDistWidget->getMaxCutOff();
  float binStepSize = m_FeatureSizeDistWidget->getBinStep();

  Q_EMIT progressText(QString("Extracting Omega 3 Distribution Values"));
  qApp->processEvents();
  m_Omega3Plot->setDistributionType(primaryStatsData->getOmegas_DistType(), false);
  m_Omega3Plot->extractStatsData(index, qbins, primaryStatsData->getFeatureSize_Omegas());
  m_Omega3Plot->setSizeDistributionValues(mu, sigma, minCutOff, maxCutOff, binStepSize);

  Q_EMIT progressText(QString("Extracting B Over a Distribution Values"));
  qApp->processEvents();
  m_BOverAPlot->setDistributionType(primaryStatsData->getBOverA_DistType(), false);
  m_BOverAPlot->extractStatsData(index, qbins, primaryStatsData->getFeatureSize_BOverA());
  m_BOverAPlot->setSizeDistributionValues(mu, sigma, minCutOff, maxCutOff, binStepSize);

  Q_EMIT progressText(QString("Extracting C Over A Distribution Values"));
  qApp->processEvents();
  m_COverAPlot->setDistributionType(primaryStatsData->getCOverA_DistType(), false);
  m_COverAPlot->extractStatsData(index, qbins, primaryStatsData->getFeatureSize_COverA());
  m_COverAPlot->setSizeDistributionValues(mu, sigma, minCutOff, maxCutOff, binStepSize);

  if(m_NeighborPlot != nullptr)
  {
    Q_EMIT progressText(QString("Extracting Neighbor Distribution Values"));
    qApp->processEvents();
    m_NeighborPlot->setDistributionType(primaryStatsData->getNeighbors_DistType(), false);
    m_NeighborPlot->extractStatsData(index, qbins, primaryStatsData->getFeatureSize_Neighbors());
    m_NeighborPlot->setSizeDistributionValues(mu, sigma, minCutOff, maxCutOff, binStepSize);
  }

  Q_EMIT progressText(QString("Extracting ODF Distribution Values"));
  qApp->processEvents();
  // Set the ODF Data
  m_ODFWidget->extractStatsData(index, primaryStatsData.get(), PhaseType::Type::Primary);

  Q_EMIT progressText(QString("Extracting MDF Distribution Values"));
  qApp->processEvents();
  // Set the ODF Data
  m_MDFWidget->extractStatsData(index, primaryStatsData.get(), PhaseType::Type::Primary);

  Q_EMIT progressText(QString("Extracting Axis ODF Distribution Values"));
  qApp->processEvents();
  // Set the Axis ODF Data
  m_AxisODFWidget->extractStatsData(index, primaryStatsData.get(), PhaseType::Type::Primary);

  // Enable all the tabs
  setTabsPlotTabsEnabled(true);
  setDataHasBeenGenerated(true);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
QIcon PrimaryPhaseWidget::getPhaseIcon()
{
  QIcon icon;
  icon.addFile(QStringLiteral(":/StatsGenerator/icons/Primary.png"), QSize(), QIcon::Normal, QIcon::Off);
  icon.addFile(QStringLiteral(":/StatsGenerator/icons/Primary_Selected.png"), QSize(), QIcon::Normal, QIcon::On);
  return icon;
}

// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setPhaseType(PhaseType::Type value)
{
  m_PhaseType = value;
}

// -----------------------------------------------------------------------------
PhaseType::Type PrimaryPhaseWidget::getPhaseType() const
{
  return m_PhaseType;
}

// -----------------------------------------------------------------------------
void PrimaryPhaseWidget::setSGPlotWidgets(const QList<StatsGenPlotWidget*>& value)
{
  m_SGPlotWidgets = value;
}

// -----------------------------------------------------------------------------
QList<StatsGenPlotWidget*> PrimaryPhaseWidget::getSGPlotWidgets() const
{
  return m_SGPlotWidgets;
}
