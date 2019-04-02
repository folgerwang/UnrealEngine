// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "VREditorFloatingUI.h"
#include "VREditorFloatingCameraUI.generated.h"

class UVREditorBaseUserWidget;
class UVREditorUISystem;

typedef FName VREditorPanelID;

/**
 * Represents an interactive floating UI camera preview panel in the VR Editor
 */
UCLASS()
class AVREditorFloatingCameraUI : public AVREditorFloatingUI
{
	GENERATED_BODY()

public:
	AVREditorFloatingCameraUI();
	void SetLinkedActor(class AActor* InActor);

	virtual FTransform MakeCustomUITransform() override;

private:
	UPROPERTY( )
	TWeakObjectPtr<AActor> LinkedActor;

};