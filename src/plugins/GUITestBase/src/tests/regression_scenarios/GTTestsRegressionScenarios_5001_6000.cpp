/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2016 UniPro <ugene@unipro.ru>
 * http://ugene.unipro.ru
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <QApplication>
#include <QFile>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTableView>
#include <QWebElement>

#include <U2Core/BaseDocumentFormats.h>
#include <U2Core/DocumentModel.h>

#include <U2Gui/ToolsMenu.h>

#include <U2View/ADVConstants.h>
#include <U2View/ADVSequenceObjectContext.h>
#include <U2View/DetView.h>
#include <U2View/MSAEditorTreeViewer.h>
#include <U2View/MSAGraphOverview.h>

#include <base_dialogs/DefaultDialogFiller.h>
#include <base_dialogs/GTFileDialog.h>
#include <base_dialogs/MessageBoxFiller.h>
#include <drivers/GTKeyboardDriver.h>
#include <drivers/GTMouseDriver.h>
#include <primitives/GTAction.h>
#include <primitives/GTCheckBox.h>
#include <primitives/GTComboBox.h>
#include <primitives/GTLineEdit.h>
#include <primitives/GTListWidget.h>
#include <primitives/GTMenu.h>
#include <primitives/GTRadioButton.h>
#include <primitives/GTSlider.h>
#include <primitives/GTSpinBox.h>
#include <primitives/GTTabWidget.h>
#include <primitives/GTTableView.h>
#include <primitives/GTTextEdit.h>
#include <primitives/GTToolbar.h>
#include <primitives/GTTreeWidget.h>
#include <primitives/GTWidget.h>
#include <primitives/PopupChooser.h>
#include <runnables/ugene/corelibs/U2View/ov_msa/DistanceMatrixDialogFiller.h>
#include <runnables/ugene/corelibs/U2View/ov_msa/GenerateAlignmentProfileDialogFiller.h>
#include <runnables/ugene/ugeneui/SequenceReadingModeSelectorDialogFiller.h>
#include <runnables/ugene/plugins/dna_export/ExportSequencesDialogFiller.h>
#include <runnables/ugene/plugins_3rdparty/umuscle/MuscleDialogFiller.h>
#include <system/GTClipboard.h>
#include <system/GTFile.h>
#include <utils/GTThread.h>
#include <utils/GTUtilsDialog.h>

#include "GTTestsRegressionScenarios_5001_6000.h"
#include "GTUtilsAnnotationsTreeView.h"
#include "GTUtilsAssemblyBrowser.h"
#include "GTUtilsCircularView.h"
#include "GTUtilsDashboard.h"
#include "GTUtilsDocument.h"
#include "GTUtilsExternalTools.h"
#include "GTUtilsLog.h"
#include "GTUtilsMdi.h"
#include "GTUtilsMsaEditor.h"
#include "GTUtilsMsaEditorSequenceArea.h"
#include "GTUtilsNotifications.h"
#include "GTUtilsOptionPanelMSA.h"
#include "GTUtilsOptionPanelSequenceView.h"
#include "GTUtilsOptionsPanel.h"
#include "GTUtilsPcr.h"
#include "GTUtilsPhyTree.h"
#include "GTUtilsPrimerLibrary.h"
#include "GTUtilsProject.h"
#include "GTUtilsProjectTreeView.h"
#include "GTUtilsSequenceView.h"
#include "GTUtilsSharedDatabaseDocument.h"
#include "GTUtilsStartPage.h"
#include "GTUtilsTask.h"
#include "GTUtilsTaskTreeView.h"
#include "GTUtilsWizard.h"
#include "GTUtilsWorkflowDesigner.h"


