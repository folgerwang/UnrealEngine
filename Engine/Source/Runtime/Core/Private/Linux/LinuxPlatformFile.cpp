// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxPlatformFile.h"

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FUnixPlatformFile Singleton;
	return Singleton;
}
