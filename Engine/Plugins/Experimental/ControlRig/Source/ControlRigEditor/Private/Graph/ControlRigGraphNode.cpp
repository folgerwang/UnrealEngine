// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigGraphNode.h"
#include "ControlRigGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "ControlRig.h"
#include "Textures/SlateIcon.h"
#include "Units/RigUnit.h"
#include "ControlRigGraphSchema.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SControlRig.h"
#include "PropertyPathHelpers.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "ScopedTransaction.h"
#include "StructReference.h"
#include "UObject/PropertyPortFlags.h"
#include "ControlRigBlueprintUtils.h"

#define LOCTEXT_NAMESPACE "ControlRigGraphNode"

FText UControlRigGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if(NodeTitle.IsEmpty() || NodeTitleFull.IsEmpty())
	{
		UScriptStruct* ScriptStruct = GetUnitScriptStruct();
		if(ScriptStruct && ScriptStruct->HasMetaData(UControlRig::DisplayNameMetaName))
		{
			if(ScriptStruct->HasMetaData(UControlRig::ShowVariableNameInTitleMetaName))
			{
				NodeTitleFull = FText::Format(LOCTEXT("NodeFullTitleFormat", "{0}\n{1}"), FText::FromName(PropertyName), FText::FromString(ScriptStruct->GetMetaData(UControlRig::DisplayNameMetaName)));
				NodeTitle = FText::FromName(PropertyName);
			}
			else
			{
				NodeTitle = NodeTitleFull = FText::FromString(ScriptStruct->GetMetaData(UControlRig::DisplayNameMetaName));
			}
		}
		else
		{
			NodeTitle = NodeTitleFull = FText::FromName(PropertyName);
		}
	}

	if(TitleType == ENodeTitleType::FullTitle)
	{
		return NodeTitleFull;
	}
	else
	{
		return NodeTitle;
	}
}

void UControlRigGraphNode::ReconstructNode()
{
	Modify();

	// Clear previously set messages
	ErrorMsg.Reset();

	// @TODO: support pin orphaning/conversions for upgrades/deprecations?

	// Move the existing pins to a saved array
	TArray<UEdGraphPin*> OldPins(Pins);
	Pins.Reset();

	// Recreate the new pins
	ReallocatePinsDuringReconstruction(OldPins);
	RewireOldPinsToNewPins(OldPins, Pins);

	// Let subclasses do any additional work
	PostReconstructNode();

	GetGraph()->NotifyGraphChanged();
}

void UControlRigGraphNode::ReallocatePinsDuringReconstruction(const TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();
}

void UControlRigGraphNode::RewireOldPinsToNewPins(TArray<UEdGraphPin*>& InOldPins, TArray<UEdGraphPin*>& InNewPins)
{
	// @TODO: we should account for redirectors, orphaning etc. here too!

	for(UEdGraphPin* OldPin : InOldPins)
	{
		for(UEdGraphPin* NewPin : InNewPins)
		{
			if(OldPin->PinName == NewPin->PinName && OldPin->PinType == NewPin->PinType && OldPin->Direction == NewPin->Direction)
			{
				NewPin->MovePersistentDataFromOldPin(*OldPin);
				break;
			}
		}
	}

	DestroyPinList(InOldPins);
}

void UControlRigGraphNode::DestroyPinList(TArray<UEdGraphPin*>& InPins)
{
	UBlueprint* Blueprint = GetBlueprint();
	// Throw away the original pins
	for (UEdGraphPin* Pin : InPins)
	{
		Pin->Modify();
		Pin->BreakAllPinLinks(!Blueprint->bIsRegeneratingOnLoad);

		UEdGraphNode::DestroyPin(Pin);
	}
}

void UControlRigGraphNode::PostReconstructNode()
{
	for (UEdGraphPin* Pin : Pins)
	{
		SetupPinDefaultsFromCDO(Pin);
	}

	UScriptStruct* ScriptStruct = GetUnitScriptStruct();
	bCanRenameNode = (ScriptStruct == nullptr) || (ScriptStruct && ScriptStruct->HasMetaData(UControlRig::DisplayNameMetaName) && ScriptStruct->HasMetaData(UControlRig::ShowVariableNameInTitleMetaName));
}

void UControlRigGraphNode::HandleVariableRenamed(UBlueprint* InBlueprint, UClass* InVariableClass, UEdGraph* InGraph, const FName& InOldVarName, const FName& InNewVarName)
{
	if(InBlueprint == GetBlueprint() && InGraph == GetGraph())
	{
		if(InOldVarName == PropertyName)
		{
			Modify();

			PropertyName = InNewVarName;
			InvalidateNodeTitle();

			for(UEdGraphPin* Pin : Pins)
			{
				FString OldPinName = Pin->PinName.ToString();
				bool bRemoved = OldPinName.RemoveFromStart(InOldVarName.ToString());
				check(bRemoved);
				Pin->PinName = FName(*FString(InNewVarName.ToString() + OldPinName));
			}
		}
	}
}

