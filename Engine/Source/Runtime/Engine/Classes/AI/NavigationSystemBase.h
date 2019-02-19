// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/NavigationModifier.h"
#include "Engine/World.h"
#include "NavigationSystemBase.generated.h"

class UNavigationSystemBase;
class UNavigationSystemConfig;
class AActor;
class UActorComponent;
class USceneComponent;
class INavigationDataInterface;
class IPathFollowingAgentInterface;
class AWorldSettings;
class ULevel;
class AController;
class UNavAreaBase;

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogNavigation, Warning, All);

UENUM()
enum class FNavigationSystemRunMode : uint8
{
	InvalidMode,
	GameMode,
	EditorMode,
	SimulationMode,
	PIEMode,
};

namespace ENavigationLockReason
{
	enum Type
	{
		Unknown = 1 << 0,
		AllowUnregister = 1 << 1,

		MaterialUpdate = 1 << 2,
		LightingUpdate = 1 << 3,
		ContinuousEditorMove = 1 << 4,
		SpawnOnDragEnter = 1 << 5,
	};
}

class ENGINE_API FNavigationLockContext
{
public:
	FNavigationLockContext(ENavigationLockReason::Type Reason = ENavigationLockReason::Unknown, bool bApplyLock = true)
		: MyWorld(NULL), LockReason(Reason), bSingleWorld(false), bIsLocked(false)
	{
		if (bApplyLock)
		{
			LockUpdates();
		}
	}

	FNavigationLockContext(UWorld* InWorld, ENavigationLockReason::Type Reason = ENavigationLockReason::Unknown, bool bApplyLock = true)
		: MyWorld(InWorld), LockReason(Reason), bSingleWorld(true), bIsLocked(false)
	{
		if (bApplyLock)
		{
			LockUpdates();
		}
	}

	~FNavigationLockContext()
	{
		UnlockUpdates();
	}

private:
	UWorld* MyWorld;
	uint8 LockReason;
	uint8 bSingleWorld : 1;
	uint8 bIsLocked : 1;

	void LockUpdates();
	void UnlockUpdates();
};

namespace FNavigationSystem
{
	/** Creates an instance of NavigationSystem (class being specified by WorldSetting's NavigationSystemConfig)
	 *	A new instance will be created only if given WorldOwner doesn't have one yet.
	 *	The new instance will be assigned to the given WorldOwner (via SetNavigationSystem 
	 *	call) and depending on value of bInitializeForWorld the InitializeForWorld 
	 *	function will be called on the new NavigationSystem instance.
	 *	(@see UWorld.NavigationSystem)
	 *	@param RunMode if set to a valid value (other than FNavigationSystemRunMode::InvalidMode) 
	 *		will also configure the created NavigationSystem instance for that mode
	 *	@param NavigationSystemConfig is used to pick the navigation system's class and set it up. If null
	 *		then WorldOwner.WorldSettings.NavigationSystemConfig will be used
	 */
	ENGINE_API void AddNavigationSystemToWorld(UWorld& WorldOwner, const FNavigationSystemRunMode RunMode = FNavigationSystemRunMode::InvalidMode, UNavigationSystemConfig* NavigationSystemConfig = nullptr, const bool bInitializeForWorld = true);

	/** Discards all navigation data chunks in all sub-levels */
	ENGINE_API void DiscardNavigationDataChunks(UWorld& InWorld);

	template<typename TNavSys>
	FORCEINLINE TNavSys* GetCurrent(UWorld* World)
	{
		return World ? Cast<TNavSys>(World->GetNavigationSystem()) : (TNavSys*)nullptr;
	}

	template<typename TNavSys>
	FORCEINLINE const TNavSys* GetCurrent(const UWorld* World)
	{
		return World ? Cast<TNavSys>(World->GetNavigationSystem()) : (const TNavSys*)nullptr;
	}

	ENGINE_API UWorld* GetWorldFromContextObject(UObject* WorldContextObject);

