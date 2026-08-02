#pragma once

#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/ui_dlgprefailyrictranscriptiondlg.h"
#include "preferences/usersettings.h"

class DlgPrefAiLyricTranscription : public DlgPreferencePage,
                                     public Ui::DlgPrefAiLyricTranscriptionDlg {
    Q_OBJECT
  public:
    DlgPrefAiLyricTranscription(QWidget* parent, UserSettingsPointer pConfig);
    ~DlgPrefAiLyricTranscription() override = default;

  public slots:
    void slotApply() override;
    void slotUpdate() override;
    void slotResetToDefaults() override;

  private slots:
    void slotBrowseModel();

  private:
    void updateStatusLabel();

    UserSettingsPointer m_pConfig;
};
