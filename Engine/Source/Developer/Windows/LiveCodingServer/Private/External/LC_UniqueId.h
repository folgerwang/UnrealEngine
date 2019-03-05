// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <string>

namespace uniqueId
{
	void Startup(void);
	void Shutdown(void);

	uint32_t Generate(const std::wstring& path);
}
