// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Process.h"


namespace disassembler
{
	// returns the size of the first instruction found at the given address
	size_t FindInstructionSize(process::Handle processHandle, const void* address);

	// returns the address of the previous instruction inside a function
	const void* FindPreviousInstructionAddress(process::Handle processHandle, const void* functionStart, const void* instructionAddress);
}
