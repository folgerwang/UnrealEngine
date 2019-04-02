// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/CoreNet.h"
#include "Components/SceneComponent.h"
#include "ChildActorComponent.generated.h"

class AActor;

USTRUCT()
struct FChildActorAttachedActorInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;
	UPROPERTY()
	FName SocketName;
	UPROPERTY()
	FTransform RelativeTransform;
};

USTRUCT()
struct ENGINE_API FChildActorComponentInstanceData : public FSceneComponentInstanceData
{
	GENERATED_BODY()
public:
	FChildActorComponentInstanceData() = default;
	FChildActorComponentInstanceData(const class UChildActorComponent* Component);

	virtual ~FChildActorComponentInstanceData() = default;

	virtual bool ContainsData() const override;

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// The name of the spawned child actor so it (attempts to) remain constant across construction script reruns
	UPROPERTY()
	FName ChildActorName;

	UPROPERTY()
	TArray<FChildActorAttachedActorInfo> AttachedActors;

	// The component instance data cache for the ChildActor spawned by this component
	TSharedPtr<FComponentInstanceDataCache> ComponentInstanceData;
};

/** A component that spawns an Actor when registered, and destroys it when unregistered.*/
UCLASS(ClassGroup=Utility, hidecategories=(Object,LOD,Physics,Lighting,TextureStreaming,Activation,"Components|Activation",Collision), meta=(BlueprintSpawnableComponent))
class ENGINE_API UChildActorComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category=ChildActorComponent)
	void SetChildActorClass(TSubclassOf<AActor> InClass);

	TSubclassOf<AActor> GetChildActorClass() const { return ChildActorClass; }

	friend class FChildActorComponentDetails;

private:
	/** The class of Actor to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=ChildActorComponent, meta=(OnlyPlaceable, AllowPrivateAccess="true", ForceRebuildProperty="ChildActorTemplate"))
	TSubclassOf<AActor>	ChildActorClass;

	/** The actor that we spawned and own */
	UPROPERTY(Replicated, BlueprintReadOnly, Category=ChildActorComponent, TextExportTransient, NonPIEDuplicateTransient, meta=(AllowPrivateAccess="true"))
	AActor*	ChildActor;

	/** Property to point to the template child actor for details panel purposes */
	UPROPERTY(VisibleDefaultsOnly, DuplicateTransient, Category=ChildActorComponent, meta=(ShowInnerProperties))
	AActor* ChildActorTemplate;

	/** We try to keep the child actor's name as best we can, so we store it off here when destroying */
	FName ChildActorName;

	/** Cached copy of the instance data when the ChildActor is destroyed to be available when needed */
	mutable FChildActorComponentInstanceData* CachedInstanceData;

	/** Flag indicating that when the component is registered that the child actor should be recreated */
	uint8 bNeedsRecreate:1;

public:

	//~ Begin Object Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
	virtual void PostEditUndo() override;
	virtual void PostLoad() override;
#endif
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostRepNotifies() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End Object Interface.

	//~ Begin ActorComponent Interface.
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	virtual void BeginPlay() override;
	//~ End ActorComponent Interface.

	/** Apply the component instance data to the child actor component */
	void ApplyComponentInstanceData(struct FChildActorComponentInstanceData* ComponentInstanceData, const ECacheApplyPhase CacheApplyPhase);

	/** Create the child actor */
	virtual void CreateChildActor();

	AActor* GetChildActor() const { return ChildActor; }
	AActor* GetChildActorTemplate() const { return ChildActorTemplate; }

	FName GetChildActorName() const { return ChildActorName; }

	/** Kill any currently present child actor */
	void DestroyChildActor();
};



