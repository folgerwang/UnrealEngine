// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigVariableDetailsCustomization.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ControlRig.h"
#include "DetailLayoutBuilder.h"
#include "BlueprintEditorModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Engine/BlueprintGeneratedClass.h"

#define LOCTEXT_NAMESPACE "ControlRigVariableDetailsCustomization"

static FName AnimationOutputMetadataName("AnimationOutput");
static FName AnimationInputMetadataName("AnimationInput");

TSharedPtr<IDetailCustomization> FControlRigVariableDetailsCustomization::MakeInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor)
{
	const TArray<UObject*>* Objects = (InBlueprintEditor.IsValid() ? InBlueprintEditor->GetObjectsCurrentlyBeingEdited() : nullptr);
	if (Objects && Objects->Num() == 1)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>((*Objects)[0]))
		{
			if (Blueprint->ParentClass->IsChildOf(UControlRig::StaticClass()))
			{
				return MakeShareable(new FControlRigVariableDetailsCustomization(InBlueprintEditor, Blueprint));
			}
		}
	}

	return nullptr;
}

void FControlRigVariableDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 0)
	{
		TWeakObjectPtr<UProperty> PropertyBeingCustomized(Cast<UProperty>(ObjectsBeingCustomized[0].Get()));
		if (PropertyBeingCustomized.IsValid())
		{
			const FText AnimationInputText = LOCTEXT("AnimationInput", "Animation Input");
			const FText AnimationOutputText = LOCTEXT("AnimationOutput", "Animation Output");
			const FText AnimationInputTooltipText = LOCTEXT("AnimationInputTooltip", "Whether this variable acts as an input to this animation controller.\nSelecting this allow it to be exposed as an input pin on Evaluation nodes.");
			const FText AnimationOutputTooltipText = LOCTEXT("AnimationOutputTooltip", "Whether this variable acts as an output from this animation controller.\nSelecting this will add a pin to the Animation Output node.");
			
			DetailLayout.EditCategory("Variable")
				.AddCustomRow(AnimationInputText)
				.NameContent()
				[
					SNew(STextBlock)
					.IsEnabled(IsAnimationFlagEnabled(PropertyBeingCustomized))
					.Font(DetailLayout.GetDetailFont())
					.Text(AnimationOutputText)
					.ToolTipText(AnimationOutputTooltipText)
				]
				.ValueContent()
				[
					SNew(SCheckBox)
					.IsEnabled(IsAnimationFlagEnabled(PropertyBeingCustomized))
					.IsChecked_Raw(this, &FControlRigVariableDetailsCustomization::IsAnimationOutputChecked, PropertyBeingCustomized)
					.OnCheckStateChanged_Raw(this, &FControlRigVariableDetailsCustomization::HandleAnimationOutputCheckStateChanged, PropertyBeingCustomized)
					.ToolTipText(AnimationOutputTooltipText)
				];

			DetailLayout.EditCategory("Variable")
				.AddCustomRow(AnimationOutputText)
				.NameContent()
				[
					SNew(STextBlock)
					.IsEnabled(IsAnimationFlagEnabled(PropertyBeingCustomized))
					.Font(DetailLayout.GetDetailFont())
					.Text(AnimationInputText)
					.ToolTipText(AnimationInputTooltipText)
				]
				.ValueContent()
				[
					SNew(SCheckBox)
					.IsEnabled(IsAnimationFlagEnabled(PropertyBeingCustomized))
					.IsChecked_Raw(this, &FControlRigVariableDetailsCustomization::IsAnimationInputChecked, PropertyBeingCustomized)
					.OnCheckStateChanged_Raw(this, &FControlRigVariableDetailsCustomization::HandleAnimationInputCheckStateChanged, PropertyBeingCustomized)
					.ToolTipText(AnimationInputTooltipText)
				];
		}
	}
}

bool FControlRigVariableDetailsCustomization::IsAnimationFlagEnabled(TWeakObjectPtr<UProperty> PropertyBeingCustomized) const
{
	if (PropertyBeingCustomized.IsValid())
	{
		if (UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(PropertyBeingCustomized.Get()->GetOwnerClass()))
		{
			UBlueprint* PropertyOwnerBlueprint = Cast<UBlueprint>(GeneratedClass->ClassGeneratedBy);
			return PropertyOwnerBlueprint == BlueprintPtr.Get();
		}

	}

	return false;
}

ECheckBoxState FControlRigVariableDetailsCustomization::IsAnimationOutputChecked(TWeakObjectPtr<UProperty> PropertyBeingCustomized) const
{
	if (PropertyBeingCustomized.IsValid())
	{
		UProperty* PropertyPtrBeingCustomized = PropertyBeingCustomized.Get();
		FString Value;
		if (PropertyPtrBeingCustomized->HasMetaData(AnimationOutputMetadataName) ||
			FBlueprintEditorUtils::GetBlueprintVariableMetaData(BlueprintPtr.Get(), PropertyPtrBeingCustomized->GetFName(), nullptr, AnimationOutputMetadataName, Value))
		{
			return ECheckBoxState::Checked;
		}
	}

	return ECheckBoxState::Unchecked;
}

void FControlRigVariableDetailsCustomization::HandleAnimationOutputCheckStateChanged(ECheckBoxState CheckBoxState, TWeakObjectPtr<UProperty> PropertyBeingCustomized)
{
	if (CheckBoxState == ECheckBoxState::Checked)
	{
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(BlueprintPtr.Get(), PropertyBeingCustomized->GetFName(), nullptr, AnimationOutputMetadataName, TEXT("true"));
	}
	else
	{
		FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(BlueprintPtr.Get(), PropertyBeingCustomized->GetFName(), nullptr, AnimationOutputMetadataName);
	}
	FBlueprintEditorUtils::ReconstructAllNodes(BlueprintPtr.Get());
}

ECheckBoxState FControlRigVariableDetailsCustomization::IsAnimationInputChecked(TWeakObjectPtr<UProperty> PropertyBeingCustomized) const
{
	if (PropertyBeingCustomized.IsValid())
	{
		UProperty* PropertyPtrBeingCustomized = PropertyBeingCustomized.Get();
		FString Value;
		if (PropertyPtrBeingCustomized->HasMetaData(AnimationInputMetadataName) ||
			FBlueprintEditorUtils::GetBlueprintVariableMetaData(BlueprintPtr.Get(), PropertyPtrBeingCustomized->GetFName(), nullptr, AnimationInputMetadataName, Value))
		{
			return ECheckBoxState::Checked;
		}
	}

	return ECheckBoxState::Unchecked;
}

void FControlRigVariableDetailsCustomization::HandleAnimationInputCheckStateChanged(ECheckBoxState CheckBoxState, TWeakObjectPtr<UProperty> PropertyBeingCustomized)
{
	if (CheckBoxState == ECheckBoxState::Checked)
	{
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(BlueprintPtr.Get(), PropertyBeingCustomized->GetFName(), nullptr, AnimationInputMetadataName, TEXT("true"));
	}
	else
	{
		FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(BlueprintPtr.Get(), PropertyBeingCustomized->GetFName(), nullptr, AnimationInputMetadataName);
	}
	FBlueprintEditorUtils::ReconstructAllNodes(BlueprintPtr.Get());
}

#undef LOCTEXT_NAMESPACE
