// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <string>

namespace compilerOptions
{
	bool CreatesPrecompiledHeader(const char* options);
	bool UsesPrecompiledHeader(const char* options);
	std::string GetPrecompiledHeaderPath(const char* options);

	bool UsesC7DebugFormat(const char* options);
	bool UsesMinimalRebuild(const char* options);
}
