// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AI/NavigationSystemBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavAreaBase.h"
#include "AI/Navigation/NavAgentInterface.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"
#include "AI/NavigationSystemConfig.h"
#include "AI/Navigation/NavigationDataChunk.h"

DEFINE_LOG_CATEGORY(LogNavigation);

#if !UE_BUILD_SHIPPING
#include "CoreGlobals.h"
#include "Misc/ConfigCacheIni.h"
#endif // !UE_BUILD_SHIPPING

namespace FNavigationSystem
{
	void DiscardNavigationDataChunks(UWorld& InWorld)
	{
		const auto& Levels = InWorld.GetLevels();
		for (ULevel* Level : Levels)
		{
			for (UNavigationDataChunk* NavChunk : Level->NavDataChunks)
			{
				if (NavChunk != nullptr)
				{
					NavChunk->MarkPendingKill();
				}
			}
			Level->NavDataChunks.Empty();
		}
	}


	void AddNavigationSystemToWorld(UWorld& WorldOwner, const FNavigationSystemRunMode RunMode, UNavigationSystemConfig* NavigationSystemConfig, const bool bInitializeForWorld)
	{
		if (WorldOwner.GetNavigationSystem() == nullptr)
		{
			if (NavigationSystemConfig == nullptr)
			{
				AWorldSettings* WorldSettings = WorldOwner.GetWorldSettings();
				if (WorldSettings)
				{
					NavigationSystemConfig = WorldSettings->GetNavigationSystemConfig();
				}
			}

			if (NavigationSystemConfig)
			{
				UNavigationSystemBase* NavSysInstance = NavigationSystemConfig->CreateAndConfigureNavigationSystem(WorldOwner);
				WorldOwner.SetNavigationSystem(NavSysInstance);
			}
		}

		if (bInitializeForWorld)
		{
			if (WorldOwner.GetNavigationSystem())
			{
				WorldOwner.GetNavigationSystem()->InitializeForWorld(WorldOwner, RunMode);
			}
			else if (RunMode == FNavigationSystemRunMode::EditorMode)
			{
				DiscardNavigationDataChunks(WorldOwner);
			}
		}
	}
	
	const FNavDataConfig& GetFallbackSupportedAgent() 
	{ 
		static FNavDataConfig FallbackSupportedAgent;
		return FallbackSupportedAgent; 
	}

	bool bWantsComponentChangeNotifies = true;
	
	class FDelegates
	{
	public:
		FActorBasedSignature UpdateActorData;
		FActorComponentBasedSignature UpdateComponentData;
		FSceneComponentBasedSignature UpdateComponentDataAfterMove;
		FActorBasedSignature OnActorBoundsChanged;
		FActorBasedSignature OnPostEditActorMove;
		FSceneComponentBasedSignature OnComponentTransformChanged;
		FActorBasedSignature OnActorRegistered;
		FActorBasedSignature OnActorUnregistered;
		FActorComponentBasedSignature OnComponentRegistered;
		FActorComponentBasedSignature OnComponentUnregistered;
		FActorBasedSignature RemoveActorData;
		FControllerBasedSignature StopMovement;
		FBoolControllerBasedSignature IsFollowingAPath;
		FBoolActorComponentBasedSignature HasComponentData;
		FNavDatConfigBasedSignature GetDefaultSupportedAgent;
		FActorBooleBasedSignature UpdateActorAndComponentData;
		FComponentBoundsChangeSignature OnComponentBoundsChanged;
		//FNavDataForPropsSignature GetNavDataForProps;
		FNavDataForActorSignature GetNavDataForActor;
		FNavDataClassFetchSignature GetDefaultNavDataClass;
		FWorldBoolBasedSignature VerifyNavigationRenderingComponents;
		FWorldBasedSignature Build;
#if WITH_EDITOR
		FWorldBasedSignature OnPIEStart;
		FWorldBasedSignature OnPIEEnd;
		FLevelBasedSignature UpdateLevelCollision;
		FNavigationAutoUpdateEnableSignature SetNavigationAutoUpdateEnable;
		FWorldByteBasedSignature AddNavigationUpdateLock;
		FWorldByteBasedSignature RemoveNavigationUpdateLock;
#endif // WITH_EDITOR

