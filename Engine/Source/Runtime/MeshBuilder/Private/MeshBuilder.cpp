// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshBuilder.h"
#include "MeshDescriptionHelper.h"
#include "MeshDescription.h"
#include "RawMesh.h"


FMeshBuilder::FMeshBuilder()
{

}

/** Convert this mesh description into the old FRawMesh format*/
void FMeshDescriptionOperations::ConverToRawMesh(const UMeshDescription* SourceMeshDescription, FRawMesh &DestinationRawMesh)
{
	FMeshDescriptionHelper::ConverToRawMesh(SourceMeshDescription, DestinationRawMesh);
}

/** Convert old FRawMesh format to MeshDescription*/
void FMeshDescriptionOperations::ConverFromRawMesh(const FRawMesh &SourceRawMesh, UMeshDescription* DestinationMeshDescription)
{
	FMeshDescriptionHelper::ConverFromRawMesh(SourceRawMesh, DestinationMeshDescription);
}
