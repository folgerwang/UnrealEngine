// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection.h"

#include "GeometryCollectionComponent.generated.h"

struct FGeometryCollectionConstantData;
struct FGeometryCollectionDynamicData;
class UGeometryCollectionComponent;

/**
*	FGeometryCollectionEdit
*     Structured RestCollection access where the scope
*     of the object controls serialization back into the
*     dynamic collection
*/
class GEOMETRYCOLLECTIONCOMPONENT_API FGeometryCollectionEdit
{

	UGeometryCollectionComponent * Component;
	bool Update;

public:
	FGeometryCollectionEdit(UGeometryCollectionComponent * InComponent, bool InUpdate = true)
		: Component(InComponent)
		, Update(InUpdate)
	{}
	~FGeometryCollectionEdit();

	UGeometryCollection* GetRestCollection();
};

/**
*	GeometryCollectionComponent
*/
UCLASS(meta = (BlueprintSpawnableComponent))
class GEOMETRYCOLLECTIONCOMPONENT_API UGeometryCollectionComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()
	friend class FGeometryCollectionEdit;

public:
	//~ Begin UActorComponent Interface.
	virtual void CreateRenderState_Concurrent() override;
	virtual void SendRenderDynamicData_Concurrent() override;
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
	FORCEINLINE virtual int32 GetNumMaterials() const override { return 1; }
	//~ End UMeshComponent Interface.

	/** Accessors for the RestCollection */
	void SetRestCollection(UGeometryCollection * RestCollectionIn);
	FORCEINLINE const UGeometryCollection* GetRestCollection() const { return RestCollection; }
	FORCEINLINE FGeometryCollectionEdit EditRestCollection(bool Update = true) { return FGeometryCollectionEdit(this, Update); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	UGeometryCollection* RestCollection;

	/** Accessors for the DynamicCollection */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	UGeometryCollection* DynamicCollection;
	FORCEINLINE UGeometryCollection* GetDynamicCollection() { return DynamicCollection; }
	FORCEINLINE const UGeometryCollection* GetDynamicCollection() const { return DynamicCollection; }

	FORCEINLINE void SetRenderStateDirty() { bRenderStateDirty = true; }

protected:

	/** Populate the static geometry structures for the render thread. */
	void InitConstantData(FGeometryCollectionConstantData* ConstantData);

	/** Populate the dynamic particle data for the render thread. */
	void InitDynamicData(FGeometryCollectionDynamicData* ConstantData);

	/** Reset the dynamic collection from the current rest state. */
	void ResetDynamicCollection();

private : 

	bool bRenderStateDirty;
};