		FDelegates()
		{
			UpdateActorData.BindLambda([](AActor&) {});
			UpdateComponentData.BindLambda([](UActorComponent&) {});
			UpdateComponentDataAfterMove.BindLambda([](UActorComponent&) {});
			OnActorBoundsChanged.BindLambda([](AActor&) {});
			OnPostEditActorMove.BindLambda([](AActor&) {});
			OnComponentTransformChanged.BindLambda([](USceneComponent&) {});
			OnActorRegistered.BindLambda([](AActor&) {});
			OnActorUnregistered.BindLambda([](AActor&) {});
			OnComponentRegistered.BindLambda([](UActorComponent&) {});
			OnComponentUnregistered.BindLambda([](UActorComponent&) {});
			RemoveActorData.BindLambda([](AActor&) {});
			StopMovement.BindLambda([](const AController&) {});
			IsFollowingAPath.BindLambda([](const AController&) { return false; });
			HasComponentData.BindLambda([](UActorComponent&) { return false; });
			GetDefaultSupportedAgent.BindStatic(&GetFallbackSupportedAgent);
			UpdateActorAndComponentData.BindLambda([](AActor&, bool) {});
			OnComponentBoundsChanged.BindLambda([](UActorComponent&, const FBox&, const FBox&) {});
			//GetNavDataForProps.BindLambda([](const FNavAgentProperties&) { return nullptr; });
			GetNavDataForActor.BindLambda([](const AActor&) { return nullptr; });
			GetDefaultNavDataClass.BindLambda([]() { return AActor::StaticClass(); });
			VerifyNavigationRenderingComponents.BindLambda([](UWorld&, bool) {});
			Build.BindLambda([](UWorld&) {});
#if WITH_EDITOR
			OnPIEStart.BindLambda([](UWorld&) {});
			OnPIEEnd.BindLambda([](UWorld&) {});
			UpdateLevelCollision.BindLambda([](ULevel&) {});
			SetNavigationAutoUpdateEnable.BindLambda([](const bool, UNavigationSystemBase*) {});
			AddNavigationUpdateLock.BindLambda([](UWorld&, uint8) {});
			RemoveNavigationUpdateLock.BindLambda([](UWorld&, uint8) {});
#endif // WITH_EDITOR
		}
	};

	FDelegates Delegates;

	void UpdateActorData(AActor& Actor) { Delegates.UpdateActorData.Execute(Actor); }
	void UpdateComponentData(UActorComponent& Comp) { Delegates.UpdateComponentData.Execute(Comp); }
	void UpdateActorAndComponentData(AActor& Actor, bool bUpdateAttachedActors) { Delegates.UpdateActorAndComponentData.Execute(Actor, bUpdateAttachedActors); }
	void UpdateComponentDataAfterMove(USceneComponent& Comp) { Delegates.UpdateComponentDataAfterMove.Execute(Comp); }
	void OnActorBoundsChanged(AActor& Actor) { Delegates.OnActorBoundsChanged.Execute(Actor); }
	void OnPostEditActorMove(AActor& Actor) { Delegates.OnPostEditActorMove.Execute(Actor); }
	void OnComponentBoundsChanged(UActorComponent& Comp, const FBox& NewBounds, const FBox& DirtyArea) { Delegates.OnComponentBoundsChanged.Execute(Comp, NewBounds, DirtyArea); }
	void OnComponentTransformChanged(USceneComponent& Comp) { Delegates.OnComponentTransformChanged.Execute(Comp); }
	void OnActorRegistered(AActor& Actor) { Delegates.OnActorRegistered.Execute(Actor); }
	void OnActorUnregistered(AActor& Actor) { Delegates.OnActorUnregistered.Execute(Actor); }
	void OnComponentRegistered(UActorComponent& Comp) { Delegates.OnComponentRegistered.Execute(Comp); }
	void OnComponentUnregistered(UActorComponent& Comp) { Delegates.OnComponentUnregistered.Execute(Comp); }
	void RemoveActorData(AActor& Actor) { Delegates.RemoveActorData.Execute(Actor); }
	bool HasComponentData(UActorComponent& Comp) { return Delegates.HasComponentData.Execute(Comp);	}
	const FNavDataConfig& GetDefaultSupportedAgent() { return Delegates.GetDefaultSupportedAgent.Execute(); }


	TSubclassOf<UNavAreaBase> DefaultWalkableArea; 
	TSubclassOf<UNavAreaBase> DefaultObstacleArea;
	TSubclassOf<UNavAreaBase> GetDefaultWalkableArea() { return DefaultWalkableArea; }
	TSubclassOf<UNavAreaBase> GetDefaultObstacleArea() { return DefaultObstacleArea; }
		
	bool WantsComponentChangeNotifies()
	{
		return bWantsComponentChangeNotifies;
	}

