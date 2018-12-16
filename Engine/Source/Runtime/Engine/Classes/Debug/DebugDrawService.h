// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ShowFlags.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DebugDrawService.generated.h"

class FCanvas;
class FSceneView;

/** 
 * 
 */
DECLARE_DELEGATE_TwoParams(FDebugDrawDelegate, class UCanvas*, class APlayerController*);

UCLASS(config=Engine)
class ENGINE_API UDebugDrawService : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	//void Register

	static FDelegateHandle Register(const TCHAR* Name, const FDebugDrawDelegate& NewDelegate);
	static void Unregister(FDelegateHandle HandleToRemove);

	// Draws debug canvas that has already been initialized to a viewport
	static void Draw(const FEngineShowFlags Flags, class UCanvas* Canvas);

	// Initialize a debug canvas object then calls above draw. If CanvasObject is null it will find/create it for you
	static void Draw(const FEngineShowFlags Flags, class FViewport* Viewport, FSceneView* View, FCanvas* Canvas, class UCanvas* CanvasObject = nullptr);

private:
	static TArray<TArray<FDebugDrawDelegate> > Delegates;
	static FEngineShowFlags ObservedFlags;
};
