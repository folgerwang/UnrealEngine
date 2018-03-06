// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FRawMesh;
struct FSectionInfo;
struct FMeshData;
class UMaterialInterface;

class IMeshMergeExtension
{
public:
	virtual void OnCreatedMergedRawMeshes(const TArray<UStaticMeshComponent*>& MergedComponents, const class FMeshMergeDataTracker& DataTracker, TArray<FRawMesh>& MergedMeshLODs) = 0;

	virtual void OnCreatedProxyMaterial(const TArray<UStaticMeshComponent*>& MergedComponents, UMaterialInterface* ProxyMaterial) = 0;
};