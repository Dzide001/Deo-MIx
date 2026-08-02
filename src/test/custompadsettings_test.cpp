#include "preferences/custompadsettings.h"

#include "test/mixxxtest.h"

namespace {

class CustomPadSettingsTest : public MixxxTest {};

TEST_F(CustomPadSettingsTest, RoundTripsSetGetAssignment) {
    CustomPadSettings settings(config());

    EXPECT_FALSE(settings.isPadAssigned("[Channel1]", 0));
    EXPECT_TRUE(settings.getPadGroup("[Channel1]", 0).isEmpty());
    EXPECT_TRUE(settings.getPadKey("[Channel1]", 0).isEmpty());

    settings.setPadAssignment("[Channel1]", 0, "[Channel1]", "reverse", "Reverse");

    EXPECT_TRUE(settings.isPadAssigned("[Channel1]", 0));
    EXPECT_EQ(settings.getPadGroup("[Channel1]", 0), "[Channel1]");
    EXPECT_EQ(settings.getPadKey("[Channel1]", 0), "reverse");
    EXPECT_EQ(settings.getPadLabel("[Channel1]", 0), "Reverse");
}

TEST_F(CustomPadSettingsTest, ClearRemovesAssignment) {
    CustomPadSettings settings(config());

    settings.setPadAssignment("[Channel1]", 3, "[Channel1]", "quantize", "Quantize");
    ASSERT_TRUE(settings.isPadAssigned("[Channel1]", 3));

    settings.clearPadAssignment("[Channel1]", 3);

    EXPECT_FALSE(settings.isPadAssigned("[Channel1]", 3));
    EXPECT_TRUE(settings.getPadGroup("[Channel1]", 3).isEmpty());
    EXPECT_TRUE(settings.getPadKey("[Channel1]", 3).isEmpty());
    EXPECT_TRUE(settings.getPadLabel("[Channel1]", 3).isEmpty());
}

TEST_F(CustomPadSettingsTest, DecksAreIsolated) {
    CustomPadSettings settings(config());

    settings.setPadAssignment("[Channel1]", 2, "[Channel1]", "slip_enabled", "Slip");
    settings.setPadAssignment("[Channel2]", 2, "[Channel2]", "sync_enabled", "Sync");

    EXPECT_EQ(settings.getPadKey("[Channel1]", 2), "slip_enabled");
    EXPECT_EQ(settings.getPadKey("[Channel2]", 2), "sync_enabled");
    EXPECT_EQ(settings.getPadGroup("[Channel1]", 2), "[Channel1]");
    EXPECT_EQ(settings.getPadGroup("[Channel2]", 2), "[Channel2]");
}

TEST_F(CustomPadSettingsTest, SupportsAnExplicitGroupOverride) {
    CustomPadSettings settings(config());

    // A curated action can target a group other than the deck showing it
    // (e.g. "Internal Sync Leader" always targets "[InternalClock]"
    // regardless of which deck's Custom bank the pad lives on).
    settings.setPadAssignment("[Channel1]", 7, "[InternalClock]", "sync_leader", "Internal Sync Leader");

    EXPECT_EQ(settings.getPadGroup("[Channel1]", 7), "[InternalClock]");
    EXPECT_EQ(settings.getPadKey("[Channel1]", 7), "sync_leader");
}

} // namespace