void UControlRigGraphNode::AllocateDefaultPins()
{
	CacheVariableInfo();
	CreateInputPins();
	CreateInputOutputPins();
	CreateOutputPins();
}

/** Helper function to check whether this is a struct reference pin */
static bool IsStructReference(const TSharedPtr<FControlRigField>& InputInfo)
{
	if(UStructProperty* StructProperty = Cast<UStructProperty>(InputInfo->GetField()))
	{
		return StructProperty->Struct->IsChildOf(FStructReference::StaticStruct());
	}

	return false;
}

void UControlRigGraphNode::CreateInputPins_Recursive(const TSharedPtr<FControlRigField>& InputInfo)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	for (const TSharedPtr<FControlRigField>& ChildInfo : InputInfo->Children)
	{
		ChildInfo->InputPin = CreatePin(EGPD_Input, ChildInfo->GetPinType(), FName(*ChildInfo->GetPropertyPath()));
		ChildInfo->InputPin->PinFriendlyName = ChildInfo->GetDisplayNameText();
		ChildInfo->InputPin->PinType.bIsReference = IsStructReference(ChildInfo);
		ChildInfo->InputPin->ParentPin = InputInfo->InputPin;
		InputInfo->InputPin->SubPins.Add(ChildInfo->InputPin);
		SetupPinAutoGeneratedDefaults(ChildInfo->InputPin);
	}

	for (const TSharedPtr<FControlRigField>& ChildInfo : InputInfo->Children)
	{
		CreateInputPins_Recursive(ChildInfo);
	}
}

void UControlRigGraphNode::CreateInputPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	const TArray<TSharedRef<FControlRigField>>& LocalInputInfos = GetInputVariableInfo();

	for (const TSharedRef<FControlRigField>& InputInfo : LocalInputInfos)
	{
		InputInfo->InputPin = CreatePin(EGPD_Input, InputInfo->GetPinType(), FName(*InputInfo->GetPropertyPath()));
		InputInfo->InputPin->PinFriendlyName = InputInfo->GetDisplayNameText();
		InputInfo->InputPin->PinType.bIsReference = IsStructReference(InputInfo);
		SetupPinAutoGeneratedDefaults(InputInfo->InputPin);

		CreateInputPins_Recursive(InputInfo);
	}
}

void UControlRigGraphNode::CreateInputOutputPins_Recursive(const TSharedPtr<FControlRigField>& InputOutputInfo)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	for (const TSharedPtr<FControlRigField>& ChildInfo : InputOutputInfo->Children)
	{
		ChildInfo->InputPin = CreatePin(EGPD_Input, ChildInfo->GetPinType(), FName(*ChildInfo->GetPropertyPath()));
		ChildInfo->InputPin->PinFriendlyName = ChildInfo->GetDisplayNameText();
		ChildInfo->InputPin->PinType.bIsReference = IsStructReference(ChildInfo);
		ChildInfo->InputPin->ParentPin = InputOutputInfo->InputPin;
		InputOutputInfo->InputPin->SubPins.Add(ChildInfo->InputPin);
		SetupPinAutoGeneratedDefaults(ChildInfo->InputPin);

		ChildInfo->OutputPin = CreatePin(EGPD_Output, ChildInfo->GetPinType(), FName(*ChildInfo->GetPropertyPath()));
		ChildInfo->OutputPin->PinFriendlyName = ChildInfo->GetDisplayNameText();
		ChildInfo->OutputPin->ParentPin = InputOutputInfo->OutputPin;
		ChildInfo->OutputPin->PinType.bIsReference = IsStructReference(ChildInfo);
		InputOutputInfo->OutputPin->SubPins.Add(ChildInfo->OutputPin);
	}

	for (const TSharedPtr<FControlRigField>& ChildInfo : InputOutputInfo->Children)
	{
		CreateInputOutputPins_Recursive(ChildInfo);
	}
}

void UControlRigGraphNode::CreateInputOutputPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	const TArray<TSharedRef<FControlRigField>>& LocalInputOutputInfos = GetInputOutputVariableInfo();

	for (const TSharedRef<FControlRigField>& InputOutputInfo : LocalInputOutputInfos)
	{
		InputOutputInfo->InputPin = CreatePin(EGPD_Input, InputOutputInfo->GetPinType(), FName(*InputOutputInfo->GetPropertyPath()));
		InputOutputInfo->InputPin->PinFriendlyName = InputOutputInfo->GetDisplayNameText();
		InputOutputInfo->InputPin->PinType.bIsReference = IsStructReference(InputOutputInfo);
		SetupPinAutoGeneratedDefaults(InputOutputInfo->InputPin);

		InputOutputInfo->OutputPin = CreatePin(EGPD_Output, InputOutputInfo->GetPinType(), FName(*InputOutputInfo->GetPropertyPath()));

		CreateInputOutputPins_Recursive(InputOutputInfo);
	}
}

