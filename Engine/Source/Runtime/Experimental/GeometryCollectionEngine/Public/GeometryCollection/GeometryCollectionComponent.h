// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MeshComponent.h"
#include "Chaos/ChaosSolverActor.h"
#include "Field/FieldSystem.h"
#include "Field/FieldSystemActor.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "GeometryCollection/GeometryCollectionSolverCallbacks.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/Experimental/PhysScene_LLImmediate.h"

#include "GeometryCollectionComponent.generated.h"

struct FGeometryCollectionConstantData;
struct FGeometryCollectionDynamicData;
class FGeometryCollectionPhysicsProxy;
class UGeometryCollectionComponent;
class UBoxComponent;
class UGeometryCollectionCache;

namespace GeometryCollection
{
	enum class ESelectionMode : uint8
	{
		None = 0,
		AllGeometry,
		InverseGeometry
	};
}

USTRUCT()
struct FGeomComponentCacheParameters
{
	GENERATED_BODY()

	FGeomComponentCacheParameters();

	// Cache mode, whether disabled, playing or recording
	UPROPERTY(EditAnywhere, Category = Cache)
	EGeometryCollectionCacheType CacheMode;

	// The cache to target when recording or playing
	UPROPERTY(EditAnywhere, Category = Cache)
	UGeometryCollectionCache* TargetCache;

	// Cache mode, whether disabled, playing or recording
	UPROPERTY(EditAnywhere, Category = Cache)
	float ReverseCacheBeginTime;

	// Whether to buffer collisions during recording
	UPROPERTY(EditAnywhere, Category = Cache)
	bool SaveCollisionData;

	// Maximum size of the collision buffer
	UPROPERTY(EditAnywhere, Category = Cache)
	int32 CollisionDataMaxSize;

	// Spatial hash collision data
	UPROPERTY(EditAnywhere, Category = Cache)
	bool DoCollisionDataSpatialHash;

	// Spatial hash radius for collision data
	UPROPERTY(EditAnywhere, Category = Cache)
	float SpatialHashRadius;

	// Maximum number of collisions per cell
	UPROPERTY(EditAnywhere, Category = Cache)
	int32 MaxCollisionPerCell;

	// Whether to buffer trailing during recording
	UPROPERTY(EditAnywhere, Category = Cache)
	bool SaveTrailingData;

	// Maximum size of the trailing buffer
	UPROPERTY(EditAnywhere, Category = Cache)
	int32 TrailingDataSizeMax;

	// Minimum speed to record trailing
	UPROPERTY(EditAnywhere, Category = Cache)
	float TrailingMinSpeedThreshold;

	// Minimum volume to record trailing
	UPROPERTY(EditAnywhere, Category = Cache)
	float TrailingMinVolumeThreshold;
};

/**
*	FGeometryCollectionEdit
*     Structured RestCollection access where the scope
*     of the object controls serialization back into the
*     dynamic collection
*
*	This will force any simulating geometry collection out of the
*	solver so it can be edited and afterwards will recreate the proxy
*/
class GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionEdit
{
public:
	FGeometryCollectionEdit(UGeometryCollectionComponent * InComponent, bool InUpdate = true);
	~FGeometryCollectionEdit();

	UGeometryCollection* GetRestCollection();

private:
	UGeometryCollectionComponent * Component;
	const bool bUpdate;
	bool bHadPhysicsState;
};

class GEOMETRYCOLLECTIONENGINE_API FScopedColorEdit
{
public:
	FScopedColorEdit(UGeometryCollectionComponent* InComponent);
	~FScopedColorEdit();

	void SetShowBoneColors(bool ShowBoneColorsIn);
	bool GetShowBoneColors() const;

	void SetShowSelectedBones(bool ShowSelectedBonesIn);
	bool GetShowSelectedBones() const;

	bool IsBoneSelected(int BoneIndex) const;
	void SetSelectedBones(const TArray<int32>& SelectedBonesIn);
	void AppendSelectedBones(const TArray<int32>& SelectedBonesIn);
	void AddSelectedBone(int32 BoneIndex);
	void ClearSelectedBone(int32 BoneIndex);
	const TArray<int32>& GetSelectedBones() const;
	void ResetBoneSelection();
	void SelectBones(GeometryCollection::ESelectionMode SelectionMode);

