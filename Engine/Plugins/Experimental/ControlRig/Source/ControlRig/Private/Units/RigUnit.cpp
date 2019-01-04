// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Units/RigUnit.h"

DEFINE_LOG_CATEGORY_STATIC(LogRigUnit, Log, All);

// this will have to change in the future and move to editor, I assume the errors will be saved in the rig unit and it will print fromthe editor module
namespace UnitLogHelpers
{
	void PrintMissingHierarchy(const FName& InputName)
	{
		//UE_LOG(LogRigUnit, Warning, TEXT("%s: Input Hierarch Link is missing"), *InputName.ToString());
	}

	void PrintUnimplemented(const FName& InputName)
	{
		UE_LOG(LogRigUnit, Warning, TEXT("%s: Not implemented"), *InputName.ToString());
	}
}