void UControlRigGraphNode::CreateOutputPins_Recursive(const TSharedPtr<FControlRigField>& OutputInfo)
{
	for (const TSharedPtr<FControlRigField>& ChildInfo : OutputInfo->Children)
	{
		ChildInfo->OutputPin = CreatePin(EGPD_Output, ChildInfo->GetPinType(), FName(*ChildInfo->GetPropertyPath()));
		ChildInfo->OutputPin->PinFriendlyName = ChildInfo->GetDisplayNameText();
		ChildInfo->OutputPin->PinType.bIsReference = IsStructReference(ChildInfo);
		ChildInfo->OutputPin->ParentPin = OutputInfo->OutputPin;
		OutputInfo->OutputPin->SubPins.Add(ChildInfo->OutputPin);
	}

	for (const TSharedPtr<FControlRigField>& ChildInfo : OutputInfo->Children)
	{
		CreateOutputPins_Recursive(ChildInfo);
	}
}

void UControlRigGraphNode::CreateOutputPins()
{
	const TArray<TSharedRef<FControlRigField>>& LocalOutputInfos = GetOutputVariableInfo();

	for (const TSharedRef<FControlRigField>& OutputInfo : LocalOutputInfos)
	{
		OutputInfo->OutputPin = CreatePin(EGPD_Output, OutputInfo->GetPinType(), FName(*OutputInfo->GetPropertyPath()));
		OutputInfo->OutputPin->PinFriendlyName = OutputInfo->GetDisplayNameText();
		OutputInfo->OutputPin->PinType.bIsReference = IsStructReference(OutputInfo);
		CreateOutputPins_Recursive(OutputInfo);
	}
}

void UControlRigGraphNode::CacheVariableInfo()
{
	InputInfos.Reset();
	GetInputFields(InputInfos);

	OutputInfos.Reset();
	GetOutputFields(OutputInfos);

	InputOutputInfos.Reset();
	GetInputOutputFields(InputOutputInfos);
}

UClass* UControlRigGraphNode::GetControlRigGeneratedClass() const
{
	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()->GetOuter());
	if(Blueprint)
	{
		if (Blueprint->GeneratedClass)
		{
			check(Blueprint->GeneratedClass->IsChildOf(UControlRig::StaticClass()));
			return Blueprint->GeneratedClass;
		}
	}

	return nullptr;
}

UClass* UControlRigGraphNode::GetControlRigSkeletonGeneratedClass() const
{
	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()->GetOuter());
	if(Blueprint)
	{
		if (Blueprint->SkeletonGeneratedClass)
		{
			check(Blueprint->SkeletonGeneratedClass->IsChildOf(UControlRig::StaticClass()));
			return Blueprint->SkeletonGeneratedClass;
		}
	}
	return nullptr;
}

FSlateIcon UControlRigGraphNode::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = GetNodeTitleColor();
	static FSlateIcon Icon("EditorStyle", "Kismet.AllClasses.FunctionIcon");
	return Icon;
}

// void UControlRigGraphNode::HandleVariableRenamed(UBlueprint* InBlueprint, UClass* InVariableClass, UEdGraph* InGraph, const FName& InOldVarName, const FName& InNewVarName)
// {
// 	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
// 
// 	Modify();
// 
// 	// first rename any disabled inputs/outputs
// 	for (FName& DisabledInput : DisabledInputs)
// 	{
// 		if (DisabledInput == InOldVarName)
// 		{
// 			DisabledInput = InNewVarName;
// 		}
// 	}
// 
// 	for (FName& DisabledOutput : DisabledOutputs)
// 	{
// 		if (DisabledOutput == InOldVarName)
// 		{
// 			DisabledOutput = InNewVarName;
// 		}
// 	}
// 
// 	RenameUserDefinedPin(InOldVarName.ToString(), InNewVarName.ToString());
// }

// bool UControlRigGraphNode::ReferencesVariable(const FName& InVarName, const UStruct* InScope) const
// {
// 	return InVarName == PropertyName;
// }

