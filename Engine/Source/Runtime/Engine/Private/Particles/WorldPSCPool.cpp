// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Particles/WorldPSCPool.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Particles/ParticleSystemComponent.h"
#include "ParticleHelper.h"

static float GParticleSystemPoolKillUnusedTime = 180.0f;
static FAutoConsoleVariableRef ParticleSystemPoolKillUnusedTime(
	TEXT("FX.ParticleSystemPool.KillUnusedTime"),
	GParticleSystemPoolKillUnusedTime,
	TEXT("How long a pooled particle component needs to be unused for before it is destroyed.")
);

static int32 GbEnableParticleSystemPooling = 1;
static FAutoConsoleVariableRef bEnableParticleSystemPooling(
	TEXT("FX.ParticleSystemPool.Enable"),
	GbEnableParticleSystemPooling,
	TEXT("How many Particle System Components to preallocate when creating new ones for the pool.")
);

static float GParticleSystemPoolingCleanTime = 30.0f;
static FAutoConsoleVariableRef ParticleSystemPoolingCleanTime(
	TEXT("FX.ParticleSystemPool.CleanTime"),
	GParticleSystemPoolingCleanTime,
	TEXT("How often should the pool be cleaned (in seconds).")
);

FPSCPool::FPSCPool()
	: MaxUsed(0)
{

}

void FPSCPool::Cleanup()
{
	for (FPSCPoolElem& Elem : FreeElements)
	{
		if (Elem.PSC)
		{
			Elem.PSC->PoolingMethod = EPSCPoolMethod::None;//Reset so we don't trigger warnings about destroying pooled PSCs.
			Elem.PSC->DestroyComponent();
		}
		else
		{
			UE_LOG(LogParticles, Error, TEXT("Free element in the WorldPSCPool was null. Someone must be keeping a reference to a PSC that has been freed to the pool and then are manually destroying it."));
		}
	}

	for (UParticleSystemComponent* PSC : InUseComponents_Auto)
	{
		//It's possible for people to manually destroy these so we have to guard against it. Though we warn about it in UParticleSystemComponent::BeginDestroy
		if (PSC)
		{
			PSC->PoolingMethod = EPSCPoolMethod::None;//Reset so we don't trigger warnings about destroying pooled PSCs.
			PSC->DestroyComponent();
		}
	}

	//Warn if there are any manually released PSCs still in the world at cleanup time.
	for (UParticleSystemComponent* PSC : InUseComponents_Manual)
	{
		//It's possible for people to manually destroy these so we have to guard against it. Though we warn about it in UParticleSystemComponent::BeginDestroy
		if (PSC)
		{
			UE_LOG(LogParticles, Warning, TEXT("Pooled PSC set to manual release is still in use as the pool is being cleaned up. %s"), *PSC->Template->GetFullName());
			PSC->PoolingMethod = EPSCPoolMethod::None;//Reset so we don't trigger warnings about destroying pooled PSCs.
			PSC->DestroyComponent();
		}
	}

	FreeElements.Empty();
	InUseComponents_Auto.Empty();
	InUseComponents_Manual.Empty();
}

UParticleSystemComponent* FPSCPool::Acquire(UWorld* World, UParticleSystem* Template, EPSCPoolMethod PoolingMethod)
{
	check(GbEnableParticleSystemPooling);
	check(PoolingMethod != EPSCPoolMethod::None);

	FPSCPoolElem RetElem;
	if (FreeElements.Num())
	{
		RetElem = FreeElements.Pop(false);
		check(RetElem.PSC->Template == Template);
		check(!RetElem.PSC->IsPendingKill());

		if (RetElem.PSC->GetWorld() != World)
		{
			// Rename the PSC to move it into the current PersistentLevel - it may have been spawned in one
			// level but is now needed in another level.
			// Use the REN_ForceNoResetLoaders flag to prevent the rename from potentially calling FlushAsyncLoading.
			RetElem.PSC->Rename(nullptr, World, REN_ForceNoResetLoaders);
		}
	}
	else
	{
		//None in the pool so create a new one.
		RetElem.PSC = NewObject<UParticleSystemComponent>(World);
		RetElem.PSC->bAutoDestroy = false;//<<< We don't auto destroy. We'll just periodically clear up the pool.
		RetElem.PSC->SecondsBeforeInactive = 0.0f;
		RetElem.PSC->bAutoActivate = false;
		RetElem.PSC->SetTemplate(Template);
		RetElem.PSC->bOverrideLODMethod = false;
		RetElem.PSC->bAllowRecycling = true;
	}

	RetElem.PSC->PoolingMethod = PoolingMethod;

#if ENABLE_PSC_POOL_DEBUGGING
	if (PoolingMethod == EPSCPoolMethod::AutoRelease)
	{
		InUseComponents_Auto.Add(RetElem.PSC);
	}
	else if (PoolingMethod == EPSCPoolMethod::ManualRelease)
	{
		InUseComponents_Manual.Add(RetElem.PSC);
	}
#endif 

	MaxUsed = FMath::Max(MaxUsed, InUseComponents_Manual.Num() + InUseComponents_Auto.Num());

	//UE_LOG(LogParticles, Log, TEXT("FPSCPool::Acquire() - World: %p - PSC: %p - Sys: %s"), World, RetElem.PSC, *Template->GetFullName());

	return RetElem.PSC;
}

