#include "common/utility/memory_leak_detection.h"

void initialize_memory_leak_detection() {
#if IS_TRUE(PLATFORM_WINDOWS)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#else // IS_TRUE(PLATFORM_WINDOWS)
#error Not yet implemented
#endif // IS_TRUE(PLATFORM_WINDOWS)
}