TSharedPtr<FControlRigField> UControlRigGraphNode::CreateControlRigField(UField* Field, const FString& PropertyPath, int32 InArrayIndex) const
{
	if (UProperty* Property = Cast<UProperty>(Field))
	{
		TSharedPtr<FControlRigField> NewField = MakeShareable(new FControlRigProperty(Property, PropertyPath, InArrayIndex));
		NewField->TooltipText = Field->GetToolTipText();
		NewField->InputPin = FindPin(PropertyPath, EGPD_Input);
		NewField->OutputPin = FindPin(PropertyPath, EGPD_Output);
		return NewField;
	}

	return nullptr;
}

/** Helper function used to prevent us from creating sub-pins for certain field types we want to be 'atomic' */
static bool CanExpandPinsForField(UField* InField)
{
	if(UStructProperty* StructProperty = Cast<UStructProperty>(InField))
	{
		if(StructProperty->Struct == TBaseStructure<FQuat>::Get())
		{
			return false;
		}
	}

	return true;
}

void UControlRigGraphNode::GetInputFields(TArray<TSharedRef<FControlRigField>>& OutFields) const
{
	GetFields([](UProperty* InProperty){ return InProperty->HasMetaData(UControlRig::InputMetaName) && !InProperty->HasMetaData(UControlRig::OutputMetaName); }, OutFields);
}

void UControlRigGraphNode::GetOutputFields(TArray<TSharedRef<FControlRigField>>& OutFields) const
{
	GetFields([](UProperty* InProperty){ return InProperty->HasMetaData(UControlRig::OutputMetaName) && !InProperty->HasMetaData(UControlRig::InputMetaName); }, OutFields);
}

void UControlRigGraphNode::GetInputOutputFields(TArray<TSharedRef<FControlRigField>>& OutFields) const
{
	GetFields([](UProperty* InProperty){ return InProperty->HasMetaData(UControlRig::InputMetaName) && InProperty->HasMetaData(UControlRig::OutputMetaName); }, OutFields);
	
	// Handle properties as in-outs
	if (UClass* MyControlRigClass = GetControlRigSkeletonGeneratedClass())
	{
		UScriptStruct* ScriptStruct = GetUnitScriptStruct();
		if(ScriptStruct == nullptr)
		{
			if(UProperty* Property = MyControlRigClass->FindPropertyByName(PropertyName))
			{
				// We don't care here whether we are dealing with input/output fields as we want a pin to be created for both
				FString PropertyPath = PropertyName.ToString();
				TSharedPtr<FControlRigField> ControlRigField = CreateControlRigField(Property, PropertyPath);
				if(ControlRigField.IsValid())
				{
					OutFields.Add(ControlRigField.ToSharedRef());
					GetFields_Recursive(ControlRigField.ToSharedRef(), PropertyPath);
				}
			}
		}
	}
}

static const FString PropertyPathDelimiter(TEXT("."));

