// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDetailSourcedArrayBuilder.h"
#include "NiagaraDataInterfaceDetails.h"
#include "NiagaraDataInterfaceSkeletalMesh.h" 
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "SNiagaraNamePropertySelector.h"

#define LOCTEXT_NAMESPACE "FNiagaraDetailSourcedArrayBuilder"

FNiagaraDetailSourcedArrayBuilder::FNiagaraDetailSourcedArrayBuilder(TSharedRef<IPropertyHandle> InBaseProperty, const TArray<TSharedPtr<FName>>& InOptionsSource, const FName InFNameSubproperty, bool InGenerateHeader, bool InDisplayResetToDefault, bool InDisplayElementNum)
	: FDetailArrayBuilder(InBaseProperty, InGenerateHeader, InDisplayResetToDefault, InDisplayElementNum)
	, OptionsSourceList(InOptionsSource)
	, ArrayProperty(InBaseProperty->AsArray())
	, FNameSubproperty(InFNameSubproperty)
{
	// bradut TODO set new Delegate for when the number of children in the array changes, where you empty the map of selectors and double check if it is regenerated and also call 
	// 
	FSimpleDelegate OnArrayNumChildrenChanged = FSimpleDelegate::CreateRaw(this, &FNiagaraDetailSourcedArrayBuilder::OnArrayNumChildrenChanged);
	ArrayProperty->SetOnNumElementsChanged(OnArrayNumChildrenChanged);
	uint32 EntryCount;
	ArrayProperty->GetNumElements(EntryCount);
	NameSelectors.AddZeroed(EntryCount);
};

void FNiagaraDetailSourcedArrayBuilder::OnGenerateEntry(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	IDetailPropertyRow& RegionRow = ChildrenBuilder.AddProperty(PropertyHandle);

	FNumberFormattingOptions NoCommas;
	NoCommas.UseGrouping = false;
	const FText SlotDesc = FText::Format(LOCTEXT("ElementIndex", "Element #{0}"), FText::AsNumber(ArrayIndex, &NoCommas));

	RegionRow.DisplayName(SlotDesc);

	RegionRow.ShowPropertyButtons(true);

	TSharedPtr<IPropertyHandle> SubPropertyHandle = PropertyHandle;
	if (FNameSubproperty != NAME_None)
	{
		SubPropertyHandle = PropertyHandle->GetChildHandle(FNameSubproperty).ToSharedRef();
	}
	NameSelectors[ArrayIndex] = SNew(SNiagaraNamePropertySelector, SubPropertyHandle.ToSharedRef(), OptionsSourceList);
	RegionRow.CustomWidget(false)
		.NameContent()
		[
			SubPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(TOptional<float>())
		[
			// add combo button
			NameSelectors[ArrayIndex].ToSharedRef()
		];
}

void FNiagaraDetailSourcedArrayBuilder::OnArrayNumChildrenChanged()
{
	NameSelectors.Empty();
	uint32 EntryCount;
	ArrayProperty->GetNumElements(EntryCount);
	NameSelectors.AddZeroed(EntryCount); // allocate the memory for ease of use when refreshing children
	RefreshChildren();
}

void FNiagaraDetailSourcedArrayBuilder::SetSourceArray(TArray<TSharedPtr<FName>>& InOptionsSource)
{
	OptionsSourceList = InOptionsSource;
	for (auto NameSelector : NameSelectors)
	{
		if (NameSelector.IsValid())
		{
			NameSelector->SetSourceArray(InOptionsSource);
		}
	}
}

void FNiagaraDetailSourcedArrayBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	uint32 NumChildren = 0;
	ArrayProperty->GetNumElements(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ElementHandle = ArrayProperty->GetElement(ChildIndex);
		
		OnGenerateEntry(ElementHandle, ChildIndex, ChildrenBuilder);
	}
}


#undef LOCTEXT_NAMESPACE