void FPSCPool::Reclaim(UParticleSystemComponent* PSC, const float CurrentTimeSeconds)
{
	check(PSC);

	//UE_LOG(LogParticles, Log, TEXT("FPSCPool::Reclaim() - World: %p - PSC: %p - Sys: %s"), PSC->GetWorld(), PSC, *PSC->Template->GetFullName());

#if ENABLE_PSC_POOL_DEBUGGING
	int32 InUseIdx = INDEX_NONE;
	if (PSC->PoolingMethod == EPSCPoolMethod::AutoRelease)
	{
		InUseIdx = InUseComponents_Auto.IndexOfByKey(PSC);
		if (InUseIdx != INDEX_NONE)
		{
			InUseComponents_Auto.RemoveAtSwap(InUseIdx);
		}
	}
	else if (PSC->PoolingMethod == EPSCPoolMethod::ManualRelease)
	{
		InUseIdx = InUseComponents_Manual.IndexOfByKey(PSC);
		if (InUseIdx != INDEX_NONE)
		{
			InUseComponents_Manual.RemoveAtSwap(InUseIdx);
		}
	}
	
	if(InUseIdx == INDEX_NONE)
	{
		UE_LOG(LogParticles, Error, TEXT("World Particle System Pool is reclaiming a component that is not in it's InUse list!"));
	}
#endif

	//Don't add back to the pool if we're no longer pooling or we've hit our max resident pool size.
	if (GbEnableParticleSystemPooling != 0 && FreeElements.Num() < (int32)PSC->Template->MaxPoolSize)
	{
		PSC->bHasBeenActivated = false;//Clear this flag so we re register with the significance manager on next activation.
		PSC->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); // When detaching, maintain world position for optimization purposes
		PSC->SetRelativeScale3D(FVector(1.f)); // Reset scale to avoid future uses of this PSC having incorrect scale
		PSC->SetAbsolute(); // Clear out Absolute settings to defaults
		PSC->UnregisterComponent();
		PSC->SetCastShadow(false);

		PSC->OnParticleSpawn.Clear();
		PSC->OnParticleBurst.Clear();
		PSC->OnParticleDeath.Clear();
		PSC->OnParticleCollide.Clear();

		//Clear some things so that this PSC is re-used as though it were brand new.
		PSC->bWasActive = false;

		//Clear out instance parameters.
		PSC->InstanceParameters.Reset();
		
		//Ensure a small cull distance doesn't linger to future users.
		PSC->SetCullDistance(FLT_MAX);

		PSC->PoolingMethod = EPSCPoolMethod::FreeInPool;
		FreeElements.Push(FPSCPoolElem(PSC, CurrentTimeSeconds));
	}
	else
	{
		//We've stopped pooling while some effects were in flight so ensure they're destroyed now.
		PSC->PoolingMethod = EPSCPoolMethod::None;//Reset so we don't trigger warnings about destroying pooled PSCs.
		PSC->DestroyComponent();
	}
}