namespace U2 {

namespace GUITest_regression_scenarios {
using namespace HI;

GUI_TEST_CLASS_DEFINITION(test_5012) {
    GTUtilsWorkflowDesigner::openWorkflowDesigner(os);

    GTUtilsWorkflowDesigner::addSample(os, "Call variants with SAMtools");
    GTUtilsWorkflowDesigner::click(os, "Read Assembly (BAM/SAM)");

    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/bam/", "scerevisiae.bam1.sam");

    GTUtilsWorkflowDesigner::createDataset(os);
    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/bam/", "scerevisiae.bam2.sam");

    GTUtilsWorkflowDesigner::createDataset(os);
    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/bam/", "scerevisiae.bam3.sam");

    GTUtilsWorkflowDesigner::click(os, "Read Sequence");

    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/genbank/", "pBR322.gb");

    GTUtilsWorkflowDesigner::createDataset(os);
    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/genbank/", "JQ040024.1.gb");

    GTUtilsWorkflowDesigner::createDataset(os);
    GTUtilsWorkflowDesigner::setDatasetInputFile(os, dataDir + "samples/Assembly/", "chrM.fa");

    GTUtilsWorkflowDesigner::click(os, "Call Variants");
    GTUtilsWorkflowDesigner::setParameter(os, "Output variants file", QDir(sandBoxDir).absoluteFilePath("test_5012.vcf"), GTUtilsWorkflowDesigner::textValue);

    GTUtilsWorkflowDesigner::runWorkflow(os);
}

GUI_TEST_CLASS_DEFINITION(test_5012_1) {
    GTLogTracer l;
    GTUtilsWorkflowDesigner::openWorkflowDesigner(os);

    GTUtilsWorkflowDesigner::addSample(os, "Call variants with SAMtools");
    GTUtilsWorkflowDesigner::click(os, "Read Assembly (BAM/SAM)");

    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/bam/", "scerevisiae.bam1.sam");

    GTUtilsWorkflowDesigner::click(os, "Read Sequence");

    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/genbank/", "pBR322.gb");

    GTUtilsWorkflowDesigner::createDataset(os);
    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/genbank/", "JQ040024.1.gb");

    GTUtilsWorkflowDesigner::createDataset(os);
    GTUtilsWorkflowDesigner::setDatasetInputFile(os, dataDir + "samples/Assembly/", "chrM.fa");

    GTUtilsWorkflowDesigner::click(os, "Call Variants");
    GTUtilsWorkflowDesigner::setParameter(os, "Output variants file", QDir(sandBoxDir).absoluteFilePath("test_5012_1.vcf"), GTUtilsWorkflowDesigner::textValue);

    GTUtilsWorkflowDesigner::runWorkflow(os);
    GTUtilsTaskTreeView::waitTaskFinished(os);
    CHECK_SET_ERR(l.hasError(), "There is no error in the log");
}

GUI_TEST_CLASS_DEFINITION(test_5012_2) {
    GTLogTracer l;
    GTUtilsWorkflowDesigner::openWorkflowDesigner(os);

    GTUtilsWorkflowDesigner::addSample(os, "Call variants with SAMtools");
    GTUtilsWorkflowDesigner::click(os, "Read Assembly (BAM/SAM)");

    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/bam/", "scerevisiae.bam1.sam");

    GTUtilsWorkflowDesigner::createDataset(os);
    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/bam/", "scerevisiae.bam2.sam");

    GTUtilsWorkflowDesigner::click(os, "Read Sequence");

    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir +"_common_data/genbank/", "pBR322.gb");

    GTUtilsWorkflowDesigner::click(os, "Call Variants");
    GTUtilsWorkflowDesigner::setParameter(os, "Output variants file", QDir(sandBoxDir).absoluteFilePath("test_5012_2.vcf"), GTUtilsWorkflowDesigner::textValue);

