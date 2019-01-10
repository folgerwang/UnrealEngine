// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderSource.h"
#include "Styling/SlateIconFinder.h"

UTakeRecorderSource::UTakeRecorderSource(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	,bEnabled(true)
	,TakeNumber(0)
{
	TrackTint = FColor(127, 127, 127);
}

const FSlateBrush* UTakeRecorderSource::GetDisplayIconImpl() const
{
	return FSlateIconFinder::FindCustomIconBrushForClass(GetClass(), TEXT("ClassThumbnail"));
}