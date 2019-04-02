// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "PackedNormal.h"

#include "MRMeshComponent.generated.h"

class UMaterial;
class FBaseMeshReconstructorModule;
class UMeshReconstructorBase;
struct FDynamicMeshVertex;

DECLARE_STATS_GROUP(TEXT("MRMesh"), STATGROUP_MRMESH, STATCAT_Advanced);
DEFINE_LOG_CATEGORY_STATIC(LogMrMesh, Warning, All);

class IMRMesh
{
public:
	struct FBrickDataReceipt
	{
		// Optionally subclass and use receipt.  For example: to release the buffers FSendBrickDataArgs has references to.
		virtual ~FBrickDataReceipt() {}
	};

	typedef uint64 FBrickId;
	struct FSendBrickDataArgs
	{
		TSharedPtr<FBrickDataReceipt, ESPMode::ThreadSafe> BrickDataReceipt;
		const FBrickId BrickId;
		const TArray<FVector>& PositionData;
		const TArray<FVector2D>& UVData;
		const TArray<FPackedNormal>& TangentXZData;
		const TArray<FColor>& ColorData;
		const TArray<uint32>& Indices;
	};

	virtual void SetConnected(bool value) = 0;
	virtual bool IsConnected() const = 0;

	virtual void SendRelativeTransform(const FTransform& Transform) = 0;
	virtual void SendBrickData(FSendBrickDataArgs Args) = 0;
	virtual void Clear() = 0;
	virtual void ClearAllBrickData() = 0;
};



UCLASS(hideCategories=(Physics), meta = (BlueprintSpawnableComponent, Experimental), ClassGroup = Rendering)
class MRMESH_API UMRMeshComponent : public UPrimitiveComponent, public IMRMesh
{
public:
	friend class FMRMeshProxy;

	GENERATED_UCLASS_BODY()

	virtual void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Mesh Reconstruction")
	bool IsConnected() const override { return bConnected; }

	void SetConnected(bool value) override { bConnected = value; }
	virtual void SendRelativeTransform(const FTransform& Transform) override { SetRelativeTransform(Transform); }

	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	void ForceNavMeshUpdate();

	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	void Clear() override;

	// UPrimitiveComponent.. public BP function needs to stay public to avoid nativization errors. (RR)
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* InMaterial) override;
protected:
	virtual void OnActorEnableCollisionChanged() override;
	virtual void UpdatePhysicsToRBChannels() override;
public:
	virtual void SetCollisionObjectType(ECollisionChannel Channel) override;
	virtual void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse) override;
	virtual void SetCollisionResponseToAllChannels(ECollisionResponse NewResponse) override;
	virtual void SetCollisionResponseToChannels(const FCollisionResponseContainer& NewResponses) override;
	virtual void SetCollisionEnabled(ECollisionEnabled::Type NewType) override;
	virtual void SetCollisionProfileName(FName InCollisionProfileName) override;

	virtual void SetWalkableSlopeOverride(const FWalkableSlopeOverride& NewOverride) override;

private:
	//~ UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
	virtual class UBodySetup* GetBodySetup();
	//~ UPrimitiveComponent
	//~ UActorComponent
	virtual bool ShouldCreatePhysicsState() const override;
	virtual void SendRenderDynamicData_Concurrent() override;
	//~ UActorComponent

	//~ IMRMesh
	virtual void SendBrickData(FSendBrickDataArgs Args) override;
	virtual void ClearAllBrickData() override;
	//~ IMRMesh

private:
	void CacheBodySetupHelper();
	class UBodySetup* CreateBodySetupHelper();
	void SendBrickData_Internal(IMRMesh::FSendBrickDataArgs Args);

	void RemoveBodyInstance(int32 BodyIndex);
	void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

	void ClearAllBrickData_Internal();

	UPROPERTY(EditAnywhere, Category = Appearance)
	UMaterialInterface* Material;

	/** If true, MRMesh will create a renderable mesh proxy.  If false it will not, but could still provide collision. */
	UPROPERTY(EditAnywhere, Category = Appearance)
	bool bCreateMeshProxySections = true;

	/** If true, MRMesh will automatically update its navmesh whenever any Mesh section is updated. This may be expensive. If this is disabled use ForceNavMeshUpdate to trigger a navmesh update when necessary.  Moving the component will also trigger a navmesh update.*/
	UPROPERTY(EditAnywhere, Category = MRMesh)
	bool bUpdateNavMeshOnMeshUpdate = true;

	/** If true, MRMesh will not create a collidable ridgid body for each mesh section and can therefore never have collision.  Avoids the cost of generating collision.*/
	UPROPERTY(EditAnywhere, Category = MRMesh)
	bool bNeverCreateCollisionMesh = false;

	bool bConnected = false;

	UPROPERTY(Transient)
	class UBodySetup* CachedBodySetup = nullptr;

	UPROPERTY(Transient)
	TArray<UBodySetup*> BodySetups;

	TArray<FBodyInstance*> BodyInstances;
	TArray<IMRMesh::FBrickId> BodyIds;
};