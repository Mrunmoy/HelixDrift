#include "BlinkEngine.hpp"
#include <gtest/gtest.h>

using helix::BlinkEngine;

TEST(BlinkEngineTest, StartsOnByDefault) {
    BlinkEngine blink(100, 200);
    EXPECT_TRUE(blink.level());
}

TEST(BlinkEngineTest, StaysOnBeforeOnWindowExpires) {
    BlinkEngine blink(100, 100);
    EXPECT_TRUE(blink.tick(50));
    EXPECT_TRUE(blink.level());
}

TEST(BlinkEngineTest, TogglesOffWhenOnWindowExpires) {
    BlinkEngine blink(100, 200);
    EXPECT_FALSE(blink.tick(100));
    EXPECT_FALSE(blink.level());
}

TEST(BlinkEngineTest, HandlesMultiplePhaseCrossings) {
    BlinkEngine blink(100, 100);
    EXPECT_TRUE(blink.tick(250));
    EXPECT_TRUE(blink.level());
}

TEST(BlinkEngineTest, SupportsStartOff) {
    BlinkEngine blink(100, 200, false);
    EXPECT_FALSE(blink.level());
    EXPECT_TRUE(blink.tick(200));
}

TEST(BlinkEngineTest, ResetReturnsToInitialPhase) {
    BlinkEngine blink(50, 50);
    blink.tick(60);
    EXPECT_FALSE(blink.level());
    blink.reset();
    EXPECT_TRUE(blink.level());
}
