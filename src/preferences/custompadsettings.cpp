#include "preferences/custompadsettings.h"

namespace {

QString itemForPad(int padIndex, const QString& suffix) {
    return QStringLiteral("pad%1_%2").arg(padIndex).arg(suffix);
}

} // namespace

QString CustomPadSettings::configGroupForDeck(const QString& deckGroup) {
    // deckGroup looks like "[Channel1]" -- strip the brackets and build a
    // dedicated config group so each deck's Custom-bank assignments
    // persist independently, mirroring StemPads.qml/StemsBankContent.qml's
    // own bracket-stripping stemGroup() helper.
    QString stripped = deckGroup;
    stripped.remove(QLatin1Char('['));
    stripped.remove(QLatin1Char(']'));
    return QStringLiteral("[CustomPads_%1]").arg(stripped);
}

QString CustomPadSettings::getPadGroup(const QString& deckGroup, int padIndex) const {
    return m_pConfig->getValueString(
            ConfigKey(configGroupForDeck(deckGroup), itemForPad(padIndex, QStringLiteral("group"))));
}

QString CustomPadSettings::getPadKey(const QString& deckGroup, int padIndex) const {
    return m_pConfig->getValueString(
            ConfigKey(configGroupForDeck(deckGroup), itemForPad(padIndex, QStringLiteral("key"))));
}

QString CustomPadSettings::getPadLabel(const QString& deckGroup, int padIndex) const {
    return m_pConfig->getValueString(
            ConfigKey(configGroupForDeck(deckGroup), itemForPad(padIndex, QStringLiteral("label"))));
}

bool CustomPadSettings::isPadAssigned(const QString& deckGroup, int padIndex) const {
    return !getPadKey(deckGroup, padIndex).isEmpty();
}

void CustomPadSettings::setPadAssignment(
        const QString& deckGroup,
        int padIndex,
        const QString& group,
        const QString& key,
        const QString& label) {
    const QString configGroup = configGroupForDeck(deckGroup);
    m_pConfig->setValue(ConfigKey(configGroup, itemForPad(padIndex, QStringLiteral("group"))), group);
    m_pConfig->setValue(ConfigKey(configGroup, itemForPad(padIndex, QStringLiteral("key"))), key);
    m_pConfig->setValue(ConfigKey(configGroup, itemForPad(padIndex, QStringLiteral("label"))), label);
}

void CustomPadSettings::clearPadAssignment(const QString& deckGroup, int padIndex) {
    const QString configGroup = configGroupForDeck(deckGroup);
    m_pConfig->remove(ConfigKey(configGroup, itemForPad(padIndex, QStringLiteral("group"))));
    m_pConfig->remove(ConfigKey(configGroup, itemForPad(padIndex, QStringLiteral("key"))));
    m_pConfig->remove(ConfigKey(configGroup, itemForPad(padIndex, QStringLiteral("label"))));
}
