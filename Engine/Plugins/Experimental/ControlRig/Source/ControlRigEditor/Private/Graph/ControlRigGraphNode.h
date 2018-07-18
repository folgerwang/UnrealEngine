// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node.h"
#include "UObject/SoftObjectPath.h"
#include "EdGraphSchema_K2.h"
#include "ControlRigGraphNode.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;
struct FSlateIcon;
class UControlRigBlueprint;

/** Information about a control rig field */
class FControlRigField
{
public:
	FControlRigField(const FEdGraphPinType& InPinType, const FString& InPropertyPath, const FText& InDisplayNameText, int32 InArrayIndex = INDEX_NONE)
		: InputPin(nullptr)
		, OutputPin(nullptr)
		, PinType(InPinType)
		, PropertyPath(InPropertyPath)
		, DisplayNameText(InDisplayNameText)
		, ArrayIndex(InArrayIndex)
	{
	}

	virtual ~FControlRigField() {}

	/** Get the field we refer to */
	virtual UField* GetField() const { return nullptr; }

	/** Get the input pin for this item */
	virtual UEdGraphPin* GetPin() const { return InputPin; }

	/** Get the output pin for this item */
	virtual UEdGraphPin* GetOutputPin() const { return OutputPin; }

	/** Get the name of this field */
	virtual FString GetPropertyPath() const { return PropertyPath; }

	/** Get the name to display for this field */
	virtual FText GetDisplayNameText() const { return DisplayNameText; }

	/** Get the tooltip to display for this field */
	virtual FText GetTooltipText() const { return TooltipText; }

	/** Get the pin type to use for this field */
	virtual FEdGraphPinType GetPinType() const { return PinType; }

	/** Cached input pin */
	UEdGraphPin* InputPin;

	/** Cached output pin */
	UEdGraphPin* OutputPin;

	/** Pin type we use for the field */
	FEdGraphPinType PinType;

	/** Cached name to display */
	FString PropertyPath;

	/** The name to display for this field  */
	FText DisplayNameText;

	/** The name to display for this field's tooltip  */
	FText TooltipText;

	/** The array index, or INDEX_NONE if this is not an array-indexed property */
	int32 ArrayIndex;

	/** Any sub-fields are represented by children of this field */
	TArray<TSharedRef<FControlRigField>> Children;
};

/** Information about an input/output property */
class FControlRigProperty : public FControlRigField
{
private:
	static FEdGraphPinType GetPinTypeFromProperty(UProperty* InProperty)
	{
		FEdGraphPinType OutPinType; 
		GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(InProperty, OutPinType);

		return OutPinType;
	}

	static FText GetDisplayNameForProperty(UProperty* InProperty, int32 InArrayIndex)
	{
		if(InArrayIndex != INDEX_NONE)
		{
			return FText::Format(NSLOCTEXT("ControlRigGraphNode", "ArrayPinFormat", "[{0}]"), FText::AsNumber(InArrayIndex));
		}

		return InProperty->GetDisplayNameText();
	}

public:
	FControlRigProperty(UProperty* InProperty, const FString& InPropertyPath, int32 InArrayIndex = INDEX_NONE)
		: FControlRigField(GetPinTypeFromProperty(InProperty), *InPropertyPath, GetDisplayNameForProperty(InProperty, InArrayIndex), InArrayIndex)
		, Property(InProperty)
	{
	}

	virtual UField* GetField() const { return Property; }

private:
	/** The field that we use as input/output */
	UProperty* Property;
};

/** Base class for animation ControlRig-related nodes */
UCLASS()
class UControlRigGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

	friend class FControlRigGraphNodeDetailsCustomization;

private:
	/** The property we represent. For template nodes this represents the struct/property type name. */
	UPROPERTY()
	FName PropertyName;

	/** Expanded pins */
	UPROPERTY()
	TArray<FString> ExpandedPins;

	/** Cached dimensions of this node (used for auto-layout) */
	FVector2D Dimensions;

	/** The cached node titles */
	mutable FText NodeTitleFull;
	mutable FText NodeTitle;

	/** Cached info about input/output pins */
	TArray<TSharedRef<FControlRigField>> InputInfos;
	TArray<TSharedRef<FControlRigField>> InputOutputInfos;
	TArray<TSharedRef<FControlRigField>> OutputInfos;

public:
	UControlRigGraphNode()
		: Dimensions(0.0f, 0.0f)
		, NodeTitleFull(FText::GetEmpty())
		, NodeTitle(FText::GetEmpty())
	{
	}

	// UEdGraphNode Interface.
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void AllocateDefaultPins() override;
	virtual void ReconstructNode() override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	virtual void DestroyNode() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual TSharedPtr<INameValidatorInterface> MakeNameValidator() const override;
	virtual void OnRenameNode(const FString& InNewName) override;
	virtual FText GetTooltipText() const override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* InSchema) const override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;	

	// UK2Node Interface
	// @TODO: need to find a better way to handle the following functions! 
	// We cant derive from UK2Node as we don't want most of its functionality
