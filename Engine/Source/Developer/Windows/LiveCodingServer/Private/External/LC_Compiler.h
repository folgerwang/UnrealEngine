// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"

namespace environment
{
	struct Block;
}


namespace compiler
{
	// creates a new entry in the cache for the given compiler .exe, and returns it
	const environment::Block* CreateEnvironmentCacheEntry(const wchar_t* absolutePathToCompilerExe);

	// gets the environment for a given compiler .exe from the cache.
	// returns nullptr if the environment is not yet in the cache.
	const environment::Block* GetEnvironmentFromCache(const wchar_t* absolutePathToCompilerExe);

	// helper function that either creates a new entry in the cache if none exists yet,
	// or returns the one found in the cache.
	const environment::Block* UpdateEnvironmentCache(const wchar_t* absolutePathToCompilerExe);
}