void UControlRigGraphNode::GetFields(TFunction<bool(UProperty*)> InPropertyCheckFunction, TArray<TSharedRef<FControlRigField>>& OutFields) const
{
	OutFields.Reset();

	if(UScriptStruct* ScriptStruct = GetUnitScriptStruct())
	{
		for (TFieldIterator<UProperty> PropertyIt(ScriptStruct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			if (InPropertyCheckFunction(Property))
			{
				FString PropertyPath = PropertyName.ToString() + PropertyPathDelimiter + Property->GetName();
				TSharedPtr<FControlRigField> ControlRigField = CreateControlRigField(Property, PropertyPath);
				if(ControlRigField.IsValid())
				{
					OutFields.Add(ControlRigField.ToSharedRef());
					GetFields_RecursiveHelper(Property, ControlRigField.ToSharedRef(), PropertyPath);
				}
			}
		}
	}
}

void UControlRigGraphNode::GetFields_RecursiveHelper(UProperty* InProperty, const TSharedRef<FControlRigField>& InControlRigField, const FString& InPropertyPath) const
{
	if(UArrayProperty* ArrayProperty = Cast<UArrayProperty>(InProperty))
	{
		// if this is an array property, add sub-fields for each element
		// Note we can only do this for nodes that are present in the CDO
		int32 ElementCount = 0;
		PerformArrayOperation(InPropertyPath, [&ElementCount](FScriptArrayHelper& InArrayHelper, int32 InArrayIndex)
		{
			ElementCount = InArrayHelper.Num();
			return true;
		}, false, false);

		for(int32 ElementIndex = 0; ElementIndex < ElementCount; ++ElementIndex)
		{
			FString SubPropertyPath = InPropertyPath + FString::Printf(TEXT("[%d]"), ElementIndex);
			TSharedPtr<FControlRigField> ControlRigSubField = CreateControlRigField(ArrayProperty->Inner, SubPropertyPath, ElementIndex);
			if(ControlRigSubField.IsValid())
			{
				InControlRigField->Children.Add(ControlRigSubField.ToSharedRef());
				GetFields_Recursive(ControlRigSubField.ToSharedRef(), SubPropertyPath);
			}
		}
	}
	else
	{
		GetFields_Recursive(InControlRigField, InPropertyPath);
	}
}

void UControlRigGraphNode::GetFields_Recursive(const TSharedRef<FControlRigField>& ParentControlRigField, const FString& ParentPropertyPath) const
{
	if(CanExpandPinsForField(ParentControlRigField->GetField()))
	{
		if(UStructProperty* StructProperty = Cast<UStructProperty>(ParentControlRigField->GetField()))
		{
			for (TFieldIterator<UProperty> PropertyIt(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
			{
				UProperty* Property = *PropertyIt;
				FString PropertyPath = ParentPropertyPath + PropertyPathDelimiter + PropertyIt->GetName();
				TSharedPtr<FControlRigField> ControlRigField = CreateControlRigField(*PropertyIt, PropertyPath);
				if(ControlRigField.IsValid())
				{
					ParentControlRigField->Children.Add(ControlRigField.ToSharedRef());
					GetFields_RecursiveHelper(Property, ControlRigField.ToSharedRef(), PropertyPath);
				}
			}
		}
	}
}

UStructProperty* UControlRigGraphNode::GetUnitProperty() const
{
	UProperty* ClassProperty = GetProperty();
	if(ClassProperty)
	{
		// Check if this is a unit struct and if so extract the pins we want to display...
		if(UStructProperty* StructProperty = Cast<UStructProperty>(ClassProperty))
		{
			if(StructProperty->Struct->IsChildOf(FRigUnit::StaticStruct()))
			{
				return StructProperty;
			}
		}
	}

	return nullptr;
}

UScriptStruct* UControlRigGraphNode::GetUnitScriptStruct() const
{
	if(UStructProperty* StructProperty = GetUnitProperty())
	{
		if(StructProperty->Struct->IsChildOf(FRigUnit::StaticStruct()))
		{
			return StructProperty->Struct;
		}
	}
	else 
	{
		// Assume that the property name we have is the name of the struct type
		UScriptStruct* Struct = FindObject<UScriptStruct>(ANY_PACKAGE, *PropertyName.ToString());
		if(Struct && Struct->IsChildOf(FRigUnit::StaticStruct()))
		{
			return Struct;
		}
	}
	return nullptr;
}

UProperty* UControlRigGraphNode::GetProperty() const
{
	if (UClass* MyControlRigClass = GetControlRigSkeletonGeneratedClass())
	{
		return MyControlRigClass->FindPropertyByName(PropertyName);
	}
	return nullptr;
}

void UControlRigGraphNode::PinConnectionListChanged(UEdGraphPin* Pin) 
{

}

void UControlRigGraphNode::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if(Context.MenuBuilder != nullptr)
	{
		if(Context.Pin != nullptr)
		{
			// Add array operations for array pins
			if(Context.Pin->PinType.IsArray())
			{
				// End the section as this function is called with a section 'open'
				Context.MenuBuilder->EndSection();

				Context.MenuBuilder->BeginSection(TEXT("ArrayOperations"), LOCTEXT("ArrayOperations", "Array Operations"));

				// Array operations
				Context.MenuBuilder->AddMenuEntry(
					LOCTEXT("ClearArray", "Clear"),
					LOCTEXT("ClearArray_Tooltip", "Clear this array of all of its entries"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateUObject(this, &UControlRigGraphNode::HandleClearArray, Context.Pin->PinName.ToString())));

				Context.MenuBuilder->EndSection();
			}
			else if(Context.Pin->ParentPin != nullptr && Context.Pin->ParentPin->PinType.IsArray())
			{
				// End the section as this function is called with a section 'open'
				Context.MenuBuilder->EndSection();

				Context.MenuBuilder->BeginSection(TEXT("ArrayElementOperations"), LOCTEXT("ArrayElementOperations", "Array Element Operations"));

				// Array element operations
				Context.MenuBuilder->AddMenuEntry(
					LOCTEXT("RemoveArrayElement", "Remove"),
					LOCTEXT("RemoveArrayElement_Tooltip", "Remove this array element"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateUObject(this, &UControlRigGraphNode::HandleRemoveArrayElement, Context.Pin->PinName.ToString())));

				Context.MenuBuilder->AddMenuEntry(
					LOCTEXT("InsertArrayElement", "Insert"),
					LOCTEXT("InsertArrayElement_Tooltip", "Insert an array element after this one"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateUObject(this, &UControlRigGraphNode::HandleInsertArrayElement, Context.Pin->PinName.ToString())));

				Context.MenuBuilder->EndSection();
			}
		}
	}
}

void UControlRigGraphNode::SetPinExpansion(const FString& InPinPropertyPath, bool bExpanded)
{
	if(bExpanded)
	{
		ExpandedPins.AddUnique(InPinPropertyPath);
	}
	else
	{
		ExpandedPins.Remove(InPinPropertyPath);
	}
}

bool UControlRigGraphNode::IsPinExpanded(const FString& InPinPropertyPath) const
{
	return ExpandedPins.Contains(InPinPropertyPath);
}

void UControlRigGraphNode::DestroyNode()
{
	if(UControlRigGraph* Graph = Cast<UControlRigGraph>(GetOuter()))
	{
		UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Graph->GetOuter());
		if(ControlRigBlueprint)
		{
			ControlRigBlueprint->Modify();

			BreakAllNodeLinks();

			FControlRigBlueprintUtils::RemoveMemberVariableIfNotUsed(ControlRigBlueprint, PropertyName, this);
		}
	}

	UEdGraphNode::DestroyNode();
}

void UControlRigGraphNode::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	CopyPinDefaultsToProperties(Pin, true, true);
}

