// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Styling/SlateColor.h"
#include "UObject/PropertyTag.h"

bool FSlateColor::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	if (Tag.Type == NAME_StructProperty)
	{
		if (Tag.StructName == NAME_Color)
		{
			FColor OldColor;
			Slot << OldColor;
			*this = FSlateColor(FLinearColor(OldColor));

			return true;
		}
		else if(Tag.StructName == NAME_LinearColor)
		{
			FLinearColor OldColor;
			Slot << OldColor;
			*this = FSlateColor(OldColor);

			return true;
		}
	}

	return false;
}
