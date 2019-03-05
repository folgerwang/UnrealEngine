// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Globals.h"

extern "C" __declspec(dllexport) void InitNatvisHelpers(FNameEntry*** NameTable, FChunkedFixedUObjectArray* ObjectArray)
{
	GFNameTableForDebuggerVisualizers_MT = NameTable;
	GObjectArrayForDebugVisualizers = ObjectArray;
}
