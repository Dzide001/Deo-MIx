#include "preferences/librarycolumnsettings.h"

#include "test/mixxxtest.h"

namespace {

class LibraryColumnSettingsTest : public MixxxTest {};

TEST_F(LibraryColumnSettingsTest, NeverToggledColumnFallsBackToDefault) {
    LibraryColumnSettings settings(config());

    EXPECT_FALSE(settings.isColumnHidden(42, false));
    EXPECT_TRUE(settings.isColumnHidden(42, true));
}

TEST_F(LibraryColumnSettingsTest, RoundTripsExplicitToggle) {
    LibraryColumnSettings settings(config());

    settings.setColumnHidden(5, true);
    // Once explicitly set, the stored value wins regardless of the
    // caller's own default.
    EXPECT_TRUE(settings.isColumnHidden(5, false));

    settings.setColumnHidden(5, false);
    EXPECT_FALSE(settings.isColumnHidden(5, true));
}

TEST_F(LibraryColumnSettingsTest, ColumnsAreIndependent) {
    LibraryColumnSettings settings(config());

    settings.setColumnHidden(1, true);
    settings.setColumnHidden(2, false);

    EXPECT_TRUE(settings.isColumnHidden(1, false));
    EXPECT_FALSE(settings.isColumnHidden(2, true));
    // A third, never-touched column is unaffected by either.
    EXPECT_TRUE(settings.isColumnHidden(3, true));
    EXPECT_FALSE(settings.isColumnHidden(3, false));
}

} // namespace
