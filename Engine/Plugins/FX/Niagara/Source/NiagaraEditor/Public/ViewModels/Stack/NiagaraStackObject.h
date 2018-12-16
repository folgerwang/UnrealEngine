// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "Misc/NotifyHook.h"
#include "PropertyEditorDelegates.h"
#include "NiagaraStackObject.generated.h"

class IPropertyRowGenerator;
class UNiagaraNode;
class IDetailTreeNode;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackObject : public UNiagaraStackItemContent, public FNotifyHook
{
	GENERATED_BODY()
		
public:
	DECLARE_DELEGATE_TwoParams(FOnSelectRootNodes, TArray<TSharedRef<IDetailTreeNode>>, TArray<TSharedRef<IDetailTreeNode>>*);

public:
	UNiagaraStackObject();

	void Initialize(FRequiredEntryData InRequiredEntryData, UObject* InObject, FString InOwnerStackItemEditorDataKey, UNiagaraNode* InOwningNiagaraNode = nullptr);

	void SetOnSelectRootNodes(FOnSelectRootNodes OnSelectRootNodes);

	void RegisterInstancedCustomPropertyLayout(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate);
	void RegisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier = nullptr);

	UObject* GetObject();

	//~ FNotifyHook interface
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged) override;

	//~ UNiagaraStackEntry interface
	virtual bool GetIsEnabled() const override;
	virtual bool GetShouldShowInStack() const override;

protected:
	virtual void FinalizeInternal() override;

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues);

private:
	void PropertyRowsRefreshed();

private:
	struct FRegisteredClassCustomization
	{
		UStruct* Class;
		FOnGetDetailCustomizationInstance DetailLayoutDelegate;
	};

	struct FRegisteredPropertyCustomization
	{
		FName PropertyTypeName;
		FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate;
		TSharedPtr<IPropertyTypeIdentifier> Identifier;
	};

	UObject* Object;
	UNiagaraNode* OwningNiagaraNode;
	FOnSelectRootNodes OnSelectRootNodesDelegate;
	TArray<FRegisteredClassCustomization> RegisteredClassCustomizations;
	TArray<FRegisteredPropertyCustomization> RegisteredPropertyCustomizations;
	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;
};