	template<typename TNavSys>
	TNavSys* GetCurrent(UObject* WorldContextObject)
	{
		UWorld* World = GetWorldFromContextObject(WorldContextObject);
		return GetCurrent<TNavSys>(World);
	}

	ENGINE_API void UpdateActorData(AActor& Actor);
	ENGINE_API void UpdateComponentData(UActorComponent& Comp);
	ENGINE_API void UpdateActorAndComponentData(AActor& Actor, bool bUpdateAttachedActors = true);
	ENGINE_API void UpdateComponentDataAfterMove(USceneComponent& Comp);
	//ENGINE_API bool HasComponentData(UActorComponent& Comp);
	ENGINE_API void OnActorBoundsChanged(AActor& Actor);
	ENGINE_API void OnPostEditActorMove(AActor& Actor);
	ENGINE_API void OnComponentBoundsChanged(UActorComponent& Comp, const FBox& NewBounds, const FBox& DirtyArea);
	ENGINE_API void OnComponentTransformChanged(USceneComponent& Comp);

	ENGINE_API void OnActorRegistered(AActor& Actor);
	ENGINE_API void OnActorUnregistered(AActor& Actor);

	ENGINE_API void OnComponentRegistered(UActorComponent& Comp);
	ENGINE_API void OnComponentUnregistered(UActorComponent& Comp);

	ENGINE_API void RemoveActorData(AActor& Actor);

	ENGINE_API bool HasComponentData(UActorComponent& Comp);
	
	ENGINE_API const FNavDataConfig& GetDefaultSupportedAgent();

	ENGINE_API TSubclassOf<UNavAreaBase> GetDefaultWalkableArea();
	ENGINE_API TSubclassOf<UNavAreaBase> GetDefaultObstacleArea();

	/**	Retrieves the transform the Navigation System is using to convert coords
	 *	from FromCoordType to ToCoordType */
	ENGINE_API const FTransform& GetCoordTransform(const ENavigationCoordSystem::Type FromCoordType, const ENavigationCoordSystem::Type ToCoordType);
	UE_DEPRECATED(4.22, "FNavigationSystem::GetCoordTransformTo is deprecated. Use FNavigationSystem::GetCoordTransform instead")
	ENGINE_API const FTransform& GetCoordTransformTo(const ENavigationCoordSystem::Type CoordType);
	UE_DEPRECATED(4.22, "FNavigationSystem::GetCoordTransformFrom is deprecated. Use FNavigationSystem::GetCoordTransform instead")
	ENGINE_API const FTransform& GetCoordTransformFrom(const ENavigationCoordSystem::Type CoordType);

	ENGINE_API bool WantsComponentChangeNotifies();

	//ENGINE_API INavigationDataInterface* GetNavDataForProps(const FNavAgentProperties& AgentProperties);
	ENGINE_API INavigationDataInterface* GetNavDataForActor(const AActor& Actor);
	ENGINE_API TSubclassOf<AActor> GetDefaultNavDataClass();

	ENGINE_API void VerifyNavigationRenderingComponents(UWorld& World, const bool bShow);
	ENGINE_API void Build(UWorld& World);

#if WITH_EDITOR
	ENGINE_API void OnPIEStart(UWorld& World);
	ENGINE_API void OnPIEEnd(UWorld& World);
	ENGINE_API void SetNavigationAutoUpdateEnabled(const bool bNewEnable, UNavigationSystemBase* InNavigationSystem);
	ENGINE_API void UpdateLevelCollision(ULevel& Level);
#endif // WITH_EDITOR

	enum class ECleanupMode : uint8
	{
		CleanupWithWorld,
		CleanupUnsafe,
	};

	// pathfollowing
	ENGINE_API bool IsFollowingAPath(const AController& Controller);
	ENGINE_API void StopMovement(const AController& Controller);
	ENGINE_API IPathFollowingAgentInterface* FindPathFollowingAgentForActor(const AActor& Actor);

