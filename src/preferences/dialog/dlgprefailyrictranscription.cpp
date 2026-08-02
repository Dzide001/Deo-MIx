#include "preferences/dialog/dlgprefailyrictranscription.h"

#include <QFileDialog>
#include <QFileInfo>

#include "library/lyrictranscription/defs_lyrictranscription.h"
#include "moc_dlgprefailyrictranscription.cpp"

DlgPrefAiLyricTranscription::DlgPrefAiLyricTranscription(
        QWidget* parent, UserSettingsPointer pConfig)
        : DlgPreferencePage(parent),
          m_pConfig(pConfig) {
    setupUi(this);

    connect(PushButtonBrowseModel,
            &QAbstractButton::clicked,
            this,
            &DlgPrefAiLyricTranscription::slotBrowseModel);
    connect(LineEditModelPath,
            &QLineEdit::textChanged,
            this,
            &DlgPrefAiLyricTranscription::updateStatusLabel);

    slotUpdate();
}

void DlgPrefAiLyricTranscription::slotUpdate() {
    LineEditModelPath->setText(
            m_pConfig->getValueString(ConfigKey(AI_LYRIC_TRANSCRIPTION_PREF_KEY, "ModelPath")));
    updateStatusLabel();
}

void DlgPrefAiLyricTranscription::slotApply() {
    m_pConfig->setValue(
            ConfigKey(AI_LYRIC_TRANSCRIPTION_PREF_KEY, "ModelPath"), LineEditModelPath->text());
}

void DlgPrefAiLyricTranscription::slotResetToDefaults() {
    LineEditModelPath->setText(QString());
    updateStatusLabel();
}

void DlgPrefAiLyricTranscription::slotBrowseModel() {
    // DontUseNativeDialog: matches DlgPrefAiStemSeparation's own browse
    // dialog -- the native macOS file panel has been reported to hang
    // when triggered from this Qt Quick-based app.
    const QString path = QFileDialog::getOpenFileName(
            this,
            tr("Select Whisper GGML Model"),
            LineEditModelPath->text(),
            tr("GGML Model (*.bin)"),
            nullptr,
            QFileDialog::DontUseNativeDialog);
    if (!path.isEmpty()) {
        LineEditModelPath->setText(path);
    }
}

void DlgPrefAiLyricTranscription::updateStatusLabel() {
    const QString path = LineEditModelPath->text();
    if (path.isEmpty()) {
        LabelModelStatus->setText(tr("Not set"));
    } else if (QFileInfo::exists(path)) {
        LabelModelStatus->setText(tr("Found"));
    } else {
        LabelModelStatus->setText(tr("File missing!"));
    }
}
