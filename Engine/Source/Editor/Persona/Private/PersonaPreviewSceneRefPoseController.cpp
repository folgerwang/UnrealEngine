// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PersonaPreviewSceneRefPoseController.h"
#include "AnimationEditorPreviewScene.h"

void UPersonaPreviewSceneRefPoseController::InitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const
{
	PreviewScene->ShowReferencePose(true, bResetBoneTransforms);
}

void UPersonaPreviewSceneRefPoseController::UninitializeView(UPersonaPreviewSceneDescription* SceneDescription, IPersonaPreviewScene* PreviewScene) const
{

}