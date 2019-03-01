// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FXSystem.cpp: Implementation of the effects system.
=============================================================================*/

#include "FXSystem.h"
#include "RenderingThread.h"
#include "VectorField.h"
#include "Particles/FXSystemPrivate.h"
#include "Particles/FXSystemSet.h"
#include "GPUSort.h"
#include "Particles/ParticleCurveTexture.h"
#include "VectorField/VectorField.h"
#include "Components/VectorFieldComponent.h"
#include "SceneUtils.h"
#include "Renderer/Private/SceneRendering.h" // needed for STATGROUP_CommandListMarkers


TMap<FName, FCreateCustomFXSystemDelegate> FFXSystemInterface::CreateCustomFXDelegates;

/*-----------------------------------------------------------------------------
	External FX system interface.
-----------------------------------------------------------------------------*/

FFXSystemInterface* FFXSystemInterface::Create(ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform)
{
	if (CreateCustomFXDelegates.Num())
	{
		FFXSystemSet* Set = new FFXSystemSet;
		Set->FXSystems.Add(new FFXSystem(InFeatureLevel, InShaderPlatform));

		for (TMap<FName, FCreateCustomFXSystemDelegate>::TConstIterator Ite(CreateCustomFXDelegates); Ite; ++Ite)
		{
			FFXSystemInterface* CustomFX = Ite.Value().Execute(InFeatureLevel, InShaderPlatform);
			if (CustomFX)
			{
				Set->FXSystems.Add(CustomFX);
			}
		}
		return Set;
	}
	else
	{
		return new FFXSystem(InFeatureLevel, InShaderPlatform);
	}
}

void FFXSystemInterface::Destroy( FFXSystemInterface* FXSystem )
{
	check(FXSystem && !FXSystem->bIsPendingKill);

	// Notify that the delete command is on its way. Preventing any future render commands from accessing the FFXSystemInterface.
	FXSystem->bIsPendingKill = true;
	ENQUEUE_RENDER_COMMAND(FDestroyFXSystemCommand)(
		[FXSystem](FRHICommandList& RHICmdList)
		{
			delete FXSystem;
		});
}

void FFXSystemInterface::RegisterCustomFXSystem(const FName& InterfaceName, const FCreateCustomFXSystemDelegate& InCreateDelegate)
{
	CreateCustomFXDelegates.Add(InterfaceName, InCreateDelegate);
}

void FFXSystemInterface::UnregisterCustomFXSystem(const FName& InterfaceName)
{
	CreateCustomFXDelegates.Remove(InterfaceName);
}

FFXSystemInterface::~FFXSystemInterface()
{
}

/*------------------------------------------------------------------------------
	FX system console variables.
------------------------------------------------------------------------------*/

namespace FXConsoleVariables
{
	int32 VisualizeGPUSimulation = 0;
	int32 bAllowGPUSorting = true;
	int32 bAllowCulling = true;
	int32 bFreezeGPUSimulation = false;
	int32 bFreezeParticleSimulation = false;
	int32 bAllowAsyncTick = !WITH_EDITOR;
	float ParticleSlackGPU = 0.02f;
	int32 MaxParticleTilePreAllocation = 100;
	int32 MaxCPUParticlesPerEmitter = 1000;
	int32 MaxGPUParticlesSpawnedPerFrame = 1024 * 1024;
	int32 GPUSpawnWarningThreshold = 20000;
	float GPUCollisionDepthBounds = 500.0f;
	TAutoConsoleVariable<int32> TestGPUSort(TEXT("FX.TestGPUSort"),0,TEXT("Test GPU sort. 1: Small, 2: Large, 3: Exhaustive, 4: Random"),ECVF_Cheat);