TSharedPtr<INameValidatorInterface> UControlRigGraphNode::MakeNameValidator() const
{
	return MakeShared<FKismetNameValidator>(GetBlueprint(), PropertyName);
}

void UControlRigGraphNode::CopyPinDefaultsToProperties(UEdGraphPin* Pin, bool bCallModify, bool bPropagateToInstances)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if(UControlRigGraph* Graph = Cast<UControlRigGraph>(GetOuter()))
	{
		UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Graph->GetOuter());
		if(ControlRigBlueprint)
		{
			// Note we need the actual generated class here
			if (UClass* MyControlRigClass = GetControlRigGeneratedClass())
			{
				if(UObject* DefaultObject = MyControlRigClass->GetDefaultObject(false))
				{
					if(bCallModify)
					{
						DefaultObject->SetFlags(RF_Transactional);
						DefaultObject->Modify();
					}

					FString DefaultValueString = Pin->GetDefaultAsString();
					if(DefaultValueString.Len() > 0)
					{
						FCachedPropertyPath PropertyPath(Pin->PinName.ToString());
						if(PropertyPathHelpers::SetPropertyValueFromString(DefaultObject, PropertyPath, DefaultValueString))
						{
							if(bCallModify)
							{
								FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
							}
						}

						if(bPropagateToInstances)
						{
							TArray<UObject*> ArchetypeInstances;
							DefaultObject->GetArchetypeInstances(ArchetypeInstances);
							
							for (UObject* ArchetypeInstance : ArchetypeInstances)
							{
								PropertyPathHelpers::SetPropertyValueFromString(ArchetypeInstance, PropertyPath, DefaultValueString);
							}
						}
					}
				}
			}
		}
	}
}

UControlRigBlueprint* UControlRigGraphNode::GetBlueprint() const
{
	if(UControlRigGraph* Graph = Cast<UControlRigGraph>(GetOuter()))
	{
		return Cast<UControlRigBlueprint>(Graph->GetOuter());
	}
	return nullptr;
}

