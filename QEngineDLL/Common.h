#pragma once

// This will allow to export any method to dll -> extern "C" will detect the function
#ifndef EDITOR_INTERFACE
	#define EDITOR_INTERFACE extern "C" __declspec(dllexport)
#endif
