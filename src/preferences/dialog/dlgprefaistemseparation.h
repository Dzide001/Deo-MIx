#pragma once

#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/dialog/ui_dlgprefaistemseparationdlg.h"
#include "preferences/usersettings.h"

class DlgPrefAiStemSeparation : public DlgPreferencePage, public Ui::DlgPrefAiStemSeparationDlg {
    Q_OBJECT
  public:
    DlgPrefAiStemSeparation(QWidget* parent, UserSettingsPointer pConfig);
    ~DlgPrefAiStemSeparation() override = default;

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
