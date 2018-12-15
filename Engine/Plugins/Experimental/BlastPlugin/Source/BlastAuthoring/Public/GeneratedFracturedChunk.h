// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once


struct FGeneratedFracturedChunk
{
	TSharedPtr<class UGeometryCollection> GeometryCollectionObject;
	FVector ChunkLocation;
	int32 ParentBone;
	int32 BoneID;
	bool FirstChunk;
	int32 FracturedChunkIndex;
};
