#pragma once

#include "preferences/usersettings.h"

// Saves/loads each deck's Custom pad-bank assignments to and from
// mixxx.cfg, mirroring ColorPaletteSettings' UserSettings-wrapper pattern.
// One config group per deck (e.g. "[CustomPads_Channel1]"), holding 3 keys
// per pad index (pad<N>_group / pad<N>_key / pad<N>_label) so each deck's
// Custom-bank assignments persist and stay independent per deck.
class CustomPadSettings {
  public:
    explicit CustomPadSettings(UserSettingsPointer pConfig)
            : m_pConfig(pConfig) {
    }

    static constexpr int kNumPads = 8;

    // deckGroup is the deck's own ControlObject group, e.g. "[Channel1]" --
    // used both to derive the per-deck config group and, by convention, as
    // the default trigger target for an assignment that didn't specify an
    // explicit group override (that resolution happens on the QML side,
    // not here -- this class only stores/retrieves whatever group string
    // it was given).
    QString getPadGroup(const QString& deckGroup, int padIndex) const;
    QString getPadKey(const QString& deckGroup, int padIndex) const;
    QString getPadLabel(const QString& deckGroup, int padIndex) const;
    bool isPadAssigned(const QString& deckGroup, int padIndex) const;

    void setPadAssignment(
            const QString& deckGroup,
            int padIndex,
            const QString& group,
            const QString& key,
            const QString& label);
    void clearPadAssignment(const QString& deckGroup, int padIndex);

  private:
    static QString configGroupForDeck(const QString& deckGroup);

    UserSettingsPointer m_pConfig;
};
