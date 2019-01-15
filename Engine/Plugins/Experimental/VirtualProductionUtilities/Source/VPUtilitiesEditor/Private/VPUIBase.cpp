// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VPUIBase.h"

#include "VPBookmark.h"
#include "VPBookmarkLifecycleDelegates.h"

#include "EngineUtils.h"
#include "Editor.h"

#include "Engine/Selection.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "VPUIBase"


bool UVPUIBase::Initialize()
{
	const bool SuperInitialized = UUserWidget::Initialize();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FVPBookmarkLifecycleDelegates::GetOnBookmarkCreated().AddUObject(this, &ThisClass::OnBookmarkCreated);
		FVPBookmarkLifecycleDelegates::GetOnBookmarkDestroyed().AddUObject(this, &ThisClass::OnBookmarkDestroyed);
		FVPBookmarkLifecycleDelegates::GetOnBookmarkCleared().AddUObject(this, &ThisClass::OnBookmarkCleared);
	}

	USelection::SelectNoneEvent.AddUObject(this, &UVPUIBase::OnEditorSelectNone);
	USelection::SelectionChangedEvent.AddUObject(this, &UVPUIBase::OnEditorSelectionChanged);
	USelection::SelectObjectEvent.AddUObject(this, &UVPUIBase::OnEditorSelectionChanged);	
	// Monitor VI.NavigationMode cvar
	IConsoleManager::Get().RegisterConsoleVariableSink_Handle(FConsoleCommandDelegate::CreateUObject(this, &UVPUIBase::CVarSinkHandler));

	GetSelectedActor();

	return SuperInitialized;
}


void UVPUIBase::BeginDestroy()
{
	USelection::SelectObjectEvent.RemoveAll(this);
	USelection::SelectionChangedEvent.RemoveAll(this);
	USelection::SelectNoneEvent.RemoveAll(this);

	FVPBookmarkLifecycleDelegates::GetOnBookmarkCleared().RemoveAll(this);
	FVPBookmarkLifecycleDelegates::GetOnBookmarkDestroyed().RemoveAll(this);
	FVPBookmarkLifecycleDelegates::GetOnBookmarkCreated().RemoveAll(this);

	Super::BeginDestroy();
}


void UVPUIBase::AppendVirtualProductionLog(FString NewMessage)
{
	VirtualProductionLog.Add(NewMessage);
	OnVirtualProductionLogUpdated();
}


FString UVPUIBase::GetLastVirtualProductionLogMessage()
{
	if (VirtualProductionLog.Num())
	{
		return VirtualProductionLog.Last();
	}
	return FString();
}


void UVPUIBase::GetSelectedActor()
{
	AActor* OldSelectedActor = SelectedActor;

	SelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>();

	if (SelectedActor)
	{
		if (SelectedActor != OldSelectedActor)
		{
			OnPropertyChangedDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UVPUIBase::OnPropertyChanged);
			OnSelectedActorChanged();
		}
	}
	else
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedDelegateHandle);
		OnEditorSelectNone();
	}
}

/* Events */

void UVPUIBase::OnEditorSelectionChanged(UObject* NewSelection)
{
	GetSelectedActor();
}


void UVPUIBase::OnEditorSelectNone()
{
	SelectedActor = nullptr;
	OnSelectedActorChanged();
}


void UVPUIBase::OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (AActor* Actor = Cast<AActor>(ObjectBeingModified))
	{
		if (Actor == SelectedActor)
		{
			OnSelectedActorPropertyChanged();
		}
	}
}


void UVPUIBase::CVarSinkHandler()
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("VI.NavigationMode"));
	if (CVar)
	{
		if (CVar->GetInt() == 1)
		{
			OnFlightModeChanged(true);
		}
		else
		{
			OnFlightModeChanged(false);
		}
	}
}

#undef LOCTEXT_NAMESPACE