	/** Register references to flags. */
	FAutoConsoleVariableRef CVarVisualizeGPUSimulation(
		TEXT("FX.VisualizeGPUSimulation"),
		VisualizeGPUSimulation,
		TEXT("Visualize the current state of GPU simulation.\n")
		TEXT("0 = off\n")
		TEXT("1 = visualize particle state\n")
		TEXT("2 = visualize curve texture"),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarAllowGPUSorting(
		TEXT("FX.AllowGPUSorting"),
		bAllowGPUSorting,
		TEXT("Allow particles to be sorted on the GPU."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarFreezeGPUSimulation(
		TEXT("FX.FreezeGPUSimulation"),
		bFreezeGPUSimulation,
		TEXT("Freeze particles simulated on the GPU."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarFreezeParticleSimulation(
		TEXT("FX.FreezeParticleSimulation"),
		bFreezeParticleSimulation,
		TEXT("Freeze particle simulation."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarAllowAsyncTick(
		TEXT("FX.AllowAsyncTick"),
		bAllowAsyncTick,
		TEXT("allow parallel ticking of particle systems."),
		ECVF_Default
		);
	FAutoConsoleVariableRef CVarParticleSlackGPU(
		TEXT("FX.ParticleSlackGPU"),
		ParticleSlackGPU,
		TEXT("Amount of slack to allocate for GPU particles to prevent tile churn as percentage of total particles."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarMaxParticleTilePreAllocation(
		TEXT("FX.MaxParticleTilePreAllocation"),
		MaxParticleTilePreAllocation,
		TEXT("Maximum tile preallocation for GPU particles."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarMaxCPUParticlesPerEmitter(
		TEXT("FX.MaxCPUParticlesPerEmitter"),
		MaxCPUParticlesPerEmitter,
		TEXT("Maximum number of CPU particles allowed per-emitter.")
		);
	FAutoConsoleVariableRef CVarMaxGPUParticlesSpawnedPerFrame(
		TEXT("FX.MaxGPUParticlesSpawnedPerFrame"),
		MaxGPUParticlesSpawnedPerFrame,
		TEXT("Maximum number of GPU particles allowed to spawn per-frame per-emitter.")
		);
	FAutoConsoleVariableRef CVarGPUSpawnWarningThreshold(
		TEXT("FX.GPUSpawnWarningThreshold"),
		GPUSpawnWarningThreshold,
		TEXT("Warning threshold for spawning of GPU particles."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarGPUCollisionDepthBounds(
		TEXT("FX.GPUCollisionDepthBounds"),
		GPUCollisionDepthBounds,
		TEXT("Limits the depth bounds when searching for a collision plane."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarAllowCulling(
		TEXT("FX.AllowCulling"),
		bAllowCulling,
		TEXT("Allow emitters to be culled."),
		ECVF_Cheat
		);
	int32 bAllowGPUParticles = true;
	FAutoConsoleVariableRef CVarAllowGPUParticles(
		TEXT("FX.AllowGPUParticles"),
		bAllowGPUParticles,
		TEXT("If true, allow the usage of GPU particles.")
	);
}

/*------------------------------------------------------------------------------
	FX system.
------------------------------------------------------------------------------*/

FFXSystem::FFXSystem(ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform)
	: ParticleSimulationResources(NULL)
	, FeatureLevel(InFeatureLevel)
	, ShaderPlatform(InShaderPlatform)
#if WITH_EDITOR
	, bSuspended(false)
#endif // #if WITH_EDITOR
{
	InitGPUSimulation();
}

FFXSystem::~FFXSystem()
{
	for (FVectorFieldInstance* VectorFieldInstance : VectorFields)
	{
		if (VectorFieldInstance)
		{
			delete VectorFieldInstance;
		}
	}
	VectorFields.Empty();

	DestroyGPUSimulation();
}

const FName FFXSystem::Name(TEXT("FFXSystem"));

FFXSystemInterface* FFXSystem::GetInterface(const FName& InName)
{
	return InName == Name ? this : nullptr;
}

void FFXSystem::Tick(float DeltaSeconds)
{
	if (RHISupportsGPUParticles())
	{
		// Test GPU sorting if requested.
		if (FXConsoleVariables::TestGPUSort.GetValueOnGameThread() != 0)
		{
			TestGPUSort((EGPUSortTest)FXConsoleVariables::TestGPUSort.GetValueOnGameThread(), GetFeatureLevel());
			// Reset CVar
			static IConsoleVariable* CVarTestGPUSort = IConsoleManager::Get().FindConsoleVariable(TEXT("FX.TestGPUSort"));

			// todo: bad use of console variables, this should be a console command 
			CVarTestGPUSort->Set(0, ECVF_SetByCode);
		}

		// Before ticking GPU particles, ensure any pending curves have been
		// uploaded.
		GParticleCurveTexture.SubmitPendingCurves();
	}
}

#if WITH_EDITOR
void FFXSystem::Suspend()
{
	if (!bSuspended && RHISupportsGPUParticles())
	{
		ReleaseGPUResources();
		bSuspended = true;
	}
}

void FFXSystem::Resume()
{
	if (bSuspended && RHISupportsGPUParticles())
	{
		bSuspended = false;
		InitGPUResources();
	}
}
#endif // #if WITH_EDITOR

/*------------------------------------------------------------------------------
	Vector field instances.
------------------------------------------------------------------------------*/

void FFXSystem::AddVectorField( UVectorFieldComponent* VectorFieldComponent )
{
	if (RHISupportsGPUParticles())
	{
		check( VectorFieldComponent->VectorFieldInstance == NULL );
		checkSlow( VectorFieldComponent->FXSystem && VectorFieldComponent->FXSystem->GetInterface(Name) == this );

		if ( VectorFieldComponent->VectorField && !IsPendingKill() )
		{
			FVectorFieldInstance* Instance = new FVectorFieldInstance();
			VectorFieldComponent->VectorField->InitInstance(Instance, /*bPreviewInstance=*/ false);
			VectorFieldComponent->VectorFieldInstance = Instance;
			Instance->WorldBounds = VectorFieldComponent->Bounds.GetBox();
			Instance->Intensity = VectorFieldComponent->Intensity;
			Instance->Tightness = VectorFieldComponent->Tightness;

			FFXSystem* FXSystem = this;
			FMatrix ComponentToWorld = VectorFieldComponent->GetComponentTransform().ToMatrixWithScale();
			ENQUEUE_RENDER_COMMAND(FAddVectorFieldCommand)(
				[FXSystem, Instance, ComponentToWorld](FRHICommandListImmediate& RHICmdList)
				{
					Instance->UpdateTransforms( ComponentToWorld );
					Instance->Index = FXSystem->VectorFields.AddUninitialized().Index;
					FXSystem->VectorFields[ Instance->Index ] = Instance;
				});
		}
	}
}

void FFXSystem::RemoveVectorField( UVectorFieldComponent* VectorFieldComponent )
{
	if (RHISupportsGPUParticles())
	{
		checkSlow(VectorFieldComponent->FXSystem && VectorFieldComponent->FXSystem->GetInterface(Name) == this);

		FVectorFieldInstance* Instance = VectorFieldComponent->VectorFieldInstance;
		VectorFieldComponent->VectorFieldInstance = NULL;

		// If pending kill the VectorFieldInstance will be freed in the destructor.
		if ( Instance  && !IsPendingKill() )
		{
			FFXSystem* FXSystem = this;
			ENQUEUE_RENDER_COMMAND(FRemoveVectorFieldCommand)(
				[FXSystem, Instance](FRHICommandListImmediate& RHICmdList)
				{
					if ( Instance->Index != INDEX_NONE )
					{
						FXSystem->VectorFields.RemoveAt( Instance->Index );
						delete Instance;
					}
				});
		}
	}
}

void FFXSystem::UpdateVectorField( UVectorFieldComponent* VectorFieldComponent )
{
	if (RHISupportsGPUParticles())
	{
		checkSlow(VectorFieldComponent->FXSystem && VectorFieldComponent->FXSystem->GetInterface(Name) == this);

		FVectorFieldInstance* Instance = VectorFieldComponent->VectorFieldInstance;

		if ( Instance && !IsPendingKill() )
		{
			struct FUpdateVectorFieldParams
			{
				FBox Bounds;
				FMatrix ComponentToWorld;
				float Intensity;
				float Tightness;
			};

			FUpdateVectorFieldParams UpdateParams;
			UpdateParams.Bounds = VectorFieldComponent->Bounds.GetBox();
			UpdateParams.ComponentToWorld = VectorFieldComponent->GetComponentTransform().ToMatrixWithScale();
			UpdateParams.Intensity = VectorFieldComponent->Intensity;
			UpdateParams.Tightness = VectorFieldComponent->Tightness;

			FFXSystem* FXSystem = this;
			ENQUEUE_RENDER_COMMAND(FUpdateVectorFieldCommand)(
				[FXSystem, Instance, UpdateParams](FRHICommandListImmediate& RHICmdList)
				{
					Instance->WorldBounds = UpdateParams.Bounds;
					Instance->Intensity = UpdateParams.Intensity;
					Instance->Tightness = UpdateParams.Tightness;
					Instance->UpdateTransforms( UpdateParams.ComponentToWorld );
				});
		}
	}
}

/*-----------------------------------------------------------------------------
	Render related functionality.
-----------------------------------------------------------------------------*/

void FFXSystem::DrawDebug( FCanvas* Canvas )
{
	if (FXConsoleVariables::VisualizeGPUSimulation > 0
		&& RHISupportsGPUParticles())
	{
		VisualizeGPUParticles(Canvas);
	}
}

void FFXSystem::PreInitViews()
{
	if (RHISupportsGPUParticles())
	{
		AdvanceGPUParticleFrame();
	}
}

bool FFXSystem::UsesGlobalDistanceField() const
{
	if (RHISupportsGPUParticles())
	{
		return UsesGlobalDistanceFieldInternal();
	}

	return false;
}

DECLARE_CYCLE_STAT(TEXT("FXPreRender_Prepare"), STAT_CLM_FXPreRender_Prepare, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender_Simulate"), STAT_CLM_FXPreRender_Simulate, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender_Finalize"), STAT_CLM_FXPreRender_Finalize, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender_PrepareCDF"), STAT_CLM_FXPreRender_PrepareCDF, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender_SimulateCDF"), STAT_CLM_FXPreRender_SimulateCDF, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender_FinalizeCDF"), STAT_CLM_FXPreRender_FinalizeCDF, STATGROUP_CommandListMarkers);


void FFXSystem::PreRender(FRHICommandListImmediate& RHICmdList, const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData)
{
	if (RHISupportsGPUParticles())
	{
		SCOPED_DRAW_EVENT(RHICmdList, GPUParticles_PreRender);
		UpdateMultiGPUResources(RHICmdList);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_Prepare));
		PrepareGPUSimulation(RHICmdList);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_Simulate));
		SimulateGPUParticles(RHICmdList, EParticleSimulatePhase::Main, nullptr, nullptr, nullptr, FUniformBufferRHIParamRef());

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_Finalize));
		FinalizeGPUSimulation(RHICmdList);

		if (IsParticleCollisionModeSupported(GetShaderPlatform(), PCM_DistanceField))
		{
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_PrepareCDF));
			PrepareGPUSimulation(RHICmdList);

			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_SimulateCDF));
			SimulateGPUParticles(RHICmdList, EParticleSimulatePhase::CollisionDistanceField, nullptr, GlobalDistanceFieldParameterData, nullptr, FUniformBufferRHIParamRef());
			//particles rendered during basepass may need to read pos/velocity buffers.  must finalize unless we know for sure that nothingin base pass will read.
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_FinalizeCDF));
			FinalizeGPUSimulation(RHICmdList);
		}
    }
}

void FFXSystem::PostRenderOpaque(
	FRHICommandListImmediate& RHICmdList, 
	const FUniformBufferRHIParamRef ViewUniformBuffer, 
	const FShaderParametersMetadata* SceneTexturesUniformBufferStruct,
	FUniformBufferRHIParamRef SceneTexturesUniformBuffer)
{
	if (RHISupportsGPUParticles() && IsParticleCollisionModeSupported(GetShaderPlatform(), PCM_DepthBuffer))
	{
		SCOPED_DRAW_EVENT(RHICmdList, GPUParticles_PostRenderOpaque);
		PrepareGPUSimulation(RHICmdList);
		
		SimulateGPUParticles(RHICmdList, EParticleSimulatePhase::CollisionDepthBuffer, ViewUniformBuffer, NULL, SceneTexturesUniformBufferStruct, SceneTexturesUniformBuffer);
		
		FinalizeGPUSimulation(RHICmdList);

		SortGPUParticles(RHICmdList);		
	}
}