    GTUtilsWorkflowDesigner::runWorkflow(os);
    GTUtilsTaskTreeView::waitTaskFinished(os);
    CHECK_SET_ERR(l.hasError(), "There is no error in the log");
}

GUI_TEST_CLASS_DEFINITION(test_5018) {
#ifdef Q_OS_WIN
    const QString homePlaceholder = "%UserProfile%";
#else
    const QString homePlaceholder = "~";
#endif

//    1. Ensure that there is no "test_5018.fa" file in the home dir.
    const QString homePath = QDir::homePath();
    if (GTFile::check(os, homePath + "/test_5018.fa")) {
        QFile(homePath + "/test_5018.fa").remove();
        CHECK_SET_ERR(!GTFile::check(os, homePath + "/test_5018.fa"), "File can't be removed");
    }

//    2. Open "data/samples/FASTA/human_T1.fa".
    GTFileDialog::openFile(os, dataDir + "samples/FASTA/human_T1.fa");
    GTUtilsTaskTreeView::waitTaskFinished(os);

//    3. Call context menu on the sequence object in the Project View, select {Export/Import -> Export sequences...} item.
//    4. Set output path to "~/test_5018.fa" for *nix and "%HOME_DIR%\test_5018.fa" for Windows. Accept the dialog.
    GTUtilsDialog::waitForDialog(os, new PopupChooserByText(os, QStringList() << "Export/Import" << "Export sequences..."));
    GTUtilsDialog::waitForDialog(os, new ExportSelectedRegionFiller(os, homePlaceholder + "/test_5018.fa"));
    GTUtilsProjectTreeView::click(os, "human_T1 (UCSC April 2002 chr7:115977709-117855134)", Qt::RightButton);

    GTUtilsTaskTreeView::waitTaskFinished(os);

//    Expected state: "test_5018.fa" appears in the home dir.
    CHECK_SET_ERR(GTFile::check(os, homePath + "/test_5018.fa"), "File was not created");
    GTUtilsDialog::waitForDialog(os, new MessageBoxNoToAllOrNo(os));
    QFile(homePath + "/test_5018.fa").remove();
    GTGlobals::sleep(5000);
}

GUI_TEST_CLASS_DEFINITION(test_5027_1) {
    //1. Open preferences and set memory limit per task 500000MB
    //2. Open WD and compose next scheme "File list" -> "SnpEff annotation and filtration"
    //3. Run schema.
    //Expected state : there is problem on dashboard "A problem occurred during allocating memory for running SnpEff."
    class MemorySetter : public CustomScenario {
    public:
        MemorySetter(int memValue)
            : memValue(memValue) {}
        void run(HI::GUITestOpStatus &os) {
            QWidget *dialog = QApplication::activeModalWidget();
            CHECK_SET_ERR(dialog, "activeModalWidget is NULL");

            AppSettingsDialogFiller::openTab(os, AppSettingsDialogFiller::Resourses);

            QSpinBox* memSpinBox = qobject_cast<QSpinBox*>(GTWidget::findWidget(os, "memorySpinBox"));
            CHECK_SET_ERR(memSpinBox != NULL, "No memorySpinBox");
            GTSpinBox::setValue(os, memSpinBox, memValue, GTGlobals::UseKeyBoard);

            GTUtilsDialog::clickButtonBox(os, dialog, QDialogButtonBox::Ok);
        }
    private:
        int memValue;
    };

    GTUtilsDialog::waitForDialog(os, new AppSettingsDialogFiller(os, new MemorySetter(500000)));
    GTMenu::clickMainMenuItem(os, QStringList() << "Settings" << "Preferences...");
    GTGlobals::sleep();

    GTUtilsWorkflowDesigner::openWorkflowDesigner(os);
    GTUtilsWorkflowDesigner::addSample(os, "SnpEff");
    GTThread::waitForMainThread();
    GTUtilsWorkflowDesigner::click(os, "Input Variations File");
    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir + "_common_data/vcf", "valid.vcf");
    GTUtilsWorkflowDesigner::runWorkflow(os);
    GTUtilsTaskTreeView::waitTaskFinished(os);

    GTWebView::findElement(os, GTUtilsDashboard::getDashboard(os), "A problem occurred during allocating memory for running SnpEff.");
}

