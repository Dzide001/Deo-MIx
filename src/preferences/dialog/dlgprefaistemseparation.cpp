#include "preferences/dialog/dlgprefaistemseparation.h"

#include <QFileDialog>
#include <QFileInfo>

#include "library/stemseparation/defs_stemseparation.h"
#include "moc_dlgprefaistemseparation.cpp"

DlgPrefAiStemSeparation::DlgPrefAiStemSeparation(QWidget* parent, UserSettingsPointer pConfig)
        : DlgPreferencePage(parent),
          m_pConfig(pConfig) {
    setupUi(this);

    connect(PushButtonBrowseModel,
            &QAbstractButton::clicked,
            this,
            &DlgPrefAiStemSeparation::slotBrowseModel);
    connect(LineEditModelPath,
            &QLineEdit::textChanged,
            this,
            &DlgPrefAiStemSeparation::updateStatusLabel);

    slotUpdate();
}

void DlgPrefAiStemSeparation::slotUpdate() {
    LineEditModelPath->setText(
            m_pConfig->getValueString(ConfigKey(AI_STEM_SEPARATION_PREF_KEY, "ModelPath")));
    updateStatusLabel();
}

void DlgPrefAiStemSeparation::slotApply() {
    m_pConfig->setValue(
            ConfigKey(AI_STEM_SEPARATION_PREF_KEY, "ModelPath"), LineEditModelPath->text());
}

void DlgPrefAiStemSeparation::slotResetToDefaults() {
    LineEditModelPath->setText(QString());
    updateStatusLabel();
}

void DlgPrefAiStemSeparation::slotBrowseModel() {
    const QString path = QFileDialog::getOpenFileName(
            this,
            tr("Select HTDemucs ONNX Model"),
            LineEditModelPath->text(),
            tr("ONNX Model (*.onnx)"));
    if (!path.isEmpty()) {
        LineEditModelPath->setText(path);
    }
}

void DlgPrefAiStemSeparation::updateStatusLabel() {
    const QString path = LineEditModelPath->text();
    if (path.isEmpty()) {
        LabelModelStatus->setText(tr("Not set"));
    } else if (QFileInfo::exists(path)) {
        LabelModelStatus->setText(tr("Found"));
    } else {
        LabelModelStatus->setText(tr("File missing!"));
    }
}
