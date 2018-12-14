// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "BlueprintAsyncActionBase.generated.h"

class UGameInstance;

UCLASS()
class ENGINE_API UBlueprintAsyncActionBase : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Called to trigger the action once the delegates have been bound */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true"))
	virtual void Activate();

	/**
	 * Call to globally register this object with a game instance, it will not be destroyed until SetReadyToDestroy is called
	 * This allows having an action stay alive until SetReadyToDestroy is manually called, allowing it to be used inside loops or if the calling BP goes away
	 */
	virtual void RegisterWithGameInstance(UObject* WorldContextObject);
	virtual void RegisterWithGameInstance(UGameInstance* GameInstance);

	/** Call when the action is completely done, this makes the action free to delete, and will unregister it with the game instance */
	virtual void SetReadyToDestroy();

protected:
	TWeakObjectPtr<UGameInstance> RegisteredWithGameInstance;
};