void FPSCPool::KillUnusedComponents(float KillTime, UParticleSystem* Template)
{
	int32 i = 0;
	while (i < FreeElements.Num())
	{
		if (FreeElements[i].LastUsedTime < KillTime)
		{
			UParticleSystemComponent* PSC = FreeElements[i].PSC;
			if (PSC)
			{
				PSC->PoolingMethod = EPSCPoolMethod::None;//Reset so we don't trigger warnings about destroying pooled PSCs.
				PSC->DestroyComponent();
			}

			FreeElements.RemoveAtSwap(i, 1, false);
		}
		else
		{
			++i;
		}
	}
	FreeElements.Shrink();

#if ENABLE_PSC_POOL_DEBUGGING
	//Clean up any in use components that have been cleared out from under the pool. This could happen in someone manually destroys a component for example.
	i = 0;
	while (i < InUseComponents_Manual.Num())
	{
		if (!InUseComponents_Manual[i])
		{
			UE_LOG(LogParticles, Log, TEXT("Manual Pooled PSC has been destroyed! Possibly via a DestroyComponent() call. You should not destroy these but rather call ReleaseToPool on the component so it can be re-used. |\t System: %s"), *Template->GetFullName());
			InUseComponents_Manual.RemoveAtSwap(i, 1, false);
		}
		else
		{
			++i;
		}
	}
	InUseComponents_Manual.Shrink();

	i = 0;
	while (i < InUseComponents_Auto.Num())
	{
		if (!InUseComponents_Auto[i])
		{
			UE_LOG(LogParticles, Log, TEXT("Auto Pooled PSC has been destroyed! Possibly via a DestroyComponent() call. You should not destroy these manually. Just deactivate them and allow then to be reclaimed by the pool automatically. |\t System: %s"), *Template->GetFullName());
			InUseComponents_Auto.RemoveAtSwap(i, 1, false);
		}
		else
		{
			++i;
		}
	}
	InUseComponents_Auto.Shrink();
#endif
}

//////////////////////////////////////////////////////////////////////////

FWorldPSCPool::FWorldPSCPool()
	: LastParticleSytemPoolCleanTime(0.0f)
	, CachedWorldTime(0.0f)
{

}

FWorldPSCPool::~FWorldPSCPool()
{
	Cleanup();
}

void FWorldPSCPool::Cleanup()
{
	for (TPair<UParticleSystem*, FPSCPool>& Pool : WorldParticleSystemPools)
	{
		Pool.Value.Cleanup();
	}

	WorldParticleSystemPools.Empty();
}

UParticleSystemComponent* FWorldPSCPool::CreateWorldParticleSystem(UParticleSystem* Template, UWorld* World, EPSCPoolMethod PoolingMethod)
{
	check(IsInGameThread());
	check(World);
	if (!Template)
	{
		UE_LOG(LogParticles, Warning, TEXT("Attempted CreateWorldParticleSystem() with a NULL Template!"));
		return nullptr;
	}

	if (World->bIsTearingDown)
	{
		UE_LOG(LogParticles, Warning, TEXT("Failed to create pooled particle system as we are tearing the world down."));
		return nullptr;
	}

	UParticleSystemComponent* PSC = nullptr;
	if (GbEnableParticleSystemPooling != 0)
	{
		if (Template->CanBePooled())
		{
			FPSCPool& PSCPool = WorldParticleSystemPools.FindOrAdd(Template);
			PSC = PSCPool.Acquire(World, Template, PoolingMethod);
		}
	}
	else
	{
		WorldParticleSystemPools.Empty();//Ensure the pools are cleared out if we've just switched to not pooling.
	}

	if(PSC == nullptr)
	{
		//Create a new auto destroy system if we're not pooling.
		PSC = NewObject<UParticleSystemComponent>(World);
		PSC->bAutoDestroy = true;
		PSC->SecondsBeforeInactive = 0.0f;
		PSC->bAutoActivate = false;
		PSC->SetTemplate(Template);
		PSC->bOverrideLODMethod = false;
	}

	check(PSC);
	return PSC;
}

/** Called when an in-use particle component is finished and wishes to be returned to the pool. */
void FWorldPSCPool::ReclaimWorldParticleSystem(UParticleSystemComponent* PSC)
{
	check(IsInGameThread());
	
	//If this component has been already destroyed we don't add it back to the pool. Just warn so users can fixup.
	if (PSC->IsPendingKill())
	{
		UE_LOG(LogParticles, Log, TEXT("Pooled PSC has been destroyed! Possibly via a DestroyComponent() call. You should not destroy components set to auto destroy manually. \nJust deactivate them and allow them to destroy themselves or be reclaimed by the pool if pooling is enabled. | PSC: %p |\t System: %s"), PSC, *PSC->Template->GetFullName());
		return;
	}

	if (GbEnableParticleSystemPooling)
	{
		float CurrentTime = PSC->GetWorld()->GetTimeSeconds();

		//Periodically clear up the pools.
		if (CurrentTime - LastParticleSytemPoolCleanTime > GParticleSystemPoolingCleanTime)
		{
			LastParticleSytemPoolCleanTime = CurrentTime;
			for (TPair<UParticleSystem*, FPSCPool>& Pair : WorldParticleSystemPools)
			{
				Pair.Value.KillUnusedComponents(CurrentTime - GParticleSystemPoolKillUnusedTime, PSC->Template);
			}
		}
		
		FPSCPool* PSCPool = WorldParticleSystemPools.Find(PSC->Template);
		if (!PSCPool)
		{
			UE_LOG(LogParticles, Warning, TEXT("WorldPSC Pool trying to reclaim a system for which it doesn't have a pool! Likely because SetTemplate() has been called on this PSC. | World: %p | PSC: %p | Sys: %s"), PSC->GetWorld(), PSC, *PSC->Template->GetFullName());
			//Just add the new pool and reclaim to that one.
			PSCPool = &WorldParticleSystemPools.Add(PSC->Template);
		}

		PSCPool->Reclaim(PSC, CurrentTime);
	}
	else
	{
		PSC->DestroyComponent();
	}
}

