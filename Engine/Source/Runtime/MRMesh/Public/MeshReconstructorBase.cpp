// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MeshReconstructorBase.h"


void UMeshReconstructorBase::StartReconstruction()
{
}

void UMeshReconstructorBase::StopReconstruction()
{
}

void UMeshReconstructorBase::PauseReconstruction()
{
}

bool UMeshReconstructorBase::IsReconstructionStarted() const
{
	return false;
}

bool UMeshReconstructorBase::IsReconstructionPaused() const
{
	return false;
}

void UMeshReconstructorBase::ConnectMRMesh(class UMRMeshComponent* Mesh)
{
}

void UMeshReconstructorBase::DisconnectMRMesh()
{
}