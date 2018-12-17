// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AnimNode_StateResult.h"

bool FAnimNode_StateResult::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	if(Tag.Type == NAME_StructProperty && Tag.StructName == FAnimNode_Root::StaticStruct()->GetFName())
	{
		FAnimNode_Root OldValue;
		FAnimNode_Root::StaticStruct()->SerializeItem(Slot, &OldValue, nullptr);
		*static_cast<FAnimNode_Root*>(this) = OldValue;

		return true;
	}

	return false;
}