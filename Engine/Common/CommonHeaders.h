#pragma once

#pragma warning(disable: 4530) // Disable exception warning for utilities

// C/C++
#include <stdint.h>
#include <assert.h>
#include <type_traits>
#include <memory>
#include <unordered_map>

// Windows Specific Headers
#if defined(_WIN64)
#include <DirectXMath.h>
#endif

// Common Headers
#include "../Utilities/Utilities.h"
#include "../Utilities/MathTypes.h"
#include "PrimitiveTypes.h"
#include "Id.h"

// Macros

#ifdef _DEBUG
#define DEBUG_OP(x) x
#else
#define DEBUG_OP(x) (void(0))
#endif
