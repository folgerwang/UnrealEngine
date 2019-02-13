// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "Async/AsyncWork.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "StaticMeshResources.h"

#include "HierarchicalInstancedStaticMeshComponent.generated.h"

class FClusterBuilder;
class FStaticLightingTextureMapping_InstancedStaticMesh;

// Due to BulkSerialize we can't edit the struct, so we must deprecated this one and create a new one
USTRUCT()
struct FClusterNode_DEPRECATED
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector BoundMin;
	UPROPERTY()
	int32 FirstChild;
	UPROPERTY()
	FVector BoundMax;
	UPROPERTY()
	int32 LastChild;
	UPROPERTY()
	int32 FirstInstance;
	UPROPERTY()
	int32 LastInstance;

	FClusterNode_DEPRECATED()
		: BoundMin(MAX_flt, MAX_flt, MAX_flt)
		, FirstChild(-1)
		, BoundMax(MIN_flt, MIN_flt, MIN_flt)
		, LastChild(-1)
		, FirstInstance(-1)
		, LastInstance(-1)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FClusterNode_DEPRECATED& NodeData)
	{
		// @warning BulkSerialize: FClusterNode is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << NodeData.BoundMin;
		Ar << NodeData.FirstChild;
		Ar << NodeData.BoundMax;
		Ar << NodeData.LastChild;
		Ar << NodeData.FirstInstance;
		Ar << NodeData.LastInstance;

		return Ar;
	}
};


USTRUCT()
struct FClusterNode
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector BoundMin;
	UPROPERTY()
	int32 FirstChild;
	UPROPERTY()
	FVector BoundMax;
	UPROPERTY()
	int32 LastChild;
	UPROPERTY()
	int32 FirstInstance;
	UPROPERTY()
	int32 LastInstance;

	UPROPERTY()
	FVector MinInstanceScale;
	UPROPERTY()
	FVector MaxInstanceScale;

	FClusterNode()
		: BoundMin(MAX_flt, MAX_flt, MAX_flt)
		, FirstChild(-1)
		, BoundMax(MIN_flt, MIN_flt, MIN_flt)
		, LastChild(-1)
		, FirstInstance(-1)
		, LastInstance(-1)
		, MinInstanceScale(MAX_flt)
		, MaxInstanceScale(-MAX_flt)
	{
	}

	FClusterNode(const FClusterNode_DEPRECATED& OldNode)
		: BoundMin(OldNode.BoundMin)
		, FirstChild(OldNode.FirstChild)
		, BoundMax(OldNode.BoundMax)
		, LastChild(OldNode.LastChild)
		, FirstInstance(OldNode.FirstChild)
		, LastInstance(OldNode.LastInstance)
		, MinInstanceScale(MAX_flt)
		, MaxInstanceScale(-MAX_flt)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FClusterNode& NodeData)
	{
		// @warning BulkSerialize: FClusterNode is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << NodeData.BoundMin;
		Ar << NodeData.FirstChild;
		Ar << NodeData.BoundMax;
		Ar << NodeData.LastChild;
		Ar << NodeData.FirstInstance;
		Ar << NodeData.LastInstance;
		Ar << NodeData.MinInstanceScale;
		Ar << NodeData.MaxInstanceScale;
		
		return Ar;
	}
};

UCLASS(ClassGroup=Rendering, meta=(BlueprintSpawnableComponent))
class ENGINE_API UHierarchicalInstancedStaticMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

	~UHierarchicalInstancedStaticMeshComponent();

	TSharedPtr<TArray<FClusterNode>, ESPMode::ThreadSafe> ClusterTreePtr;

	// Table for remaping instances from cluster tree to PerInstanceSMData order
	UPROPERTY()
	TArray<int32> SortedInstances;

	// The number of instances in the ClusterTree. Subsequent instances will always be rendered.
	UPROPERTY()
	int32 NumBuiltInstances;

	// Normally equal to NumBuiltInstances, but can be lower if density scaling is in effect
	int32 NumBuiltRenderInstances;

	// Bounding box of any built instances (cached from the ClusterTree)
	UPROPERTY()
	FBox BuiltInstanceBounds;

	// Bounding box of any unbuilt instances
	UPROPERTY()
	FBox UnbuiltInstanceBounds;

	// Bounds of each individual unbuilt instance, used for LOD calculation
	UPROPERTY()
	TArray<FBox> UnbuiltInstanceBoundsList;

	// Enable for detail meshes that don't really affect the game. Disable for anything important.
	// Typically, this will be enabled for small meshes without collision (e.g. grass) and disabled for large meshes with collision (e.g. trees)
	UPROPERTY()
	uint32 bEnableDensityScaling:1;

	// Current value of density scaling applied to this component
	float CurrentDensityScaling;

