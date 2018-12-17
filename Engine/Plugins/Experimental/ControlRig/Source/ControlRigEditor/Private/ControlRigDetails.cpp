// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigDetails.h"
#include "UObject/Class.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "DetailLayoutBuilder.h"
#include "ControlRig.h"
#include "Units/RigUnitEditor_Base.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "ControlRigEditorLibrary.h"

#define LOCTEXT_NAMESPACE "ControlRigDetails"

TSharedRef<IDetailCustomization> FControlRigDetails::MakeInstance()
{
	return MakeShareable( new FControlRigDetails );
}

void FControlRigDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	auto CreateButton = [this](IDetailCategoryBuilder& InEventCategory, URigUnitEditor_Base* EditorClass, const FName& RigUnitPath, const FName& FunctionName)
	{
		// loose binding with RigUnitPath - that way it's safer during recompilation
		const FString RigUnitName = RigUnitPath.ToString();
		const FString DisplayName = EditorClass->GetDisplayName();

		TArray<FStringFormatArg> Arguments;
		Arguments.Add(FStringFormatArg(RigUnitName));
		Arguments.Add(FStringFormatArg(FunctionName.ToString()));

		const FString UniqueFunctionName = FString::Format(TEXT("{0}_{1}"), Arguments);

		Arguments[0] = DisplayName;
		const FString UniqueDisplayFunctionName = FString::Format(TEXT("{0} : {1}"), Arguments);
		const FString ToolTip = EditorClass->GetActionToolTip(FunctionName);

		FDetailWidgetRow& CustomRow = InEventCategory.AddCustomRow(FText::FromString(UniqueFunctionName))
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.Text(FText::FromString(UniqueDisplayFunctionName))
				.ToolTipText(FText::FromString(ToolTip))
				.OnClicked(FOnClicked::CreateSP(this, &FControlRigDetails::TriggerScriptEvent, EditorClass, FunctionName))
			]
		];
	};

	TArray< TWeakObjectPtr<UObject> > SelectedObjectsList = DetailBuilder.GetSelectedObjects();

	// find SelectedControlRig
	for (int32 Idx = 0; Idx < SelectedObjectsList.Num(); ++Idx)
	{
		if (SelectedObjectsList[Idx].Get()->IsA(UControlRig::StaticClass()))
		{
			SelectedControlRig = SelectedObjectsList[Idx];
		}
	}

	// allow if it's only one selected
	if (SelectedControlRig.IsValid())
	{
		UControlRig* ControlRig = Cast<UControlRig>(SelectedControlRig.Get());
		if (ControlRig && !ControlRig->IsTemplate())
		{
			IDetailCategoryBuilder& EventCategory = DetailBuilder.EditCategory("Event");

			UControlRigBlueprintGeneratedClass* GeneratedClass = Cast<UControlRigBlueprintGeneratedClass>(ControlRig->GetClass());
			for (UStructProperty* RigUnitProperty : GeneratedClass->RigUnitProperties)
			{
				FRigUnit* RigUnit = RigUnitProperty->ContainerPtrToValuePtr<FRigUnit>(ControlRig);
				URigUnitEditor_Base* EditorClass = UControlRigEditorLibrary::GetEditorObject(ControlRig, RigUnit->RigUnitName);

				if (EditorClass)
				{
					// generate function list
					TArray<FName> FunctionList;
					EditorClass->GetClass()->GenerateFunctionList(FunctionList);
					if (FunctionList.Num() > 0)
					{
						// create button
						// get display name
						// invoke with Iter.Key(), RigUnit->ActionList
						for (int32 Id = 0; Id < FunctionList.Num(); ++Id)
						{
							if (FunctionList[Id] != NAME_None)
							{
								UFunction* Function = EditorClass->FindFunction(FunctionList[Id]);
								//@fixme : for now we only support 0 param, we should extend it so people can add param later
								if (Function && Function->NumParms == 0)
								{
									CreateButton(EventCategory, EditorClass, RigUnitProperty->GetFName(), FunctionList[Id]);
								}
							}
						}
					}
				}
			}
		}
	}
}

FReply FControlRigDetails::TriggerScriptEvent(URigUnitEditor_Base* EditorClass, FName FunctionName)
{
	if (SelectedControlRig.IsValid())
	{
		UFunction* Function = EditorClass->FindFunction(FunctionName);
		// for now we support 0 params
		if (Function && Function->NumParms == 0)
		{
			EditorClass->ProcessEvent(Function, nullptr);
		}
	}

	return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE
