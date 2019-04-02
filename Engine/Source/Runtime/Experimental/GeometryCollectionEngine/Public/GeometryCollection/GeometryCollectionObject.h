// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Crc.h"
#include "GeometryCollection/ManagedArray.h"
#include "GeometryCollectionObject.generated.h"

class UMaterialInterface;
class UGeometryCollectionCache;
class FGeometryCollection;
class FManagedArrayCollection;
struct FGeometryCollectionSection;

/**
* UGeometryCollectionObject (UObject)
*
* UObject wrapper for the FGeometryCollection
*
*/
UCLASS(customconstructor)
class GEOMETRYCOLLECTIONENGINE_API UGeometryCollection : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UGeometryCollection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UObject Interface */
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	/** End UObject Interface */

	void Serialize(FArchive& Ar);

	/** Accessors for internal geometry collection */
	void GetGeometryCollection(TSharedPtr<FGeometryCollection> GeometryCollectionIn) { GeometryCollection = GeometryCollectionIn; }
	TSharedPtr<FGeometryCollection>       GetGeometryCollection() { return GeometryCollection; }
	const TSharedPtr<FGeometryCollection> GetGeometryCollection() const { return GeometryCollection; }

	void Initialize(FManagedArrayCollection & CollectionIn);
	int32 AppendGeometry(const UGeometryCollection & Element);
	int32 NumElements(const FName & Group);
	void RemoveElements(const FName & Group, const TArray<int32>& SortedDeletionList);

	/** ReindexMaterialSections */
	void ReindexMaterialSections();

	/** appends the standard materials to this uobject */
	void AppendStandardMaterials();

	/** Returns true if there is anything to render */
	bool HasVisibleGeometry();

	/** Invalidates this collection signaling a structural change and renders any previously recorded caches unable to play with this collection */
	void InvalidateCollection();

	/** Accessors for the two guids used to identify this collection */
	FGuid GetIdGuid() const;
	FGuid GetStateGuid() const;

	/** The editable mesh representation of this geometry collection */
	class UObject* EditableMesh;
	
	UPROPERTY()
	TArray<UMaterialInterface*> Materials;

	int GetInteriorMaterialIndex() { return InteriorMaterialIndex; }

	int GetBoneSelectedMaterialIndex() { return BoneSelectedMaterialIndex; }

private:

	/** Guid created on construction of this collection. It should be used to uniquely identify this collection */
	UPROPERTY()
	FGuid PersistentGuid;

	/** 
	 * Guid that can be invalidated on demand - essentially a 'version' that should be changed when a structural change is made to
	 * the geometry collection. This signals to any caches that attempt to link to a geometry collection whether the collection
	 * is still valid (hasn't structurally changed post-recording)
	 */
	UPROPERTY()
	FGuid StateGuid;

	UPROPERTY()
	int32 InteriorMaterialIndex;

	UPROPERTY()
	int32 BoneSelectedMaterialIndex;

	TSharedPtr<FGeometryCollection> GeometryCollection;
};