// 	virtual bool ShouldShowNodeProperties() const { return true; }

	/** Handle a variable being renamed */
	virtual void HandleVariableRenamed(UBlueprint* InBlueprint, UClass* InVariableClass, UEdGraph* InGraph, const FName& InOldVarName, const FName& InNewVarName);

	/** Set the cached dimensions of this node */
	void SetDimensions(const FVector2D& InDimensions) { Dimensions = InDimensions; }

	/** Get the cached dimensions of this node */
	const FVector2D& GetDimensions() const { return Dimensions; }

	/** Set the property name we reference */
	void SetPropertyName(const FName& InPropertyName, bool bReplaceInnerProperties=false);

	/** Get the property name we reference */
	FName GetPropertyName() const { return PropertyName; }

	/** Get the input variable names */
	const TArray<TSharedRef<FControlRigField>>& GetInputVariableInfo() const { return InputInfos; }

	/** Get the input-output variable names */
	const TArray<TSharedRef<FControlRigField>>& GetInputOutputVariableInfo() const { return InputOutputInfos; }

	/** Get the output variable names */
	const TArray<TSharedRef<FControlRigField>>& GetOutputVariableInfo() const { return OutputInfos; }

	/** Record a pin's expansion state */
	void SetPinExpansion(const FString& InPinPropertyPath, bool bExpanded);

	/** Check a pin's expansion state */
	bool IsPinExpanded(const FString& InPinPropertyPath) const;

	/** Propagate pin defaults to underlying properties if they have changed */
	void CopyPinDefaultsToProperties(UEdGraphPin* Pin, bool bCallModify, bool bPropagateToInstances);

	/** Check whether we are a property accessor */
	bool IsPropertyAccessor() const { return GetUnitScriptStruct() == nullptr; }

	/** Get the blueprint that this node is contained within */
	UControlRigBlueprint* GetBlueprint() const;

	/** Add a new array element to the array referred to by the property path */
	void HandleAddArrayElement(FString InPropertyPath);

	/** Rebuild the cached info about our inputs/outputs */
	void CacheVariableInfo();

protected:
	/** Helper function for AllocateDefaultPins */
	void CreateInputPins();
	void CreateInputPins_Recursive(const TSharedPtr<FControlRigField>& InputInfo);

	/** Helper function for AllocateDefaultPins */
	void CreateInputOutputPins();
	void CreateInputOutputPins_Recursive(const TSharedPtr<FControlRigField>& InputOutputInfo);

	/** Helper function for AllocateDefaultPins */
	void CreateOutputPins();
	void CreateOutputPins_Recursive(const TSharedPtr<FControlRigField>& OutputInfo);

	/** Get the generated ControlRig class */
	UClass* GetControlRigGeneratedClass() const;

	/** Get the skeleton generated ControlRig class */
	UClass* GetControlRigSkeletonGeneratedClass() const;

	/** Create a ControlRig field from a field on the ControlRig class, if possible */
	TSharedPtr<FControlRigField> CreateControlRigField(UField* Field, const FString& PropertyPath, int32 InArrayIndex = INDEX_NONE) const;

	/** Get all fields that act as inputs for this node */
	void GetInputFields(TArray<TSharedRef<FControlRigField>>& OutFields) const;

	/** Get all fields that act as outputs for this node */
	void GetOutputFields(TArray<TSharedRef<FControlRigField>>& OutFields) const;

	/** Get all fields that act as input-outputs for this node */
	void GetInputOutputFields(TArray<TSharedRef<FControlRigField>>& OutFields) const;

	/** Helper function for GetInputFields/GetOutputFields */
	void GetFields(TFunction<bool(UProperty*)> InPropertyCheckFunction, TArray<TSharedRef<FControlRigField>>& OutFields) const;
	void GetFields_Recursive(const TSharedRef<FControlRigField>& ParentControlRigField, const FString& ParentPropertyPath) const;
	void GetFields_RecursiveHelper(UProperty* InProperty, const TSharedRef<FControlRigField>& InControlRigField, const FString& InPropertyPath) const;

	/** Get the struct property for the unit we represent, if any (we could just be a property accessor) */
	UStructProperty* GetUnitProperty() const;

	/** Get the script struct for the unit we represent, if any (we could just be a property accessor) */
	UScriptStruct* GetUnitScriptStruct() const;

	/** Get the property for the unit we represent */
	UProperty* GetProperty() const;

	/** Creates auto generated defaults for pins */
	void SetupPinAutoGeneratedDefaults(UEdGraphPin* Pin);

	/** Copies default values from underlying properties into pin defaults, for editing */
	void SetupPinDefaultsFromCDO(UEdGraphPin* Pin);

	/** Recreate pins when we reconstruct this node */
	virtual void ReallocatePinsDuringReconstruction(const TArray<UEdGraphPin*>& OldPins);

	/** Wire-up new pins given old pin wiring */
	virtual void RewireOldPinsToNewPins(TArray<UEdGraphPin*>& InOldPins, TArray<UEdGraphPin*>& InNewPins);

	/** Handle anything post-reconstruction */
	virtual void PostReconstructNode();

	/** Something that could change our title has changed */
	void InvalidateNodeTitle() const;

	/** Destroy all pins in an array */
	void DestroyPinList(TArray<UEdGraphPin*>& InPins);

	/** 
	 * Perform the specified operation on the array described by the passed-in property path. 
	 * If bCallModify is true then it is assumed that the array will be mutated.
	 */
	bool PerformArrayOperation(const FString& InPropertyPath, TFunctionRef<bool(FScriptArrayHelper&,int32)> InOperation, bool bCallModify, bool bPropagateToInstances) const;

	/** Clear the array referred to by the property path */
	void HandleClearArray(FString InPropertyPath);

	/** Remove the array element referred to by the property path */
	void HandleRemoveArrayElement(FString InPropertyPath);

	/** Insert a new array element after the element referred to by the property path */
	void HandleInsertArrayElement(FString InPropertyPath);
};
