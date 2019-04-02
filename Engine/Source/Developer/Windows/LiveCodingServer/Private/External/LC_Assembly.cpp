// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Assembly.h"
#include <string.h>


namespace
{
	template <typename T>
	static uint8_t WriteAssembly(uint8_t*& code, T value)
	{
		memcpy(code, &value, sizeof(T));
		code += sizeof(T);

		return static_cast<uint8_t>(sizeof(T));
	}
}


// NOP, 1 byte
assembly::Instruction assembly::MakeNOP(void)
{
	Instruction instr = {};
	uint8_t* code = instr.code;

	instr.size += WriteAssembly<uint8_t>(code, 0x90);

	return instr;
}


// Jump near, relative 32-bit, displacement relative to next instruction.
// 5 bytes: opcode (1b) followed by address (4b)
// http://www.felixcloutier.com/x86/JMP.html
assembly::Instruction assembly::MakeRelativeNearJump(int32_t displacement)
{
	Instruction instr = {};
	uint8_t* code = instr.code;

	instr.size += WriteAssembly<uint8_t>(code, 0xE9);					// opcode
	instr.size += WriteAssembly<int32_t>(code, displacement - 5);		// displacement is relative to the next instruction

	return instr;
}


// Jump short, relative 32-bit, displacement relative to next instruction.
// 2 bytes: opcode (1b) followed by address (1b)
// http://www.felixcloutier.com/x86/JMP.html
assembly::Instruction assembly::MakeRelativeShortJump(int8_t displacement)
{
	Instruction instr = {};
	uint8_t* code = instr.code;

	instr.size += WriteAssembly<uint8_t>(code, 0xEB);					// opcode
	instr.size += WriteAssembly<int8_t>(code, displacement - 2);		// displacement is relative to the next instruction

	return instr;
}
