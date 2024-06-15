
#pragma comment(lib, "Engine.lib")

#include "TestEntityComponents.h"

#define TEST_ENTITY_COMPONENT 1

#if TEST_ENTITY_COMPONENT

#else
#error One of the tests need to be enabled
#endif

int main()
{
#if _DEBUG
	// Check for memory leaks
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	new float[4];
	engine_test test{};

	if(test.initialize())
	{
		test.run();
	}

	test.shutdown();
}