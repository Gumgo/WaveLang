#include "common/common.h"
#include "common/math/floating_point.h"

#include <gtest/gtest.h>

#include <iostream>

int main(int argc, char **argv) {
	initialize_floating_point_behavior();

	testing::InitGoogleTest(&argc, argv);
	int32 result = RUN_ALL_TESTS();

#if IS_TRUE(PLATFORM_WINDOWS) && IS_TRUE(COMPILER_MSVC)
	if (IsDebuggerPresent()) {
		std::cout << "Press <enter> to continue\n";
		while (getchar() != '\n');
	}
#endif // IS_TRUE(PLATFORM_WINDOWS) && IS_TRUE(COMPILER_MSVC)

	return result;
}