	//INavigationDataInterface* GetNavDataForProps(const FNavAgentProperties& AgentProperties) { return Delegates.GetNavDataForProps.Execute(AgentProperties); }
	INavigationDataInterface* GetNavDataForActor(const AActor& Actor) { return Delegates.GetNavDataForActor.Execute(Actor); }
	TSubclassOf<AActor> GetDefaultNavDataClass() { return Delegates.GetDefaultNavDataClass.Execute(); }

	void VerifyNavigationRenderingComponents(UWorld& World, const bool bShow) { Delegates.VerifyNavigationRenderingComponents.Execute(World, bShow); }
	void Build(UWorld& World) { Delegates.Build.Execute(World); }

	// pathfollowing
	bool IsFollowingAPath(const AController& Controller) { return Delegates.IsFollowingAPath.Execute(Controller); }
	void StopMovement(const AController& Controller) { Delegates.StopMovement.Execute(Controller); }
	IPathFollowingAgentInterface* FindPathFollowingAgentForActor(const AActor& Actor)
	{
		const TSet<UActorComponent*>& Components = Actor.GetComponents();
		for (UActorComponent* Component : Components)
		{
			IPathFollowingAgentInterface* AsPFAgent = Cast<IPathFollowingAgentInterface>(Component);
			if (AsPFAgent)
			{
				return AsPFAgent;
			}
		}
		return nullptr;
	}

#if WITH_EDITOR
	void OnPIEStart(UWorld& World) { Delegates.OnPIEStart.Execute(World); }
	void OnPIEEnd(UWorld& World) { Delegates.OnPIEEnd.Execute(World); }
	void SetNavigationAutoUpdateEnabled(const bool bNewEnable, UNavigationSystemBase* InNavigationSystem) { Delegates.SetNavigationAutoUpdateEnable.Execute(bNewEnable, InNavigationSystem); }
	void UpdateLevelCollision(ULevel& Level) { Delegates.UpdateLevelCollision.Execute(Level); }
#endif // WITH_EDITOR

	struct FCoordTransforms
	{
		FTransform& Get(const ENavigationCoordSystem::Type FromCoordType, const ENavigationCoordSystem::Type ToCoordType)
		{
			static FTransform CoordTypeTransforms[ENavigationCoordSystem::MAX][ENavigationCoordSystem::MAX] = {
				{FTransform::Identity, FTransform::Identity}
				, {FTransform::Identity, FTransform::Identity}
			};

			return CoordTypeTransforms[uint8(FromCoordType)][uint8(ToCoordType)];
		}
	};

	FCoordTransforms& GetCoordTypeTransforms()
	{
		static FCoordTransforms CoordTypeTransforms;
		return CoordTypeTransforms;
	}

	const FTransform& GetCoordTransformTo(const ENavigationCoordSystem::Type CoordType)
	{
		return GetCoordTransform(ENavigationCoordSystem::Unreal, CoordType);
	}

	const FTransform& GetCoordTransformFrom(const ENavigationCoordSystem::Type CoordType)
	{
		return GetCoordTransform(CoordType, ENavigationCoordSystem::Unreal);
	}

	const FTransform& GetCoordTransform(const ENavigationCoordSystem::Type FromCoordType, const ENavigationCoordSystem::Type ToCoordType)
	{
		return GetCoordTypeTransforms().Get(FromCoordType, ToCoordType);
	}

	UWorld* GetWorldFromContextObject(UObject* WorldContextObject)
	{
		return (WorldContextObject != nullptr)
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
			: nullptr;
	}
}

//----------------------------------------------------------------------//
// FNavigationLockContext                                                                
//----------------------------------------------------------------------//
void FNavigationLockContext::LockUpdates()
{
#if WITH_EDITOR
	bIsLocked = true;

	if (bSingleWorld)
	{
		if (MyWorld)
		{
			FNavigationSystem::Delegates.AddNavigationUpdateLock.Execute(*MyWorld, LockReason);
		}
	}
	else
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World())
			{
				FNavigationSystem::Delegates.AddNavigationUpdateLock.Execute(*Context.World(), LockReason);
			}
		}
	}
#endif
}

void FNavigationLockContext::UnlockUpdates()
{
#if WITH_EDITOR
	if (!bIsLocked)
	{
		return;
	}

	if (bSingleWorld)
	{
		if (MyWorld)
		{
			FNavigationSystem::Delegates.RemoveNavigationUpdateLock.Execute(*MyWorld, LockReason);
		}
	}
	else
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World())
			{
				FNavigationSystem::Delegates.RemoveNavigationUpdateLock.Execute(*Context.World(), LockReason);
			}
		}
	}