	DECLARE_DELEGATE_OneParam(FActorBasedSignature, AActor& /*Actor*/);
	DECLARE_DELEGATE_OneParam(FActorComponentBasedSignature, UActorComponent& /*Comp*/);
	DECLARE_DELEGATE_OneParam(FSceneComponentBasedSignature, USceneComponent& /*Comp*/);
	DECLARE_DELEGATE_OneParam(FWorldBasedSignature, UWorld& /*World*/);
	DECLARE_DELEGATE_OneParam(FLevelBasedSignature, ULevel& /*Level*/);
	DECLARE_DELEGATE_OneParam(FControllerBasedSignature, const AController& /*Controller*/);
	DECLARE_DELEGATE_TwoParams(FNavigationAutoUpdateEnableSignature, const bool /*bNewEnable*/, UNavigationSystemBase* /*InNavigationSystem*/);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FBoolControllerBasedSignature, const AController& /*Controller*/);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FBoolActorComponentBasedSignature, UActorComponent& /*Comp*/);
	DECLARE_DELEGATE_RetVal(TSubclassOf<UNavAreaBase>, FNavAreaBasedSignature);
	DECLARE_DELEGATE_RetVal(const FNavDataConfig&, FNavDatConfigBasedSignature);
	DECLARE_DELEGATE_TwoParams(FWorldByteBasedSignature, UWorld& /*World*/, uint8 /*Flags*/);
	DECLARE_DELEGATE_TwoParams(FActorBooleBasedSignature, AActor& /*Actor*/, bool /*bUpdateAttachedActors*/);
	DECLARE_DELEGATE_ThreeParams(FComponentBoundsChangeSignature, UActorComponent& /*Comp*/, const FBox& /*NewBounds*/, const FBox& /*DirtyArea*/)
	DECLARE_DELEGATE_RetVal_OneParam(INavigationDataInterface*, FNavDataForPropsSignature, const FNavAgentProperties& /*AgentProperties*/);
	DECLARE_DELEGATE_RetVal_OneParam(INavigationDataInterface*, FNavDataForActorSignature, const AActor& /*Actor*/);
	DECLARE_DELEGATE_RetVal(TSubclassOf<AActor>, FNavDataClassFetchSignature);
	DECLARE_DELEGATE_TwoParams(FWorldBoolBasedSignature, UWorld& /*World*/, const bool /*bShow*/);
}


UCLASS(Abstract, config = Engine, defaultconfig, Transient)
class ENGINE_API UNavigationSystemBase : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UNavigationSystemBase(){}

	virtual void Tick(float DeltaSeconds) PURE_VIRTUAL(UNavigationSystemBase::Tick, );
	virtual void CleanUp(const FNavigationSystem::ECleanupMode Mode) PURE_VIRTUAL(UNavigationSystemBase::CleanUp, );
	virtual void Configure(const UNavigationSystemConfig& Config) PURE_VIRTUAL(UNavigationSystemBase::Configure, );

	/**
	*	Called when owner-UWorld initializes actors
	*/
	virtual void OnInitializeActors() {}

	virtual bool IsNavigationBuilt(const AWorldSettings* Settings) const { return false; }

	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) PURE_VIRTUAL(UNavigationSystemBase::ApplyWorldOffset, );

	virtual void InitializeForWorld(UWorld& World, FNavigationSystemRunMode Mode) PURE_VIRTUAL(UNavigationSystemBase::InitializeForWorld, );

	/** 
	 *	If you're using NavigationSysstem module consider calling 
	 *	FNavigationSystem::GetCurrent<UNavigationSystemV1>()->GetDefaultNavDataInstance 
	 *	instead.
	 */
	virtual INavigationDataInterface* GetMainNavData() const { return nullptr; }

	UE_DEPRECATED(4.20, "GetMainNavData is deprecated. Use FNavigationSystem::GetCurrent<UNavigationSystemV1>()->GetDefaultNavDataInstance instead")
	INavigationDataInterface* GetMainNavData(int) { return nullptr; }