GUI_TEST_CLASS_DEFINITION(test_5027_2) {
    //1. Open preferences and set memory limit per task 512MB
    //2. Open WD and compose next scheme "File list" -> "SnpEff annotation and filtration"
    //3. Run schema.
    //Expected state : there is problem on dashboard "There is not enough memory to complete the SnpEff execution."
    class MemorySetter : public CustomScenario {
    public:
        MemorySetter(int memValue)
            : memValue(memValue) {}
        void run(HI::GUITestOpStatus &os) {
            QWidget *dialog = QApplication::activeModalWidget();
            CHECK_SET_ERR(dialog, "activeModalWidget is NULL");

            AppSettingsDialogFiller::openTab(os, AppSettingsDialogFiller::Resourses);

            QSpinBox* memSpinBox = qobject_cast<QSpinBox*>(GTWidget::findWidget(os, "memorySpinBox"));
            CHECK_SET_ERR(memSpinBox != NULL, "No memorySpinBox");
            GTSpinBox::setValue(os, memSpinBox, memValue, GTGlobals::UseKeyBoard);

            GTUtilsDialog::clickButtonBox(os, dialog, QDialogButtonBox::Ok);
        }
    private:
        int memValue;
    };

    GTUtilsDialog::waitForDialog(os, new AppSettingsDialogFiller(os, new MemorySetter(256)));
    GTMenu::clickMainMenuItem(os, QStringList() << "Settings" << "Preferences...");
    GTGlobals::sleep();

    GTUtilsWorkflowDesigner::openWorkflowDesigner(os);
    GTUtilsWorkflowDesigner::addSample(os, "SnpEff");
    GTThread::waitForMainThread();
    GTUtilsWorkflowDesigner::click(os, "Input Variations File");
    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir + "_common_data/vcf", "valid.vcf");
    GTUtilsWorkflowDesigner::runWorkflow(os);
    GTUtilsTaskTreeView::waitTaskFinished(os);

    GTWebView::findElement(os, GTUtilsDashboard::getDashboard(os), "There is not enough memory to complete the SnpEff execution.");
}

GUI_TEST_CLASS_DEFINITION(test_5029) {
    GTMenu::clickMainMenuItem(os, QStringList() << "Settings" << "Plugins...");
    GTGlobals::sleep();
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(GTWidget::findWidget(os,"treeWidget"));
    int numPlugins = tree->topLevelItemCount();
    CHECK_SET_ERR( numPlugins > 10, QString("Not all plugins were loaded. Loaded %1 plugins").arg(numPlugins));
}

GUI_TEST_CLASS_DEFINITION(test_5052) {
    //1. Open "samples/Genbank/murine.gb".
    GTFileDialog::openFile(os, dataDir + "samples/Genbank/murine.gb");
    //2. Close the opened sequence view.
    GTGlobals::FindOptions findOptions;
    findOptions.matchPolicy = Qt::MatchContains;
    GTUtilsMdi::closeWindow(os, "NC_", findOptions);
    //3. Click "murine.gb" on Start Page.
    GTUtilsStartPage::clickResentDocument(os, "murine.gb");
    //Expected: The file is loaded, the view is opened.
    GTUtilsTaskTreeView::waitTaskFinished(os);
    CHECK_SET_ERR(GTUtilsDocument::isDocumentLoaded(os, "murine.gb"), "The file is not loaded");
    QString title = GTUtilsMdi::activeWindowTitle(os);
    CHECK_SET_ERR(title.contains("NC_"), "Wrong MDI window is active");
}

