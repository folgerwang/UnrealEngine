// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <string>

namespace environment
{
	struct Block;

	Block* CreateBlockFromFile(const wchar_t* path);
	void DestroyBlock(Block*& block);

	void DumpBlockData(const wchar_t* name, const Block* block);

	const void* GetBlockData(const Block* block);
	const size_t GetBlockSize(const Block* block);

	std::wstring GetVariable(const wchar_t* variable);
}
