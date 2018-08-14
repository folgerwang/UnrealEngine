// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/World.h"

class UStaticMesh;
class USkeletalMesh;
class UGeometryCollection;

/**
* The public interface to this module
*/
class GEOMETRYCOLLECTIONCOMPONENTEDITOR_API FGeometryCollectionConversion
{
public:

	/**
	*  Appends a static mesh to a GeometryCollectionComponent.
	*  @param StaticMeshComponent : Const mesh to read vertex/normals/index data from
	*  @param StaticMeshTransform : Mesh transform.
	*  @param GeometryCollection  : Collection to append the mesh into.
	*/
	static void AppendStaticMesh(const UStaticMesh * StaticMesh, const FTransform & StaticMeshTransform, UGeometryCollection * GeometryCollection);

	/**
	*  Appends a skeletal mesh to a GeometryCollectionComponent.
	*  @param SkeletalMeshComponent : Const mesh to read vertex/normals/index data from
	*  @param SkeletalMeshTransform : Mesh transform.
	*  @param GeometryCollection    : Collection to append the mesh into.
	*/
	static void AppendSkeletalMesh(const USkeletalMesh* SkeletalMesh, const FTransform & SkeletalMeshTransform, UGeometryCollection * GeometryCollection);

	/**
	*  Command invoked from "GeometryCollection.CreateFromSelectedActors", uses the selected Actors to create a GeometryCollection Asset
	*  @param World
	*/
	static void CreateFromSelectedActorsCommand(UWorld * World);

	/**
	*  Command invoked from "GeometryCollection.CreateFromSelectedAssets", uses the selected Assets to create a GeometryCollection Asset
	*  @param World
	*/
	static void CreateFromSelectedAssetsCommand(UWorld * World);

};