GUI_TEST_CLASS_DEFINITION(test_5079) {
    //1. Open "COI.aln".
    GTFileDialog::openFile(os, dataDir + "samples/CLUSTALW", "COI.aln");
    GTUtilsTaskTreeView::waitTaskFinished(os);

    //2. Set the consensus type to "Levitsky".
    GTUtilsOptionPanelMsa::openTab(os, GTUtilsOptionPanelMsa::General);
    GTGlobals::sleep(200);
    QComboBox *consensusCombo = qobject_cast<QComboBox*>(GTWidget::findWidget(os, "consensusType"));
    CHECK_SET_ERR(consensusCombo != NULL, "consensusCombo is NULL");
    GTComboBox::setIndexWithText(os, consensusCombo, "Levitsky");

    //3. Add an additional sequence from file : "test/_common_data/fasta/amino_ext.fa".
    GTUtilsDialog::waitForDialog(os, new GTFileDialogUtils(os, testDir + "_common_data/fasta/amino_ext.fa"));
    GTToolbar::clickButtonByTooltipOnToolbar(os, MWTOOLBAR_ACTIVEMDI, "Align sequence to this alignment");
    GTUtilsTaskTreeView::waitTaskFinished(os);

    //4. Open the "Export consensus" OP tab.
    GTUtilsOptionPanelMsa::openTab(os, GTUtilsOptionPanelMsa::ExportConsensus);

    GTLogTracer l;

    //5. Press "Undo" button.
    GTUtilsMsaEditor::undo(os);
    GTUtilsTaskTreeView::waitTaskFinished(os);

    //6. Press "Redo" button.
    GTUtilsMsaEditor::redo(os);
    GTUtilsTaskTreeView::waitTaskFinished(os);
    //Expected state : the tab is successfully updated. No error in log.
    CHECK_SET_ERR(!l.hasError(), "unexpected errors in log");
}

GUI_TEST_CLASS_DEFINITION(test_5082) {
    GTLogTracer l;
    // 1. Open "_common_data/clustal/big.aln".
    GTFileDialog::openFile(os, testDir + "_common_data/clustal/big.aln");
    GTUtilsTaskTreeView::waitTaskFinished(os);

    // 2. Align it with MUSCLE.
    GTUtilsDialog::waitForDialog(os, new PopupChooserByText(os, QStringList() << "Align" << "Align with MUSCLE..."));
    GTUtilsDialog::waitForDialog(os, new MuscleDialogFiller(os));
    GTUtilsMSAEditorSequenceArea::callContextMenu(os);

    // Expected: Error notification appears with a correct human readable error. There is a error in log wit memory requirements.
    GTUtilsNotifications::waitForNotification(os, true, "There is not enough memory to align these sequences with MUSCLE.");
    GTUtilsTaskTreeView::waitTaskFinished(os);
    CHECK_SET_ERR(l.checkMessage("Not enough resources for the task, resource name:"), "No default error in log");
}

GUI_TEST_CLASS_DEFINITION(test_5090) {
//    1. Open "_common_data/genbank/join_complement_ann.gb".
//    Expected state: the file is successfully opened;
//                    a warning appears. It contains next message: "The file contains joined annotations with regions, located on different strands. All such joined parts will be stored on the same strand."
//                    there are two annotations: 'just_an_annotation' (40..50) and 'join_complement' (join(10..15,20..25)). // the second one should have another location after UGENE-3423 will be done

    GTLogTracer logTracer;
    GTUtilsNotifications::waitForNotification(os, false, "The file contains joined annotations with regions, located on different strands. All such joined parts will be stored on the same strand.");

    GTFileDialog::openFile(os, testDir + "_common_data/genbank/join_complement_ann.gb");
    GTUtilsTaskTreeView::waitTaskFinished(os);

    GTUtilsLog::checkContainsError(os, logTracer, "The file contains joined annotations with regions, located on different strands. All such joined parts will be stored on the same strand.");

    GTUtilsMdi::activateWindow(os, "join_complement_ann [s] A_SEQ_1");

    const QString simpleAnnRegion = GTUtilsAnnotationsTreeView::getAnnotationRegionString(os, "just_an_annotation");
    CHECK_SET_ERR("40..50" == simpleAnnRegion, QString("An incorrect annotation region: expected '%1', got '%2'").arg("40..50").arg(simpleAnnRegion));
    const QString joinComplementAnnRegion = GTUtilsAnnotationsTreeView::getAnnotationRegionString(os, "join_complement");
    CHECK_SET_ERR("join(10..15,20..25)" == joinComplementAnnRegion, QString("An incorrect annotation region: expected '%1', got '%2'").arg("join(10..15,20..25)").arg(simpleAnnRegion));
}

