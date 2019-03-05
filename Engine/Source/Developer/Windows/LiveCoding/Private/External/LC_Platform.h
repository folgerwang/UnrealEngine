// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "LC_Preprocessor.h"

#define LC_FILE								__FILE__
#define LC_LINE								__LINE__
#define LC_FUNCTION_NAME					__FUNCTION__
#define LC_FUNCTION_SIGNATURE				__FUNCSIG__
#define LC_UNUSED(_value)					(void)_value
#define LC_ASSERT(_condition, _msg)			checkf(_condition, TEXT("%s"), TEXT(_msg))

#define LC_DISABLE_COPY(_name)				_name(const _name&) = delete
#define LC_DISABLE_MOVE(_name)				_name(_name&&) = delete
#define LC_DISABLE_ASSIGNMENT(_name)		_name& operator=(const _name&) = delete
#define LC_DISABLE_MOVE_ASSIGNMENT(_name)	_name& operator=(_name&&) = delete

#if PLATFORM_64BITS
#	define LC_64_BIT 1
#	define LC_32_BIT 0
#else
#	define LC_64_BIT 0
#	define LC_32_BIT 1
#endif

// convenience macro for referring to symbol identifiers.
// some identifiers contain an extra leading underscore in 32-bit builds.
#if LC_64_BIT
#	define LC_IDENTIFIER(_name)		_name
#else
#	define LC_IDENTIFIER(_name)		"_" _name
#endif
