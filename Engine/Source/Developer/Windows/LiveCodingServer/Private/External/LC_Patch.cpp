// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Patch.h"
#include "LC_Assembly.h"
#include "LC_PointerUtil.h"
#include "LC_Platform.h"
#include "LC_Logging.h"


namespace
{
	void Install(process::Handle processHandle, void* address, const assembly::Instruction& instruction)
	{
		process::WriteProcessMemory(processHandle, address, instruction.code, instruction.size);
		process::FlushInstructionCache(processHandle, address, instruction.size);
	}
}


void patch::InstallNOPs(process::Handle processHandle, void* address, size_t size)
{
	const assembly::Instruction nop = assembly::MakeNOP();
	for (size_t i = 0u; i < size; ++i)
	{
		process::WriteProcessMemory(processHandle, pointer::Offset<void*>(address, i), nop.code, nop.size);
	}

	process::FlushInstructionCache(processHandle, address, size);
}


void patch::InstallJumpToSelf(process::Handle processHandle, void* address)
{
	InstallRelativeShortJump(processHandle, address, address);
}


void patch::InstallRelativeShortJump(process::Handle processHandle, void* address, void* destination)
{
	char* oldFuncAddr = static_cast<char*>(address);
	char* newFuncAddr = static_cast<char*>(destination);

	const ptrdiff_t displacement = newFuncAddr - oldFuncAddr;
	LC_ASSERT(displacement >= std::numeric_limits<int8_t>::min(), "Displacement is out-of-range.");
	LC_ASSERT(displacement <= std::numeric_limits<int8_t>::max(), "Displacement is out-of-range.");

	const assembly::Instruction jump = assembly::MakeRelativeShortJump(static_cast<int8_t>(displacement));
	Install(processHandle, address, jump);
}


void patch::InstallRelativeNearJump(process::Handle processHandle, void* address, void* destination)
{
	char* oldFuncAddr = static_cast<char*>(address);
	char* newFuncAddr = static_cast<char*>(destination);

	const ptrdiff_t displacement = newFuncAddr - oldFuncAddr;
	LC_ASSERT(displacement >= std::numeric_limits<int32_t>::min(), "Displacement is out-of-range.");
	LC_ASSERT(displacement <= std::numeric_limits<int32_t>::max(), "Displacement is out-of-range.");

	const assembly::Instruction jump = assembly::MakeRelativeNearJump(static_cast<int32_t>(displacement));
	Install(processHandle, address, jump);
}