#if WITH_EDITOR
	// in Editor mode we might disable the density scaling for edition
	bool bCanEnableDensityScaling;
#endif

	// The number of nodes in the occlusion layer
	UPROPERTY()
	int32 OcclusionLayerNumNodes;

	// The last mesh bounds that was cache
	UPROPERTY()
	FBoxSphereBounds CacheMeshExtendedBounds;

	bool bIsAsyncBuilding;
	bool bDiscardAsyncBuildResults;
	bool bConcurrentChanges;
	bool bAutoRebuildTreeOnInstanceChanges;

	UPROPERTY()
	bool bDisableCollision;

	// Instances to render (including removed one until the build is complete)
	UPROPERTY()
	int32 InstanceCountToRender;

	// Apply the results of the async build
	void ApplyBuildTreeAsync(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, TSharedRef<FClusterBuilder, ESPMode::ThreadSafe> Builder, double StartTime);

	virtual void OnComponentCreated() override;

public:

	//Begin UObject Interface
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	// UInstancedStaticMesh interface
	virtual int32 AddInstance(const FTransform& InstanceTransform) override;
	virtual bool RemoveInstance(int32 InstanceIndex) override;
	virtual bool UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty = false, bool bTeleport = false) override;
	virtual void ClearInstances() override;
	virtual TArray<int32> GetInstancesOverlappingSphere(const FVector& Center, float Radius, bool bSphereInWorldSpace = true) const override;
	virtual TArray<int32> GetInstancesOverlappingBox(const FBox& Box, bool bBoxInWorldSpace = true) const override;
	virtual void PreAllocateInstancesMemory(int32 AddedInstanceCount) override;

	/** Removes all the instances with indices specified in the InstancesToRemove array. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	bool RemoveInstances(const TArray<int32>& InstancesToRemove);

	/** Get the number of instances that overlap a given sphere */
	int32 GetOverlappingSphereCount(const FSphere& Sphere) const;
	/** Get the number of instances that overlap a given box */
	int32 GetOverlappingBoxCount(const FBox& Box) const;
	/** Get the transforms of instances inside the provided box */
	void GetOverlappingBoxTransforms(const FBox& Box, TArray<FTransform>& OutTransforms) const;

	virtual bool ShouldCreatePhysicsState() const override;

	bool BuildTreeIfOutdated(bool Async, bool ForceUpdate);
	static void BuildTreeAnyThread(TArray<FMatrix>& InstanceTransforms, const FBox& MeshBox, TArray<FClusterNode>& OutClusterTree, TArray<int32>& OutSortedInstances, TArray<int32>& OutInstanceReorderTable, int32& OutOcclusionLayerNum, int32 MaxInstancesPerLeaf );
	void AcceptPrebuiltTree(TArray<FClusterNode>& InClusterTree, int32 InOcclusionLayerNumNodes, int32 InNumBuiltRenderInstances);
	bool IsAsyncBuilding() const { return bIsAsyncBuilding; }
	bool IsTreeFullyBuilt() const { return NumBuiltInstances == PerInstanceSMData.Num(); }

	/** Heuristic for the number of leaves in the tree **/
	int32 DesiredInstancesPerLeaf();

	virtual void ApplyComponentInstanceData(struct FInstancedStaticMeshComponentInstanceData* InstancedMeshData) override;
	
	// Number of instances in the render-side instance buffer
	virtual int32 GetNumRenderInstances() const { return SortedInstances.Num(); }

	/** Will apply current density scaling, if enabled **/
	void UpdateDensityScaling();

	virtual void PropagateLightingScenarioChange() override;

protected:
	void BuildTree();
	void BuildTreeAsync();
	void SetPerInstanceLightMapAndEditorData(FStaticMeshInstanceData& PerInstanceData, const TArray<TRefCountPtr<HHitProxy>>& HitProxies);

	/** Removes specified instances */ 
	void RemoveInstancesInternal(const int32* InstanceIndices, int32 Num);
	
	/** Gets and approximate number of verts for each LOD to generate heuristics **/
	int32 GetVertsForLOD(int32 LODIndex);
	/** Average number of instances per leaf **/
	float ActualInstancesPerLeaf();
	/** For testing, prints some stats after any kind of build **/
	void PostBuildStats();

	virtual void OnPostLoadPerInstanceData() override;

	virtual void GetNavigationPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& InstanceData) const override;
	virtual void PartialNavigationUpdate(int32 InstanceIdx) override;
	virtual FBox GetNavigationBounds() const override;
	void FlushAccumulatedNavigationUpdates();
	mutable FBox AccumulatedNavigationDirtyArea;

	FGraphEventArray BuildTreeAsyncTasks;

protected:
	friend FStaticLightingTextureMapping_InstancedStaticMesh;
	friend FInstancedLightMap2D;
	friend FInstancedShadowMap2D;
	friend class FClusterBuilder;
};

