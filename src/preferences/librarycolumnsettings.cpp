#include "preferences/librarycolumnsettings.h"

namespace {

const QString kConfigGroup = QStringLiteral("[Library]");

QString itemForColumn(int columnIdx) {
    return QStringLiteral("col%1_hidden").arg(columnIdx);
}

} // namespace

bool LibraryColumnSettings::isColumnHidden(int columnIdx, bool defaultHidden) const {
    const ConfigKey key(kConfigGroup, itemForColumn(columnIdx));
    if (!m_pConfig->exists(key)) {
        return defaultHidden;
    }
    return m_pConfig->getValue<bool>(key);
}

void LibraryColumnSettings::setColumnHidden(int columnIdx, bool hidden) {
    m_pConfig->setValue<bool>(ConfigKey(kConfigGroup, itemForColumn(columnIdx)), hidden);
}
