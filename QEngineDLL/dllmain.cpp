// dllmain.cpp : Defines the entry point for the DLL application.
#pragma comment(lib, "QEngine.lib")

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <crtdbg.h>

BOOL APIENTRY dllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
#if _DEBUG
        // Check for memory leaks
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

