#pragma once

#include "preferences/usersettings.h"

// Saves/loads which library track-list columns the user has explicitly
// shown/hidden, to and from mixxx.cfg -- mirrors CustomPadSettings'
// UserSettings-wrapper pattern. One global setting (not per-view/source),
// matching the QML skin's own architecture where a single `defaultColumns`
// array already backs every sidebar source.
//
// Only columns the user has actually toggled are stored; a column never
// touched falls back to whatever default the caller (QML, per-column) asks
// for, so newly-added columns can default to hidden without needing a
// hardcoded curated list here.
class LibraryColumnSettings {
  public:
    explicit LibraryColumnSettings(UserSettingsPointer pConfig)
            : m_pConfig(pConfig) {
    }

    // columnIdx is a QmlLibraryTrackListColumn::SQLColumns value (really a
    // ColumnCache::Column). defaultHidden is what to return if the user
    // has never explicitly toggled this column.
    bool isColumnHidden(int columnIdx, bool defaultHidden) const;
    void setColumnHidden(int columnIdx, bool hidden);

  private:
    UserSettingsPointer m_pConfig;
};
