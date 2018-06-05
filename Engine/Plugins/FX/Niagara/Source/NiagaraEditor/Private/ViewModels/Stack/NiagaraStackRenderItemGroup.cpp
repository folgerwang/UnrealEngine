// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackRenderItemGroup.h"
#include "ViewModels/Stack/NiagaraStackRendererItem.h"
#include "ViewModels/Stack/NiagaraStackSpacer.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterEditorData.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "NiagaraRendererProperties.h"

#include "ScopedTransaction.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "UNiagaraStackRenderItemGroup"

class FRenderItemGroupAddAction : public INiagaraStackItemGroupAddAction
{
public:
	FRenderItemGroupAddAction(UClass* InRendererClass)
		: RendererClass(InRendererClass)
	{
	}

	virtual FText GetCategory() const override
	{
		return LOCTEXT("AddRendererCategory", "Add Renderer");
	}

	virtual FText GetDisplayName() const override
	{
		return RendererClass->GetDisplayNameText();
	}

	virtual FText GetDescription() const override
	{
		return FText::FromString(RendererClass->GetDescription());
	}

	virtual FText GetKeywords() const override
	{
		return FText();
	}

	UClass* GetRendererClass() const
	{
		return RendererClass;
	}

private:
	UClass* RendererClass;
};

class FRenderItemGroupAddUtilities : public TNiagaraStackItemGroupAddUtilities<UNiagaraRendererProperties*>
{
public:
	FRenderItemGroupAddUtilities(TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, FOnItemAdded InOnItemAdded)
		: TNiagaraStackItemGroupAddUtilities(LOCTEXT("RenderGroupAddItemName", "Renderer"), EAddMode::AddFromAction, true, InOnItemAdded)
		, EmitterViewModel(InEmitterViewModel)
	{
	}

	virtual void AddItemDirectly() override { unimplemented(); }

	virtual void GenerateAddActions(TArray<TSharedRef<INiagaraStackItemGroupAddAction>>& OutAddActions) const override
	{
		TArray<UClass*> RendererClasses;
		GetDerivedClasses(UNiagaraRendererProperties::StaticClass(), RendererClasses);
		for (UClass* RendererClass : RendererClasses)
		{
			OutAddActions.Add(MakeShared<FRenderItemGroupAddAction>(RendererClass));
		}
	}

	virtual void ExecuteAddAction(TSharedRef<INiagaraStackItemGroupAddAction> AddAction, int32 TargetIndex) override
	{
		TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModelPinned = EmitterViewModel.Pin();
		if (EmitterViewModelPinned.IsValid() == false)
		{
			return;
		}

		TSharedRef<FRenderItemGroupAddAction> RenderAddAction = StaticCastSharedRef<FRenderItemGroupAddAction>(AddAction);

		FScopedTransaction ScopedTransaction(LOCTEXT("AddNewRendererTransaction", "Add new renderer"));

		UNiagaraEmitter* Emitter = EmitterViewModelPinned->GetEmitter();
		Emitter->Modify();
		UNiagaraRendererProperties* RendererProperties = NewObject<UNiagaraRendererProperties>(Emitter, RenderAddAction->GetRendererClass(), NAME_None, RF_Transactional);
		Emitter->AddRenderer(RendererProperties);

		bool bVarsAdded = false;
		TArray<FNiagaraVariable> MissingAttributes = UNiagaraStackRendererItem::GetMissingVariables(RendererProperties, Emitter);
		for (int32 i = 0; i < MissingAttributes.Num(); i++)
		{
			if (UNiagaraStackRendererItem::AddMissingVariable(Emitter, MissingAttributes[i]))
			{
				bVarsAdded = true;
			}
		}

		if (bVarsAdded)
		{
			FNotificationInfo Info(LOCTEXT("AddedVariables", "One or more variables have been added to the Spawn script to support the added renderer."));
			Info.ExpireDuration = 5.0f;
			Info.bFireAndForget = true;
			Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Info"));
			FSlateNotificationManager::Get().AddNotification(Info);
		}

		OnItemAdded.ExecuteIfBound(RendererProperties);
	}

private:
	TWeakPtr<FNiagaraEmitterViewModel> EmitterViewModel;
};

void UNiagaraStackRenderItemGroup::Initialize(FRequiredEntryData InRequiredEntryData)
{
	FText DisplayName = LOCTEXT("RenderGroupName", "Render");
	FText ToolTip = LOCTEXT("RendererGroupTooltip", "Describes how we should display/present each particle. Note that this doesn't have to be visual. Multiple renderers are supported. Order in this stack is not necessarily relevant to draw order.");
	AddUtilities = MakeShared<FRenderItemGroupAddUtilities>(InRequiredEntryData.EmitterViewModel,
		TNiagaraStackItemGroupAddUtilities<UNiagaraRendererProperties*>::FOnItemAdded::CreateUObject(this, &UNiagaraStackRenderItemGroup::ItemAdded));
	Super::Initialize(InRequiredEntryData, DisplayName, ToolTip, AddUtilities.Get());
}

void UNiagaraStackRenderItemGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	int32 RendererIndex = 0;
	for (UNiagaraRendererProperties* RendererProperties : GetEmitterViewModel()->GetEmitter()->GetRenderers())
	{
		FName RendererSpacerKey = *FString::Printf(TEXT("Renderer%i"), RendererIndex);
		UNiagaraStackSpacer* RendererSpacer = FindCurrentChildOfTypeByPredicate<UNiagaraStackSpacer>(CurrentChildren,
			[=](UNiagaraStackSpacer* CurrentRendererSpacer) { return CurrentRendererSpacer->GetSpacerKey() == RendererSpacerKey; });

		if (RendererSpacer == nullptr)
		{
			RendererSpacer = NewObject<UNiagaraStackSpacer>(this);
			RendererSpacer->Initialize(CreateDefaultChildRequiredData(), RendererSpacerKey);
		}

		NewChildren.Add(RendererSpacer);

		UNiagaraStackRendererItem* RendererItem = FindCurrentChildOfTypeByPredicate<UNiagaraStackRendererItem>(CurrentChildren,
			[=](UNiagaraStackRendererItem* CurrentRendererItem) { return CurrentRendererItem->GetRendererProperties() == RendererProperties; });

		if (RendererItem == nullptr)
		{
			RendererItem = NewObject<UNiagaraStackRendererItem>(this);
			RendererItem->Initialize(CreateDefaultChildRequiredData(), RendererProperties);
			RendererItem->SetOnModifiedGroupItems(UNiagaraStackItem::FOnModifiedGroupItems::CreateUObject(this, &UNiagaraStackRenderItemGroup::ChildModifiedGroupItems));
		}

		NewChildren.Add(RendererItem);

		RendererIndex++;
	}

	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
}

void UNiagaraStackRenderItemGroup::ItemAdded(UNiagaraRendererProperties* AddedRenderer)
{
	RefreshChildren();
	OnDataObjectModified().Broadcast(AddedRenderer);
}

void UNiagaraStackRenderItemGroup::ChildModifiedGroupItems()
{
	RefreshChildren();
}

#undef LOCTEXT_NAMESPACE

