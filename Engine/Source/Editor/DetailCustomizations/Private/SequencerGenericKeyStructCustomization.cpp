// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SequencerGenericKeyStructCustomization.h"

#include "DetailLayoutBuilder.h"
#include "SequencerGenericKeyStruct.h"

#define LOCTEXT_NAMESPACE "SequencerGenericKeyStructCustomization"

TSharedRef<IDetailCustomization> FSequencerGenericKeyStructCustomization::MakeInstance()
{
	return MakeShared<FSequencerGenericKeyStructCustomization>();
}

void FSequencerGenericKeyStructCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TSharedPtr<FStructOnScope>> Structs;
	DetailBuilder.GetStructsBeingCustomized(Structs);

	const UStruct* StructType = Structs.Num() == 1 ? Structs[0]->GetStruct() : nullptr;
	if (StructType && StructType == FSequencerGenericKeyStruct::StaticStruct())
	{
		auto* StructValue = (FSequencerGenericKeyStruct*)Structs[0]->GetStructMemory();
		if (StructValue && StructValue->CustomizationImpl.IsValid())
		{
			StructValue->CustomizationImpl->Extend(DetailBuilder);
		}
	}
}

#undef LOCTEXT_NAMESPACE
