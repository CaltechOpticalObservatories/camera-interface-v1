#include "gtest/gtest.h"
#include "../utils/utilities.h"

TEST(UtilitiesTest, TimeStampNotLocalTest) {
    struct timespec timezero{};
    EXPECT_EQ(timestamp_from("GMT", timezero), "1970-01-01T00:00:00.000000");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