protected:
	/**	Sets the Transform the Navigation System will use when converting from FromCoordType
	 *	to ToCoordType
	 *	@param bAddInverse if true (default) will also set coord transform in 
	 *		the reverse order using Transform.Inverse() */
	static void SetCoordTransform(const ENavigationCoordSystem::Type FromCoordType, const ENavigationCoordSystem::Type ToCoordType, const FTransform& Transform, bool bAddInverse = true);
	UE_DEPRECATED(4.22, "FNavigationSystem::SetCoordTransformTo is deprecated. Use FNavigationSystem::SetCoordTransform instead")
	static void SetCoordTransformTo(const ENavigationCoordSystem::Type CoordType, const FTransform& Transform);
	UE_DEPRECATED(4.22, "FNavigationSystem::SetCoordTransformFrom is deprecated. Use FNavigationSystem::SetCoordTransform instead")
	static void SetCoordTransformFrom(const ENavigationCoordSystem::Type CoordType, const FTransform& Transform);
	static void SetWantsComponentChangeNotifies(const bool bEnable);
	static void SetDefaultWalkableArea(TSubclassOf<UNavAreaBase> InAreaClass);
	static void SetDefaultObstacleArea(TSubclassOf<UNavAreaBase> InAreaClass);

	static FNavigationSystem::FActorBasedSignature& UpdateActorDataDelegate();
	static FNavigationSystem::FActorComponentBasedSignature& UpdateComponentDataDelegate();
	static FNavigationSystem::FSceneComponentBasedSignature& UpdateComponentDataAfterMoveDelegate();
	static FNavigationSystem::FActorBasedSignature& OnActorBoundsChangedDelegate();
	static FNavigationSystem::FActorBasedSignature& OnPostEditActorMoveDelegate();
	static FNavigationSystem::FSceneComponentBasedSignature& OnComponentTransformChangedDelegate();
	static FNavigationSystem::FActorBasedSignature& OnActorRegisteredDelegate();
	static FNavigationSystem::FActorBasedSignature& OnActorUnregisteredDelegate();
	static FNavigationSystem::FActorComponentBasedSignature& OnComponentRegisteredDelegate();
	static FNavigationSystem::FActorComponentBasedSignature& OnComponentUnregisteredDelegate();
	static FNavigationSystem::FActorBasedSignature& RemoveActorDataDelegate();
	static FNavigationSystem::FBoolActorComponentBasedSignature& HasComponentDataDelegate();
	static FNavigationSystem::FNavDatConfigBasedSignature& GetDefaultSupportedAgentDelegate();
	static FNavigationSystem::FActorBooleBasedSignature& UpdateActorAndComponentDataDelegate();
	static FNavigationSystem::FComponentBoundsChangeSignature& OnComponentBoundsChangedDelegate();
	static FNavigationSystem::FNavDataForActorSignature& GetNavDataForActorDelegate();
	static FNavigationSystem::FNavDataClassFetchSignature& GetDefaultNavDataClassDelegate();
	static FNavigationSystem::FWorldBoolBasedSignature& VerifyNavigationRenderingComponentsDelegate();
	static FNavigationSystem::FWorldBasedSignature& BuildDelegate();
#if WITH_EDITOR
	static FNavigationSystem::FWorldBasedSignature& OnPIEStartDelegate();
	static FNavigationSystem::FWorldBasedSignature& OnPIEEndDelegate();
	static FNavigationSystem::FLevelBasedSignature& UpdateLevelCollisionDelegate();
	static FNavigationSystem::FNavigationAutoUpdateEnableSignature& SetNavigationAutoUpdateEnableDelegate();
	static FNavigationSystem::FWorldByteBasedSignature& AddNavigationUpdateLockDelegate();
	static FNavigationSystem::FWorldByteBasedSignature& RemoveNavigationUpdateLockDelegate();
#endif // WITH_EDITOR
};


class ENGINE_API IPathFollowingManagerInterface
{
protected:
	static FNavigationSystem::FControllerBasedSignature& StopMovementDelegate();
	static FNavigationSystem::FBoolControllerBasedSignature& IsFollowingAPathDelegate();
};


