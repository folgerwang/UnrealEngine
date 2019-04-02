// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <stdint.h>

namespace assembly
{
	// the longest instruction on x86 is 15 bytes (opcode + 14 bytes), which makes this fit into 16 bytes nicely
	struct Instruction
	{
		static const unsigned int MAX_SIZE = 15u;

		uint8_t size;
		uint8_t code[MAX_SIZE];
	};

	Instruction MakeNOP(void);
	Instruction MakeRelativeNearJump(int32_t displacement);
	Instruction MakeRelativeShortJump(int8_t displacement);
}
