
#if !defined(SHIPPPING)
#include "..\Content\ContentLoader.h"
#include "..\Components\Script.h"
#include <thread>

bool engine_initialize()
{
	bool result{ quiet::content::load_game() };
	return result;
}

void engine_update()
{
	quiet::script::update(10.f);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void engine_shutdown()
{
	quiet::content::unload_game();
}

#endif // !defined(SHIPPING)