#endif
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
UNavigationSystem::UNavigationSystem(const FObjectInitializer& ObjectInitializer)
{
#if !UE_BUILD_SHIPPING
	if (HasAnyFlags(RF_ClassDefaultObject) && GetClass() == UNavigationSystem::StaticClass())
	{
		struct FIniChecker
		{
			FIniChecker()
			{
				const TCHAR EngineTemplage[] = TEXT("/Script/Engine.%s");
				const TCHAR MessageTemplate[] = TEXT("[/Script/Engine.%s] found in the DefaultEngine.ini file. This class has been moved. Please rename that section to [/Script/NavigationSystem.%s]");
				const TArray<FString> MovedIniClasses = {
					TEXT("RecastNavMesh")
					, TEXT("NavArea")
					, TEXT("NavAreaMeta")
					, TEXT("NavArea_Default")
					, TEXT("NavArea_LowHeight")
					, TEXT("NavArea_Null")
					, TEXT("NavArea_Obstacle")
					, TEXT("NavAreaMeta_SwitchByAgent")
					, TEXT("AbstractNavData")
					, TEXT("NavCollision")
					, TEXT("NavigationData")
					, TEXT("NavigationGraph")
					, TEXT("NavigationGraphNode")
					, TEXT("NavigationGraphNodeComponent")
				};

				// NavigationSystem changed name, treat tit separately
				UE_CLOG(GConfig->DoesSectionExist(*FString::Printf(EngineTemplage, TEXT("NavigationSystem")), GEngineIni)
					, LogNavigation, Error, MessageTemplate, TEXT("NavigationSystem"), TEXT("NavigationSystemV1"));

				for (auto ClassName : MovedIniClasses)
				{
					UE_CLOG(GConfig->DoesSectionExist(*FString::Printf(EngineTemplage, *ClassName), GEngineIni)
						, LogNavigation, Error, MessageTemplate, *ClassName, *ClassName);
				}
			}
		};
		static FIniChecker IniChecker;
	}
#endif // !UE_BUILD_SHIPPING
}

void UNavigationSystemBase::SetCoordTransformTo(const ENavigationCoordSystem::Type CoordType, const FTransform& Transform)
{
	SetCoordTransform(ENavigationCoordSystem::Unreal, CoordType, Transform);
}

void UNavigationSystemBase::SetCoordTransformFrom(const ENavigationCoordSystem::Type CoordType, const FTransform& Transform)
{
	SetCoordTransform(CoordType, ENavigationCoordSystem::Unreal, Transform);
}

void UNavigationSystemBase::SetCoordTransform(const ENavigationCoordSystem::Type FromCoordType, const ENavigationCoordSystem::Type ToCoordType, const FTransform& Transform, bool bAddInverse)
{
	FNavigationSystem::GetCoordTypeTransforms().Get(FromCoordType, ToCoordType) = Transform;
	if (bAddInverse)
	{
		FNavigationSystem::GetCoordTypeTransforms().Get(ToCoordType, FromCoordType) = Transform.Inverse();
	}
}

void UNavigationSystemBase::SetWantsComponentChangeNotifies(const bool bEnable)
{
	FNavigationSystem::bWantsComponentChangeNotifies = bEnable;
}

void UNavigationSystemBase::SetDefaultWalkableArea(TSubclassOf<UNavAreaBase> InAreaClass)
{
	FNavigationSystem::DefaultWalkableArea = InAreaClass;
}

void UNavigationSystemBase::SetDefaultObstacleArea(TSubclassOf<UNavAreaBase> InAreaClass)
{
	FNavigationSystem::DefaultObstacleArea = InAreaClass;
}


