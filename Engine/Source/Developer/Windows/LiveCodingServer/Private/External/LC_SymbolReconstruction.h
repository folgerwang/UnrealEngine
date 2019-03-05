// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Coff.h"
#include "LC_Symbols.h"
#include "LC_Process.h"
#include "LC_Executable.h"


namespace symbols
{
	typedef types::unordered_map<uint32_t, std::wstring> DiaSymbolCache;

	void ReconstructFromExecutableCoff
	(
		const symbols::Provider* provider,
		const executable::Image* image,
		const executable::ImageSectionDB* imageSections,
		const coff::CoffDB* coffDb,
		const types::StringSet& strippedSymbols,
		const symbols::ObjPath& objPath,
		const symbols::CompilandDB* compilandDb,
		const symbols::ContributionDB* contributionDb,
		const symbols::ThunkDB* thunkDb,
		const symbols::ImageSectionDB* imageSectionDb,
		symbols::SymbolDB* symbolDB,
		DiaSymbolCache* diaSymbolCache
	);
}
