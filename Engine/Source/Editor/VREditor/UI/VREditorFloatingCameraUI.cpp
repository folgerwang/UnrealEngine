// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VREditorFloatingCameraUI.h"
#include "VREditorUISystem.h"
#include "VREditorBaseUserWidget.h"
#include "VREditorMode.h"
#include "Components/WidgetComponent.h"
#include "VREditorWidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "VRModeSettings.h"
#include "VREditorAssetContainer.h"
#include "SLevelViewport.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AVREditorFloatingCameraUI"

AVREditorFloatingCameraUI::AVREditorFloatingCameraUI():
	Super()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	const UVREditorAssetContainer& AssetContainer = UVREditorMode::LoadAssetContainer();

	{
		UStaticMesh* WindowMesh = AssetContainer.WindowMesh;
		WindowMeshComponent->SetStaticMesh(WindowMesh);
		check(WindowMeshComponent != nullptr);
	}
}

void AVREditorFloatingCameraUI::SetLinkedActor(class AActor* InActor)
{
	const FScopedTransaction Transaction( LOCTEXT( "AVREditorFloatingCameraUI", "SetLinkedActor" ) );
	Modify();
	LinkedActor = InActor;
}

FTransform AVREditorFloatingCameraUI::MakeCustomUITransform()
{
	FTransform CameraTransform = FTransform::Identity;
	FTransform UITransform = FTransform::Identity;
	if (LinkedActor != nullptr)
	{
		CameraTransform = LinkedActor->GetTransform();

		const FTransform UIFlipTransform(FRotator(0.0f, 180.0f, 0.0f).Quaternion(), FVector::ZeroVector);
		const FVector Offset = FVector(-25.0f, 0.0f, 80.0f);
		const FTransform OffsetTransform(FRotator::ZeroRotator, Offset);

		UITransform = UIFlipTransform * OffsetTransform * CameraTransform;
	}
	return UITransform;
}

#undef LOCTEXT_NAMESPACE