UCLASS(Abstract, config = Engine, defaultconfig)
class ENGINE_API UNavigationSystem : public UObject
{
	GENERATED_BODY()
public:
	UNavigationSystem(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UE_DEPRECATED(4.20, "UpdateActorInNavOctree is deprecated. Use FNavigationSystem::RemoveActorData instead")
	static void ClearNavOctreeAll(AActor* Actor);
	UE_DEPRECATED(4.20, "UpdateActorInNavOctree is deprecated. Use FNavigationSystem::UpdateActorData instead")
	static void UpdateActorInNavOctree(AActor& Actor);
	UE_DEPRECATED(4.20, "UpdateComponentInNavOctree is deprecated. Use FNavigationSystem::UpdateComponentData instead")
	static void UpdateComponentInNavOctree(UActorComponent& Comp);
	UE_DEPRECATED(4.20, "UpdateActorAndComponentsInNavOctree is deprecated. Use FNavigationSystem::UpdateActorAndComponentData instead")
	static void UpdateActorAndComponentsInNavOctree(AActor& Actor, bool bUpdateAttachedActors = true);
	UE_DEPRECATED(4.20, "UpdateNavOctreeAfterMove is deprecated. Use FNavigationSystem::UpdateComponentDataAfterMove instead")
	static void UpdateNavOctreeAfterMove(USceneComponent* Comp);
	UE_DEPRECATED(4.20, "UpdateNavOctreeBounds is deprecated. Use FNavigationSystem::OnActorBoundsChanged instead")
	static void UpdateNavOctreeBounds(AActor* Actor);
	UE_DEPRECATED(4.20, "InitializeForWorld is deprecated. Use FNavigationSystem::CreateNavigationSystem instead")
	static void InitializeForWorld(UWorld* World, FNavigationSystemRunMode Mode);
	UE_DEPRECATED(4.20, "CreateNavigationSystem is deprecated. Use FNavigationSystem::CreateNavigationSystem instead")
	static UNavigationSystem* CreateNavigationSystem(UWorld* WorldOwner);
	UE_DEPRECATED(4.20, "OnComponentRegistered is deprecated. Use FNavigationSystem::OnComponentRegistered instead")
	static void OnComponentRegistered(UActorComponent* Comp);
	UE_DEPRECATED(4.20, "OnComponentUnregistered is deprecated. Use FNavigationSystem::OnComponentUnregistered instead")
	static void OnComponentUnregistered(UActorComponent* Comp);
	UE_DEPRECATED(4.20, "OnActorRegistered is deprecated. Use FNavigationSystem::OnActorRegistered instead")
	static void OnActorRegistered(AActor* Actor);
	UE_DEPRECATED(4.20, "OnActorUnregistered is deprecated. Use FNavigationSystem::OnActorUnregistered instead")
	static void OnActorUnregistered(AActor* Actor);
	UE_DEPRECATED(4.20, "GetCurrent is deprecated. Use FNavigationSystem::GetCurrent instead")
	static UNavigationSystem* GetCurrent(UWorld* World);
	UE_DEPRECATED(4.20, "GetCurrent is deprecated. Use FNavigationSystem::GetCurrent instead")
	static UNavigationSystem* GetCurrent(UObject* WorldContextObject);
	UE_DEPRECATED(4.20, "ShouldUpdateNavOctreeOnComponentChange is deprecated. Use FNavigationSystem::WantsComponentChangeNotifies instead")
	static bool ShouldUpdateNavOctreeOnComponentChange();
	UE_DEPRECATED(4.20, "GetDefaultWalkableArea is deprecated. Use FNavigationSystem::GetDefaultWalkableArea instead")
	static TSubclassOf<UNavAreaBase> GetDefaultWalkableArea();
	UE_DEPRECATED(4.20, "UNavigationSystem::GetNavDataForProps is deprecated. Use FNavigationSystem::GetDefaultObstacleArea instead")
	static TSubclassOf<UNavAreaBase> GetDefaultObstacleArea();
	UE_DEPRECATED(4.20, "UNavigationSystem::K2_GetRandomReachablePointInRadius is deprecated. Use UNavigationSystemV1::K2_GetRandomReachablePointInRadius instead")
	static bool K2_GetRandomReachablePointInRadius(UObject* WorldContextObject, const FVector& Origin, FVector& RandomLocation, float Radius, UObject* NavData = NULL, TSubclassOf<UObject> FilterClass = NULL) { return false; }
	UE_DEPRECATED(4.20, "UNavigationSystem::SimpleMoveToActor is deprecated. Use UAIBlueprintHelperLibrary::SimpleMoveToActor instead")
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta = (DisplayName = "SimpleMoveToActor_DEPRECATED", ScriptNoExport, DeprecatedFunction, DeprecationMessage = "SimpleMoveToActor is deprecated. Use AIBlueprintHelperLibrary::SimpleMoveToActor instead"))
	static void SimpleMoveToActor(AController* Controller, const AActor* Goal) {}
	UE_DEPRECATED(4.20, "UNavigationSystem::SimpleMoveToLocation is deprecated. Use UAIBlueprintHelperLibrary::SimpleMoveToLocation instead")
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta = (DisplayName = "SimpleMoveToLocation_DEPRECATED", ScriptNoExport, DeprecatedFunction, DeprecationMessage = "SimpleMoveToLocation is deprecated. Use AIBlueprintHelperLibrary::SimpleMoveToLocation instead"))
	static void SimpleMoveToLocation(AController* Controller, const FVector& Goal) {}

	UE_DEPRECATED(4.20, "UpdateNavOctreeElementBounds is deprecated. Use FNavigationSystem::OnComponentBoundsChanged instead")
	bool UpdateNavOctreeElementBounds(UActorComponent* Comp, const FBox& NewBounds, const FBox& DirtyArea);

	UE_DEPRECATED(4.20, "GetObjectsNavOctreeId is deprecated. You need to access NavigationSystem's modul NavigationSystem to access this functionality")
	const class FOctreeElementId* GetObjectsNavOctreeId(const UObject* Object) const { return nullptr; }

	UE_DEPRECATED(4.20, "HasPendingObjectNavOctreeId is deprecated. You need to access NavigationSystem's modul NavigationSystem class to access this functionality")
	bool HasPendingObjectNavOctreeId(UObject* Object) const { return false; }

	UE_DEPRECATED(4.20, "GetDefaultSupportedAgentConfig is deprecated. Use FNavigationSystem::GetDefaultSupportedAgent instead")
	const FNavDataConfig& GetDefaultSupportedAgentConfig() const;
	
	UE_DEPRECATED(4.20, "GetNavDataForProps is deprecated. Use FNavigationSystem::GetNavDataForProps instead")
	INavigationDataInterface* GetNavDataForProps(const FNavAgentProperties& AgentProperties);

	UE_DEPRECATED(4.20, "GetSupportedAgents has been moved. Use UNavigationSystemV1::GetSupportedAgents instead")
	const TArray<FNavDataConfig>& GetSupportedAgents() const { return FakeSupportedAgents; }

	UE_DEPRECATED(4.20, "GetMainNavData has been moved. Use FNavigationSystem::GetCurrent<UNavigationSystemV1>()->GetDefaultNavDataInstance instead")
	UObject* GetMainNavData(int) { return nullptr; }
	UE_DEPRECATED(4.20, "GetMainNavData has been moved. Use FNavigationSystem::GetCurrent<UNavigationSystemV1>()->GetDefaultNavDataInstance instead")
	UObject* GetMainNavData() const { return nullptr; }

	UE_DEPRECATED(4.20, "ProjectPointToNavigation has been deprecated along with the whole UNavigationSystem class. Use UNavigationSystemV1 instead")
	bool ProjectPointToNavigation(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent = INVALID_NAVEXTENT, const UObject* NavData = NULL, void* QueryFilter = NULL) const { return false; }

private:
	const TArray<FNavDataConfig> FakeSupportedAgents;
};