	bool IsBoneHighlighted(int BoneIndex) const;
	void SetHighlightedBones(const TArray<int32>& HighlightedBonesIn);
	void AddHighlightedBone(int32 BoneIndex);
	const TArray<int32>& GetHighlightedBones() const;
	void ResetHighlightedBones();

	void SetLevelViewMode(int ViewLevel);
	int GetViewLevel();

private:
	void UpdateBoneColors();

	UGeometryCollectionComponent * Component;
	static TArray<FLinearColor> RandomColors;
};

/**
*	GeometryCollectionComponent
*/
UCLASS(meta = (BlueprintSpawnableComponent))
class GEOMETRYCOLLECTIONENGINE_API UGeometryCollectionComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()
	friend class FGeometryCollectionEdit;
	friend class FScopedColorEdit;
	friend class FGeometryCollectionCommands;

public:

	//~ Begin UActorComponent Interface.
	virtual void CreateRenderState_Concurrent() override;
	virtual void SendRenderDynamicData_Concurrent() override;
	FORCEINLINE void SetRenderStateDirty() { bRenderStateDirty = true; }
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type ReasonEnd) override;
	//~ Begin UActorComponent Interface. 


	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool HasAnySockets() const override { return false; }
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ Begin USceneComponent Interface.


	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void OnRegister()  override;
	//~ End UPrimitiveComponent Interface.


	//~ Begin UMeshComponent Interface.
	// #todo(dmp): Is OverrideMaterials the correct thing to query here?
	// #todo(dmp): for backwards compatibility with existing maps, we need to have a default of 3 materials.  Otherwise
	// some existing test scenes will crash
	FORCEINLINE virtual int32 GetNumMaterials() const override { return OverrideMaterials.Num() == 0 ? 3 : OverrideMaterials.Num(); }
	//~ End UMeshComponent Interface.

	/** Chaos RBD Solver */
	UPROPERTY(EditAnywhere, Category = "ChaosPhysics", meta = (DisplayName = "Chaos Solver"))
	AChaosSolverActor* ChaosSolverActor;

	/** RestCollection */
	void SetRestCollection(UGeometryCollection * RestCollectionIn);
	FORCEINLINE const UGeometryCollection* GetRestCollection() const { return RestCollection; }
	FORCEINLINE FGeometryCollectionEdit EditRestCollection(bool Update = true) { return FGeometryCollectionEdit(this, Update); }
	FORCEINLINE FScopedColorEdit EditBoneSelection() { return FScopedColorEdit(this); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChaosPhysics")
	UGeometryCollection* RestCollection;

	/** DynamicCollection */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ChaosPhysics")
	UGeometryCollection* DynamicCollection;
	FORCEINLINE UGeometryCollection* GetDynamicCollection() { return DynamicCollection; }
	FORCEINLINE const UGeometryCollection* GetDynamicCollection() const { return DynamicCollection; }


	/* FieldSystem */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChaosPhysics")
	AFieldSystemActor * FieldSystem;

	/**
	* When Simulating is enabled the Component will initialize its rigid bodies within the solver.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|General")
	bool Simulating;

	/*
	*  ObjectType defines how to initialize the rigid objects state, Kinematic, Sleeping, Dynamic.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|General")
	EObjectTypeEnum ObjectType;

	/*
	*  CollisionType defines how to initialize the rigid collision structures.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Clustering")
	bool EnableClustering;

	/**
	* Maximum level for cluster breaks
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Clustering")
	int32 MaxClusterLevel;

	/**
	* Damage threshold for clusters at differnet levels.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Clustering")
	TArray<float> DamageThreshold;

	/*
	*  CollisionType defines how to initialize the rigid collision structures.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	ECollisionTypeEnum CollisionType;

	/*
	*  CollisionType defines how to initialize the rigid collision structures.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	EImplicitTypeEnum ImplicitType;

	/*
	*  Resolution on the smallest axes for the level set. (def: 5)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	int32 MinLevelSetResolution;

	/*
	*  Resolution on the smallest axes for the level set. (def: 10)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	int32 MaxLevelSetResolution;

	/**
	* Mass As Density (def:false)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	bool MassAsDensity;

	/**
	* Total Mass of Collection (def : 1.0)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float Mass;

	/**
	* Smallest allowable mass (def:0.1)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float MinimumMassClamp;

	/**
	 * Number of particles on the triangulated surface to use for collisions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float CollisionParticlesFraction;

	/**
	* Uniform Friction
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float Friction;

	/**
	* Coefficient of Restitution (aka Bouncyness)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float Bouncyness;

	/**
	* Uniform Friction
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float LinearSleepingThreshold;

	/**
	* Coefficient of Restitution (aka Bouncyness)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float AngularSleepingThreshold;

	/**
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Initial Velocity")
	EInitialVelocityTypeEnum InitialVelocityType;

	/**
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Initial Velocity")
	FVector InitialLinearVelocity;

	/**
	*
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Initial Velocity")
	FVector InitialAngularVelocity;

	UPROPERTY(EditAnywhere, Category = "ChaosPhysics|Caching", meta=(ShowOnlyInnerProperties))
	FGeomComponentCacheParameters CacheParameters;

	/**
	*
	*/
	/* ---------------------------------------------------------------------------------------- */
	
	void SetShowBoneColors(bool ShowBoneColorsIn);
	bool GetShowBoneColors() const { return ShowBoneColors; }
	bool GetShowSelectedBones() const { return ShowSelectedBones; }
	
	// Init the material slots on the component.  Note that this will also add the slots for internal
	// materials and the selection material
	void InitializeMaterials(const TArray<UMaterialInterface*> &Materials, int32 InteriorMaterialIndex, int32 BoneSelectedMaterialIndex);
	
	// #todo(dmp): Do we want to call these "ID" or "index".  They are used differently in some places, but they
	// do represent the index into the material slots array on the component
	int GetInteriorMaterialID() { return InteriorMaterialID;  }
	int GetBoneSelectedMaterialID() { return BoneSelectedMaterialID; }
	
	FORCEINLINE const TArray<int32>& GetSelectedBones() const { return SelectedBones; }
	FORCEINLINE const TArray<int32>& GetHighlightedBones() const { return HighlightedBones; }

	void ForceInitRenderData();

	FGeometryCollectionPhysicsProxy* GetPhysicsProxy() { return PhysicsProxy; }

#if INCLUDE_CHAOS

	const TSharedPtr<FPhysScene_Chaos> GetPhysicsScene() const;

#endif

	/**/
	const TManagedArray<int32>& GetRigidBodyIdArray() const { return RigidBodyIds; }

	UPROPERTY(transient)
	UBoxComponent* DummyBoxComponent;

	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	virtual bool ShouldCreatePhysicsState() const override;
	virtual bool HasValidPhysicsState() const override;

	// Mirrored from the proxy on a sync
	TManagedArray<int32> RigidBodyIds;

protected:

	/** Populate the static geometry structures for the render thread. */
	void InitConstantData(FGeometryCollectionConstantData* ConstantData);

	/** Populate the dynamic particle data for the render thread. */
	void InitDynamicData(FGeometryCollectionDynamicData* ConstantData);

	/** Reset the dynamic collection from the current rest state. */
	void ResetDynamicCollection();

private:
	bool bRenderStateDirty;
	bool ShowBoneColors;
	bool ShowSelectedBones;
	int ViewLevel;
	
	UPROPERTY()
	int InteriorMaterialID;

	UPROPERTY()
	int BoneSelectedMaterialID;
	
	TArray<int32> SelectedBones;
	TArray<int32> HighlightedBones;

	FGeometryCollectionPhysicsProxy* PhysicsProxy;

#if WITH_EDITORONLY_DATA
	// Tracked editor actor that owns the original component so we can write back recorded caches
	// from PIE.
	UPROPERTY(Transient)
	AActor* EditorActor;
#endif
};
