// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "VariantObjectBinding.h"

#include "Variant.h"
#include "MovieSceneTrack.h"


UVariantObjectBinding::UVariantObjectBinding(const FObjectInitializer& Init)
{

}

UVariantObjectBinding* UVariantObjectBinding::Clone(UObject* ClonesOuter)
{
	if (ClonesOuter == INVALID_OBJECT)
	{
		ClonesOuter = GetOuter();
	}

	UVariantObjectBinding* NewBinding = DuplicateObject(this, ClonesOuter);
	NewBinding->Init(GetObject(), GetSortingOrder() + 1);

	return NewBinding;
}

UVariant* UVariantObjectBinding::GetParent()
{
	return Cast<UVariant>(GetOuter());
}
