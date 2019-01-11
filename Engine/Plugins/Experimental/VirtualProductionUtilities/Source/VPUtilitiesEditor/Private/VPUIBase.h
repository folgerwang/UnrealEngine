// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Delegates/IDelegateInstance.h"

#include "VPUIBase.generated.h"


class AActor;
class UVPBookmark;


UCLASS()
class UVPUIBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Virtual Production")
	AActor* SelectedActor;
	
	UFUNCTION(BlueprintCallable, Category = "VirtualProduction")
	void AppendVirtualProductionLog(FString NewMessage);

protected:
	virtual bool Initialize() override;
	virtual void BeginDestroy() override;

	/* Log */
	
	UFUNCTION(BlueprintCallable, Category = "VirtualProduction")
	FString GetLastVirtualProductionLogMessage();

	UFUNCTION(BlueprintImplementableEvent, Category = "Virtual Production")
	void OnVirtualProductionLogUpdated();

	/* Bookmarks */

	UFUNCTION(BlueprintImplementableEvent, Category = "Virtual Production")
	void OnSelectedActorChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Virtual Production")
	void OnBookmarkCreated(UVPBookmark* Created);

	UFUNCTION(BlueprintImplementableEvent, Category = "Virtual Production")
	void OnBookmarkDestroyed(UVPBookmark* Destroyed);

	UFUNCTION(BlueprintImplementableEvent, Category = "Virtual Production")
	void OnBookmarkCleared(UVPBookmark* Cleared);

	/* State changes */

	/** UI subscribes to this to know when the property window for SelectedActor should be refreshed */
	UFUNCTION(BlueprintImplementableEvent, Category = "Virtual Production")
	void OnSelectedActorPropertyChanged();

	/** Fires whenever flight mode changes. True if enabled, false is disengaged */
	UFUNCTION(BlueprintImplementableEvent, Category = "Virtual Production")
	void OnFlightModeChanged(const bool WasEntered);

private:
	UPROPERTY(Transient)
	TArray<FString> VirtualProductionLog; // Full internal log. We don't use this in the UI, but if we want to dump the entire log to a file later we could do that
	
	FDelegateHandle OnPropertyChangedDelegateHandle;	
	void OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);

	void CVarSinkHandler();
	
	void OnEditorSelectionChanged(UObject* NewSelection);
	void OnEditorSelectNone();
	void GetSelectedActor();
};