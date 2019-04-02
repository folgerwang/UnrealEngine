// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Process.h"


namespace patch
{
	void InstallNOPs(process::Handle processHandle, void* address, size_t size);
	void InstallJumpToSelf(process::Handle processHandle, void* address);

	void InstallRelativeShortJump(process::Handle processHandle, void* address, void* destination);
	void InstallRelativeNearJump(process::Handle processHandle, void* address, void* destination);
}
