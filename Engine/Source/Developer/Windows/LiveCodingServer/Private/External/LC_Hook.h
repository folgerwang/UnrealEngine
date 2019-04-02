// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <stdint.h>

class ImmutableString;

namespace symbols
{
	struct ImageSectionDB;
}


namespace hook
{
	typedef void (*Function)(void);

	uint32_t FindFirstInSection(const symbols::ImageSectionDB* imageSectionDb, const ImmutableString& sectionName);
	uint32_t FindLastInSection(const symbols::ImageSectionDB* imageSectionDb, const ImmutableString& sectionName);

	const Function* MakeFunction(const void* moduleBase, uint32_t rva);
}
