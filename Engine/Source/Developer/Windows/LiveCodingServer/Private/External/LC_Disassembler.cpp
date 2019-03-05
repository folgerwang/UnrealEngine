// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Disassembler.h"
#include "LC_PointerUtil.h"
#include "LC_Platform.h"
#include "LC_Logging.h"
#include "distorm.h"


namespace
{
	// an x86/x64 instruction is at most 16 bytes long
	static const size_t LONGEST_X86_INSTRUCTION = 16u;
}


size_t disassembler::FindInstructionSize(process::Handle processHandle, const void* address)
{
	uint8_t code[LONGEST_X86_INSTRUCTION] = {};
	process::ReadProcessMemory(processHandle, address, code, LONGEST_X86_INSTRUCTION);

	_CodeInfo codeInfo = {};
	codeInfo.code = code;
	codeInfo.codeLen = LONGEST_X86_INSTRUCTION;
	codeInfo.codeOffset = pointer::AsInteger<uint64_t>(address);

#if LC_64_BIT
	codeInfo.dt = Decode64Bits;
#else
	codeInfo.dt = Decode32Bits;
#endif

	_DInst result = {};
	unsigned int instructionCount = 0u;
	distorm_decompose(&codeInfo, &result, 1u, &instructionCount);

	if (instructionCount == 0u)
	{
		// something went horribly wrong
		LC_ERROR_DEV("Could not disassemble instruction at 0x%p", address);
		return 0u;
	}

	if (result.flags == FLAG_NOT_DECODABLE)
	{
		// the opcode could not be decoded
		LC_ERROR_DEV("Could not decode instruction at 0x%p", address);
		return 0u;
	}

	return result.size;
}


const void* disassembler::FindPreviousInstructionAddress(process::Handle processHandle, const void* functionStart, const void* instructionAddress)
{
	// starting at the function, disassemble instructions until we arrive at the given address
	const void* currentAddress = functionStart;
	for (;;)
	{
		const size_t currentSize = FindInstructionSize(processHandle, currentAddress);
		if (currentSize == 0u)
		{
			// something went wrong
			break;
		}

		const void* previousAddress = currentAddress;
		currentAddress = pointer::Offset<const void*>(currentAddress, currentSize);
		if (currentAddress == instructionAddress)
		{
			// we just decoded the instruction right before the given address
			return previousAddress;
		}

		if (currentAddress > instructionAddress)
		{
			// something went wrong, we should arrive *exactly* at the given address
			break;
		}
	}

	return nullptr;
}