void UControlRigGraphNode::SetupPinAutoGeneratedDefaults(UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if(UControlRigGraph* Graph = Cast<UControlRigGraph>(GetOuter()))
	{
		UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Graph->GetOuter());
		if(ControlRigBlueprint)
		{
			FString DefaultValueString;
			FCachedPropertyPath PropertyPath(Pin->PinName.ToString());
				
			// If GetPropertyValueAsString fails then the generated class doesn't have the property yet (likely just the skeleton class has been compiled)
			UScriptStruct* ScriptStruct = GetUnitScriptStruct();
			UStructProperty* UnitStructProperty = GetUnitProperty();
			if(ScriptStruct && UnitStructProperty)
			{
				TArray<uint8> TempBuffer;
				TempBuffer.AddUninitialized(UnitStructProperty->ElementSize);
				ScriptStruct->InitializeDefaultValue(TempBuffer.GetData());
							
				// Trim the property path to look at the members of this struct
				PropertyPath.RemoveFromStart();
				PropertyPathHelpers::GetPropertyValueAsString(TempBuffer.GetData(), ScriptStruct, PropertyPath, DefaultValueString);
								
				K2Schema->GetPinDefaultValuesFromString(Pin->PinType, Pin->GetOwningNodeUnchecked(), DefaultValueString, Pin->AutogeneratedDefaultValue, Pin->DefaultObject, Pin->DefaultTextValue);
				Pin->DefaultValue = Pin->AutogeneratedDefaultValue;
			}
			else if(UProperty* Property = GetProperty())
			{
				if(UStructProperty* StructProperty = Cast<UStructProperty>(Property))
				{
					TArray<uint8> TempBuffer;
					TempBuffer.AddUninitialized(StructProperty->ElementSize);
					StructProperty->Struct->InitializeDefaultValue(TempBuffer.GetData());
								
					// Fill in the root defaults from the struct itself
					if(Pin->ParentPin == nullptr)
					{
						Property->ExportTextItem(DefaultValueString, TempBuffer.GetData(), nullptr, nullptr, PPF_None);
					}
					else
					{
						// Trim the property path to look at the members of this struct
						PropertyPath.RemoveFromStart();

						PropertyPathHelpers::GetPropertyValueAsString(TempBuffer.GetData(), StructProperty->Struct, PropertyPath, DefaultValueString);
					}

					K2Schema->GetPinDefaultValuesFromString(Pin->PinType, Pin->GetOwningNodeUnchecked(), DefaultValueString, Pin->AutogeneratedDefaultValue, Pin->DefaultObject, Pin->DefaultTextValue);
					Pin->DefaultValue = Pin->AutogeneratedDefaultValue;
				}
				else
				{
					// Plain ol' properties are simpler to set up
					TArray<uint8> TempBuffer;
					TempBuffer.AddUninitialized(Property->ElementSize);
					Property->InitializeValue(TempBuffer.GetData());

					Property->ExportTextItem(DefaultValueString, TempBuffer.GetData(), nullptr, nullptr, PPF_None);

					K2Schema->GetPinDefaultValuesFromString(Pin->PinType, Pin->GetOwningNodeUnchecked(), DefaultValueString, Pin->AutogeneratedDefaultValue, Pin->DefaultObject, Pin->DefaultTextValue);
					Pin->DefaultValue = Pin->AutogeneratedDefaultValue;
				}
			}
		}
	}
}

void UControlRigGraphNode::SetupPinDefaultsFromCDO(UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if(UControlRigGraph* Graph = Cast<UControlRigGraph>(GetOuter()))
	{
		UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Graph->GetOuter());
		if(ControlRigBlueprint)
		{
			// Note we need the actual generated class here
			if (UClass* MyControlRigClass = GetControlRigGeneratedClass())
			{
				if(UObject* DefaultObject = MyControlRigClass->GetDefaultObject(false))
				{
					FString DefaultValueString;
					FCachedPropertyPath PropertyPath(Pin->PinName.ToString());
					if(PropertyPathHelpers::GetPropertyValueAsString(DefaultObject, PropertyPath, DefaultValueString))
					{
						K2Schema->GetPinDefaultValuesFromString(Pin->PinType, Pin->GetOwningNodeUnchecked(), DefaultValueString, Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue);
					}
				}
			}
		}
	}
}

bool UControlRigGraphNode::PerformArrayOperation(const FString& InPropertyPath, TFunctionRef<bool(FScriptArrayHelper&,int32)> InOperation, bool bCallModify, bool bPropagateToInstances) const
{
	if(UStructProperty* StructProperty = GetUnitProperty())
	{
		if (UClass* MyControlRigClass = GetControlRigGeneratedClass())
		{
			if(UObject* DefaultObject = MyControlRigClass->GetDefaultObject(false))
			{
				if(bCallModify)
				{
					DefaultObject->SetFlags(RF_Transactional);
					DefaultObject->Modify();
				}

				FCachedPropertyPath CachedPropertyPath(InPropertyPath);
				if(PropertyPathHelpers::PerformArrayOperation(DefaultObject, CachedPropertyPath, InOperation))
				{
					if(bCallModify)
					{
						FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());

						if(bPropagateToInstances)
						{
							TArray<UObject*> ArchetypeInstances;
							DefaultObject->GetArchetypeInstances(ArchetypeInstances);
							
							for (UObject* ArchetypeInstance : ArchetypeInstances)
							{
								PropertyPathHelpers::PerformArrayOperation(ArchetypeInstance, CachedPropertyPath, InOperation);
							}
						}
					}
					return true;
				}
			}
		}
	}

	return false;
}

void UControlRigGraphNode::OnRenameNode(const FString& InNewName)
{
	FBlueprintEditorUtils::RenameMemberVariable(GetBlueprint(), PropertyName, *InNewName);
	PropertyName = *InNewName;
	InvalidateNodeTitle();
	FBlueprintEditorUtils::ReconstructAllNodes(GetBlueprint());
}

FText UControlRigGraphNode::GetTooltipText() const
{
	if(GetUnitScriptStruct())
	{
		return GetUnitScriptStruct()->GetToolTipText();
	}
	else if(GetUnitProperty())
	{
		return GetUnitProperty()->GetToolTipText();
	}

	return FText::FromName(PropertyName);
}

