// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LODActor.h"
#include "HLODProxy.generated.h"

class UStaticMesh;

/** A mesh proxy entry */
USTRUCT()
struct FHLODProxyMesh
{
	GENERATED_BODY()

	FHLODProxyMesh()
		: StaticMesh(nullptr)
	{
	}

#if WITH_EDITOR
	FHLODProxyMesh(ALODActor* InLODActor, UStaticMesh* InStaticMesh, const FName& InKey)
		: LODActor(InLODActor)
		, StaticMesh(InStaticMesh)
		, Key(InKey)

	{
	}

	bool operator==(const FHLODProxyMesh& InHLODProxyMesh) const
	{
		return LODActor == InHLODProxyMesh.LODActor &&
			   StaticMesh == InHLODProxyMesh.StaticMesh &&
			   Key == InHLODProxyMesh.Key;
	}
#endif

	/** Get the mesh for this proxy mesh */
	const UStaticMesh* GetStaticMesh() const { return StaticMesh; }

	/** Get the actor for this proxy mesh */
	const TLazyObjectPtr<ALODActor>& GetLODActor() const { return LODActor; }

	/** Get the key for this proxy mesh */
	const FName& GetKey() const { return Key; }

private:
	/** The ALODActor that we were generated from */
	UPROPERTY(VisibleAnywhere, Category = "Proxy Mesh")
	TLazyObjectPtr<ALODActor> LODActor;

	/** The mesh used to display this proxy */
	UPROPERTY(VisibleAnywhere, Category = "Proxy Mesh")
	UStaticMesh* StaticMesh;

	/** The key generated from an ALODActor. If this differs from that generated from the ALODActor, then the mesh needs regenerating. */
	UPROPERTY(VisibleAnywhere, Category = "Proxy Mesh")
	FName Key;
};

/** This asset acts as a proxy to a static mesh for ALODActors to display */
UCLASS()
class ENGINE_API UHLODProxy : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/** Setup the map - only called at initial construction */
	void SetMap(const UWorld* InMap);

	/** Adds a static mesh and the key used to generate it */
	void AddMesh(ALODActor* InLODActor, UStaticMesh* InStaticMesh, const FName& InKey);

	/** Clean out invalid proxy mesh entries */
	void Clean();

	/** Helper for recursive traversing LODActors to retrieve a semi deterministic first AActor for resulting asset naming */
	static const AActor* FindFirstActor(const ALODActor* LODActor);

	/**
	 * Recursively retrieves StaticMeshComponents from a LODActor and its child LODActors
	 *
	 * @param Actor - LODActor instance
	 * @param InOutComponents - Will hold the StaticMeshComponents
	 */
	static void ExtractStaticMeshComponentsFromLODActor(const ALODActor* LODActor, TArray<UStaticMeshComponent*>& InOutComponents);

	/** Extract components that we would use for LOD generation. Used to generate keys for LOD actors. */
	static void ExtractComponents(const ALODActor* LODActor, TArray<UPrimitiveComponent*>& InOutComponents);

	/** Build a unique key for the LOD actor, used to determine if the actor needs rebuilding */
	static FName GenerateKeyForActor(const ALODActor* LODActor);
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Check if we contain data for the specified actor */
	bool ContainsDataForActor(const ALODActor* InLODActor) const;
#endif

private:
#if WITH_EDITORONLY_DATA
	/** Keep hold of the level in the editor to allow for package cleaning etc. */
	UPROPERTY(VisibleAnywhere, Category = "Proxy Mesh")
	TSoftObjectPtr<UWorld> OwningMap;
#endif

	/** All the mesh proxies we contain */
	UPROPERTY(VisibleAnywhere, Category = "Proxy Mesh")
	TArray<FHLODProxyMesh> ProxyMeshes;
};