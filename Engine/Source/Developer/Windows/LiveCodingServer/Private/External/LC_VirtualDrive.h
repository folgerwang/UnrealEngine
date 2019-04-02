// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"

namespace virtualDrive
{
	void Add(const wchar_t* driveLetterPlusColon, const wchar_t* path);
	void Remove(const wchar_t* driveLetterPlusColon, const wchar_t* path);
}