void UControlRigGraphNode::InvalidateNodeTitle() const
{
	NodeTitleFull = FText();
	NodeTitle = FText();
}

bool UControlRigGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* InSchema) const
{
	return InSchema->IsA<UControlRigGraphSchema>();
}

void UControlRigGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	const UControlRigGraphSchema* Schema = GetDefault<UControlRigGraphSchema>();

	for(UEdGraphPin* Pin : Pins)
	{
		FControlRigPinConnectionResponse ConnectResponse = Schema->CanCreateConnection_Extended(FromPin, Pin);
		if(ConnectResponse.Response.Response != ECanCreateConnectionResponse::CONNECT_RESPONSE_DISALLOW)
		{
			if(Schema->TryCreateConnection(FromPin, Pin))
			{
				// expand any sub-pins so the connection is visible
				if(UControlRigGraphNode* OuterNode = Cast<UControlRigGraphNode>(Pin->GetOwningNode()))
				{
					UEdGraphPin* ParentPin = Pin->ParentPin;
					while(ParentPin != nullptr)
					{
						OuterNode->SetPinExpansion(ParentPin->PinName.ToString(), true);
						ParentPin = ParentPin->ParentPin;
					}
				}
				return;
			}
		}
	}
}

void UControlRigGraphNode::HandleAddArrayElement(FString InPropertyPath)
{
	const FScopedTransaction Transaction(LOCTEXT("AddArrayElement", "Add Array Element"));

	if(PerformArrayOperation(InPropertyPath, [](FScriptArrayHelper& InArrayHelper, int32 InArrayIndex)
	{
		InArrayHelper.AddValues(1);
		return true;
	}, true, true))
	{
		ReconstructNode();
	}
}

void UControlRigGraphNode::HandleClearArray(FString InPropertyPath)
{
	const FScopedTransaction Transaction(LOCTEXT("ClearArray", "Clear Array"));

	if(PerformArrayOperation(InPropertyPath, [](FScriptArrayHelper& InArrayHelper, int32 InArrayIndex)
	{
		InArrayHelper.EmptyValues();
		return true;
	}, true, true))
	{
		ReconstructNode();
	}
}

void UControlRigGraphNode::HandleRemoveArrayElement(FString InPropertyPath)
{
	const FScopedTransaction Transaction(LOCTEXT("RemoveArrayElement", "Remove Array Element"));

	if(PerformArrayOperation(InPropertyPath, [](FScriptArrayHelper& InArrayHelper, int32 InArrayIndex)
	{
		if(InArrayIndex != INDEX_NONE)
		{
			InArrayHelper.RemoveValues(InArrayIndex);
			return true;
		}
		return false;
	}, true, true))
	{
		ReconstructNode();
	}
}

void UControlRigGraphNode::HandleInsertArrayElement(FString InPropertyPath)
{
	const FScopedTransaction Transaction(LOCTEXT("InsertArrayElement", "Insert Array Element"));

	if(PerformArrayOperation(InPropertyPath, [](FScriptArrayHelper& InArrayHelper, int32 InArrayIndex)
	{
		if(InArrayIndex != INDEX_NONE)
		{
			InArrayHelper.InsertValues(InArrayIndex);
			return true;
		}
		return false;
	}, true, true))
	{
		ReconstructNode();
	}
}

void ReplacePropertyName(TArray<TSharedRef<FControlRigField>>& InArray, const FString& OldPropName, const FString& NewPropName)
{
	for (int32 Index = 0; Index < InArray.Num(); ++Index)
	{
		InArray[Index]->PropertyPath = InArray[Index]->PropertyPath.Replace(*OldPropName, *NewPropName);

		ReplacePropertyName(InArray[Index]->Children, OldPropName, NewPropName);
	}
};

void UControlRigGraphNode::SetPropertyName(const FName& InPropertyName, bool bReplaceInnerProperties/*=false*/)
{ 
	const FString OldPropertyName = PropertyName.ToString();
	const FString NewPropertyName = InPropertyName.ToString();
	PropertyName = InPropertyName;

	if (bReplaceInnerProperties && InPropertyName != NAME_None)
	{
		ReplacePropertyName(InputInfos, OldPropertyName, NewPropertyName);
		ReplacePropertyName(InputOutputInfos, OldPropertyName, NewPropertyName);
		ReplacePropertyName(OutputInfos, OldPropertyName, NewPropertyName);

		// now change pins
		for (int32 Index = 0; Index < Pins.Num(); ++Index)
		{
			FString PinString = Pins[Index]->PinName.ToString();
			Pins[Index]->PinName = FName(*PinString.Replace(*OldPropertyName, *NewPropertyName));
		}
	}
}
#undef LOCTEXT_NAMESPACE