GUI_TEST_CLASS_DEFINITION(test_5128) {
    //1. Open any 3D structure.
    GTFileDialog::openFile(os, dataDir + "samples/PDB/1CF7.PDB");

    //2. Context menu: { Molecular Surface -> * }.
    //3. Select any model.
    //Current state: crash
    GTUtilsDialog::waitForDialog(os, new PopupChooserByText(os, QStringList() << "Molecular Surface" << "SAS"));
    GTWidget::click(os, GTWidget::findWidget(os, "1-1CF7"), Qt::RightButton);
    GTUtilsTaskTreeView::waitTaskFinished(os);
}

GUI_TEST_CLASS_DEFINITION(test_5137) {
    //    1. Open document test/_common_data/clustal/big.aln
    GTFileDialog::openFile(os, dataDir + "samples/CLUSTALW", "COI.aln");
    GTUtilsTaskTreeView::waitTaskFinished(os);
    //    2. Add big sequence 
    GTFileDialogUtils *ob = new GTFileDialogUtils(os, testDir + "_common_data/fasta/", "PF07724_full_family.fa");
    GTUtilsDialog::waitForDialog(os, ob);

    QAbstractButton *align = GTAction::button(os, "Align sequence to this alignment");
    CHECK_SET_ERR(align != NULL, "MSA \"Align sequence to this alignment\" action not found");
    GTWidget::click(os, align);
    GTUtilsNotifications::waitForNotification(os, true, "A problem occurred during adding sequences. The multiple alignment is no more available.");
    GTGlobals::sleep();
    GTUtilsProjectTreeView::click(os, "COI");
    GTKeyboardDriver::keyClick(Qt::Key_Delete);
    GTGlobals::sleep(6000);
}

GUI_TEST_CLASS_DEFINITION(test_5138_1) {
    //1. Open document test/_common_data/scenarios/msa/ma2_gapped.aln
    GTUtilsDialog::waitForDialog(os, new SequenceReadingModeSelectorDialogFiller(os, SequenceReadingModeSelectorDialogFiller::Join));
    GTFileDialog::openFile(os, testDir + "_common_data/scenarios/msa/", "big_aln.fa");
    GTUtilsTaskTreeView::waitTaskFinished(os);
    //2. Do MSA area context menu->Statistics->generate distance matrix
    //    Expected state: notification about low memory has appeared
    Runnable* dis = new DistanceMatrixDialogFiller(os, DistanceMatrixDialogFiller::NONE, testDir + "_common_data/scenarios/sandbox/matrix.html");
    GTUtilsDialog::waitForDialog(os, dis);
    Runnable* pop = new PopupChooser(os, QStringList() << MSAE_MENU_STATISTICS << "Generate distance matrix", GTGlobals::UseKey);
    GTUtilsDialog::waitForDialog(os, pop);
    GTMenu::showContextMenu(os, GTUtilsMdi::activeWindow(os));
    GTUtilsNotifications::waitForNotification(os, true, "not enough memory");
    GTUtilsTaskTreeView::waitTaskFinished(os);
    GTGlobals::sleep();
}

GUI_TEST_CLASS_DEFINITION(test_5138_2) {
    //    1. Open document test/_common_data/clustal/big.aln
    GTFileDialog::openFile(os, testDir + "_common_data/scenarios/_regression/5138", "big_5138.aln");
    GTUtilsTaskTreeView::waitTaskFinished(os);
    //    2. Do MSA area context menu->Statistics->generate grid profile
    //    Expected state: notification about low memory has appeared
    GTUtilsDialog::waitForDialog(os, new PopupChooser(os, QStringList() << MSAE_MENU_STATISTICS << "Generate grid profile", GTGlobals::UseKey));
    GTUtilsDialog::waitForDialog(os, new GenerateAlignmentProfileDialogFiller(os, true, GenerateAlignmentProfileDialogFiller::NONE,
        testDir + "_common_data/scenarios/sandbox/stat.html"));
    GTMenu::showContextMenu(os, GTUtilsMdi::activeWindow(os));
    GTUtilsNotifications::waitForNotification(os, true, "not enough memory");
    GTUtilsTaskTreeView::waitTaskFinished(os);
    GTGlobals::sleep();
}

} // namespace GUITest_regression_scenarios

} // namespace U2