void FWorldPSCPool::ReclaimActiveParticleSystems()
{
	check(IsInGameThread());

	for (TPair<UParticleSystem*, FPSCPool>& Pair : WorldParticleSystemPools)
	{
		FPSCPool& Pool = Pair.Value;

		for(int32 i = Pool.InUseComponents_Auto.Num() - 1; i >= 0; --i)
		{
			UParticleSystemComponent* PSC = Pool.InUseComponents_Auto[i];
			if (ensureAlways(PSC))
			{
				PSC->Complete();
			}
		}

		for(int32 i = Pool.InUseComponents_Manual.Num() - 1; i >= 0; --i)
		{
			UParticleSystemComponent* PSC = Pool.InUseComponents_Manual[i];
			if (ensureAlways(PSC))
			{
				PSC->Complete();
			}
		}
	}
}

void FWorldPSCPool::Dump()
{
#if ENABLE_PSC_POOL_DEBUGGING
	FString DumpStr;

	uint32 TotalMemUsage = 0;
	for (TPair<UParticleSystem*, FPSCPool>& Pair : WorldParticleSystemPools)
	{
		UParticleSystem* System = Pair.Key;
		FPSCPool& Pool = Pair.Value;		
		uint32 FreeMemUsage = 0;
		for (FPSCPoolElem& Elem : Pool.FreeElements)
		{
			if (ensureAlways(Elem.PSC))
			{
				FreeMemUsage += Elem.PSC->GetApproxMemoryUsage();
			}
		}
		uint32 InUseMemUsage = 0;
		for (UParticleSystemComponent* PSC : Pool.InUseComponents_Auto)
		{
			if (ensureAlways(PSC))
			{
				InUseMemUsage += PSC->GetApproxMemoryUsage();				
			}
		}
		for (UParticleSystemComponent* PSC : Pool.InUseComponents_Manual)
		{
			if (ensureAlways(PSC))
			{
				InUseMemUsage += PSC->GetApproxMemoryUsage();
			}
		}

		TotalMemUsage += FreeMemUsage;
		TotalMemUsage += InUseMemUsage;

		DumpStr += FString::Printf(TEXT("Free: %d (%uB) \t|\t Used(Auto - Manual): %d - %d (%uB) \t|\t MaxUsed: %d \t|\t System: %s\n"), Pool.FreeElements.Num(), FreeMemUsage, Pool.InUseComponents_Auto.Num(), Pool.InUseComponents_Manual.Num(), InUseMemUsage, Pool.MaxUsed, *System->GetFullName());
	}

	UE_LOG(LogParticles, Log, TEXT("***************************************"));
	UE_LOG(LogParticles, Log, TEXT("*Particle System Pool Info - Total Mem = %.2fMB*"), TotalMemUsage / 1024.0f / 1024.0f);
	UE_LOG(LogParticles, Log, TEXT("***************************************"));
	UE_LOG(LogParticles, Log, TEXT("%s"), *DumpStr);
	UE_LOG(LogParticles, Log, TEXT("***************************************"));

#endif
}

void DumpPooledWorldParticleSystemInfo(UWorld* World)
{
	check(World);
	World->GetPSCPool().Dump();
}

FAutoConsoleCommandWithWorld DumpPSCPoolInfoCommand(
	TEXT("fx.DumpPSCPoolInfo"),
	TEXT("Dump Particle System Pooling Info"),
	FConsoleCommandWithWorldDelegate::CreateStatic(&DumpPooledWorldParticleSystemInfo)
);