FNavigationSystem::FActorBasedSignature& UNavigationSystemBase::UpdateActorDataDelegate() { return FNavigationSystem::Delegates.UpdateActorData; }
FNavigationSystem::FActorComponentBasedSignature& UNavigationSystemBase::UpdateComponentDataDelegate() { return FNavigationSystem::Delegates.UpdateComponentData; }
FNavigationSystem::FSceneComponentBasedSignature& UNavigationSystemBase::UpdateComponentDataAfterMoveDelegate() { return FNavigationSystem::Delegates.UpdateComponentDataAfterMove; }
FNavigationSystem::FActorBasedSignature& UNavigationSystemBase::OnActorBoundsChangedDelegate() { return FNavigationSystem::Delegates.OnActorBoundsChanged; }
FNavigationSystem::FActorBasedSignature& UNavigationSystemBase::OnPostEditActorMoveDelegate() { return FNavigationSystem::Delegates.OnPostEditActorMove; }
FNavigationSystem::FSceneComponentBasedSignature& UNavigationSystemBase::OnComponentTransformChangedDelegate() { return FNavigationSystem::Delegates.OnComponentTransformChanged; }
FNavigationSystem::FActorBasedSignature& UNavigationSystemBase::OnActorRegisteredDelegate() { return FNavigationSystem::Delegates.OnActorRegistered; }
FNavigationSystem::FActorBasedSignature& UNavigationSystemBase::OnActorUnregisteredDelegate() { return FNavigationSystem::Delegates.OnActorUnregistered; }
FNavigationSystem::FActorComponentBasedSignature& UNavigationSystemBase::OnComponentRegisteredDelegate() { return FNavigationSystem::Delegates.OnComponentRegistered; }
FNavigationSystem::FActorComponentBasedSignature& UNavigationSystemBase::OnComponentUnregisteredDelegate() { return FNavigationSystem::Delegates.OnComponentUnregistered; }
FNavigationSystem::FActorBasedSignature& UNavigationSystemBase::RemoveActorDataDelegate() { return FNavigationSystem::Delegates.RemoveActorData; }
FNavigationSystem::FBoolActorComponentBasedSignature& UNavigationSystemBase::HasComponentDataDelegate() { return FNavigationSystem::Delegates.HasComponentData; }
FNavigationSystem::FNavDatConfigBasedSignature& UNavigationSystemBase::GetDefaultSupportedAgentDelegate() { return FNavigationSystem::Delegates.GetDefaultSupportedAgent; }
FNavigationSystem::FActorBooleBasedSignature& UNavigationSystemBase::UpdateActorAndComponentDataDelegate() { return FNavigationSystem::Delegates.UpdateActorAndComponentData; }
FNavigationSystem::FComponentBoundsChangeSignature& UNavigationSystemBase::OnComponentBoundsChangedDelegate() { return FNavigationSystem::Delegates.OnComponentBoundsChanged; }
FNavigationSystem::FNavDataForActorSignature& UNavigationSystemBase::GetNavDataForActorDelegate() { return FNavigationSystem::Delegates.GetNavDataForActor; }
FNavigationSystem::FNavDataClassFetchSignature& UNavigationSystemBase::GetDefaultNavDataClassDelegate() { return FNavigationSystem::Delegates.GetDefaultNavDataClass; }
FNavigationSystem::FWorldBoolBasedSignature& UNavigationSystemBase::VerifyNavigationRenderingComponentsDelegate() { return FNavigationSystem::Delegates.VerifyNavigationRenderingComponents; }
FNavigationSystem::FWorldBasedSignature& UNavigationSystemBase::BuildDelegate() { return FNavigationSystem::Delegates.Build; }
#if WITH_EDITOR
FNavigationSystem::FWorldBasedSignature& UNavigationSystemBase::OnPIEStartDelegate() { return FNavigationSystem::Delegates.OnPIEStart; }
FNavigationSystem::FWorldBasedSignature& UNavigationSystemBase::OnPIEEndDelegate() { return FNavigationSystem::Delegates.OnPIEEnd; }
FNavigationSystem::FLevelBasedSignature& UNavigationSystemBase::UpdateLevelCollisionDelegate() { return FNavigationSystem::Delegates.UpdateLevelCollision; }
FNavigationSystem::FNavigationAutoUpdateEnableSignature& UNavigationSystemBase::SetNavigationAutoUpdateEnableDelegate() { return FNavigationSystem::Delegates.SetNavigationAutoUpdateEnable; }
FNavigationSystem::FWorldByteBasedSignature& UNavigationSystemBase::AddNavigationUpdateLockDelegate() { return FNavigationSystem::Delegates.AddNavigationUpdateLock; }
FNavigationSystem::FWorldByteBasedSignature& UNavigationSystemBase::RemoveNavigationUpdateLockDelegate() { return FNavigationSystem::Delegates.RemoveNavigationUpdateLock; }
#endif // WITH_EDITOR
//----------------------------------------------------------------------//
// IPathFollowingManagerInterface
//----------------------------------------------------------------------//
FNavigationSystem::FControllerBasedSignature& IPathFollowingManagerInterface::StopMovementDelegate() { return FNavigationSystem::Delegates.StopMovement; }
FNavigationSystem::FBoolControllerBasedSignature& IPathFollowingManagerInterface::IsFollowingAPathDelegate() { return FNavigationSystem::Delegates.IsFollowingAPath; }
