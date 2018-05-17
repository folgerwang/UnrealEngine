 // Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
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
};

void FNiagaraDetailSourcedArrayBuilder::OnGenerateEntry(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	TSharedRef<IPropertyHandle> SubPropertyHandle = PropertyHandle;
	if (FNameSubproperty != NAME_None)
	{
		SubPropertyHandle = PropertyHandle->GetChildHandle(FNameSubproperty).ToSharedRef();
	}
	IDetailPropertyRow& RegionRow = ChildrenBuilder.AddProperty(PropertyHandle);

	FNumberFormattingOptions NoCommas;
	NoCommas.UseGrouping = false;
	const FText SlotDesc = FText::Format(LOCTEXT("ElementIndex", "Element #{0}"), FText::AsNumber(ArrayIndex, &NoCommas));

	RegionRow.DisplayName(SlotDesc);

	RegionRow.ShowPropertyButtons(true);
	
	NameSelector = SNew(SNiagaraNamePropertySelector, SubPropertyHandle, OptionsSourceList);
	
	RegionRow.CustomWidget(false)
		.NameContent()
		[
			SubPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(TOptional<float>())
		[
			// add combo button
			NameSelector.ToSharedRef()
		];
}

void FNiagaraDetailSourcedArrayBuilder::SetSourceArray(TArray<TSharedPtr<FName>>& InOptionsSource)
{
	OptionsSourceList = InOptionsSource;
	if (NameSelector.IsValid())
	{
		NameSelector->SetSourceArray(InOptionsSource);
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