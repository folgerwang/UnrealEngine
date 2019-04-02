// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Renderer.cpp: Renderer module implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Stats/Stats.h"
#include "Modules/ModuleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "RenderTargetPool.h"
#include "PostProcess/SceneRenderTargets.h"
#include "VisualizeTexture.h"
#include "SceneCore.h"
#include "SceneHitProxyRendering.h"
#include "SceneRendering.h"
#include "BasePassRendering.h"
#include "MobileBasePassRendering.h"
#include "TranslucentRendering.h"
#include "RendererModule.h"
#include "GPUBenchmark.h"
#include "SystemSettings.h"
#include "VisualizeTexturePresent.h"
#include "MeshPassProcessor.inl"
#include "DebugViewModeRendering.h"
#include "EditorPrimitivesRendering.h"
#include "VisualizeTexturePresent.h"
#include "ScreenSpaceDenoise.h"

DEFINE_LOG_CATEGORY(LogRenderer);

IMPLEMENT_MODULE(FRendererModule, Renderer);

#if !IS_MONOLITHIC
	// visual studio cannot find cross dll data for visualizers
	// thus as a workaround for now, copy and paste this into every module
	// where we need to visualize SystemSettings
	FSystemSettings* GSystemSettingsForVisualizers = &GSystemSettings;
#endif

// Dummy Reflection capture uniform buffer for translucent tile mesh rendering
class FDummyReflectionCaptureUniformBuffer : public TUniformBuffer<FReflectionCaptureShaderData>
{
	typedef TUniformBuffer< FReflectionCaptureShaderData > Super;
	
public:
	virtual void InitDynamicRHI() override
	{
		FReflectionCaptureShaderData DummyPositionsBuffer;
		FMemory::Memzero(DummyPositionsBuffer);
		SetContentsNoUpdate(DummyPositionsBuffer);
		Super::InitDynamicRHI();
	}
};
static TGlobalResource< FDummyReflectionCaptureUniformBuffer > GDummyReflectionCaptureUniformBuffer;

void FRendererModule::StartupModule()
{
	GScreenSpaceDenoiser = IScreenSpaceDenoiser::GetDefaultDenoiser();
}

void FRendererModule::ShutdownModule()
{
	// Free up the memory of the default denoiser. Responsibility of the plugin to free up theirs.
	delete IScreenSpaceDenoiser::GetDefaultDenoiser();
}

void FRendererModule::ReallocateSceneRenderTargets()
{
	FLightPrimitiveInteraction::InitializeMemoryPool();
	FSceneRenderTargets::GetGlobalUnsafe().UpdateRHI();
}

void FRendererModule::SceneRenderTargetsSetBufferSize(uint32 SizeX, uint32 SizeY)
{
	FSceneRenderTargets::GetGlobalUnsafe().SetBufferSize(SizeX, SizeY);
	FSceneRenderTargets::GetGlobalUnsafe().UpdateRHI();
}

void FRendererModule::InitializeSystemTextures(FRHICommandListImmediate& RHICmdList)
{
	GSystemTextures.InitializeTextures(RHICmdList, GMaxRHIFeatureLevel);
}

void FRendererModule::DrawTileMesh(FRHICommandListImmediate& RHICmdList, FMeshPassProcessorRenderState& DrawRenderState, const FSceneView& SceneView, FMeshBatch& Mesh, bool bIsHitTesting, const FHitProxyId& HitProxyId)
{
	if (!GUsingNullRHI)
	{
		// Create an FViewInfo so we can initialize its RHI resources
		//@todo - reuse this view for multiple tiles, this is going to be slow for each tile
		FViewInfo View(&SceneView);
		View.ViewRect = View.UnscaledViewRect;
		
		const auto FeatureLevel = View.GetFeatureLevel();
		const EShadingPath ShadingPath = FSceneInterface::GetShadingPath(FeatureLevel);

		Mesh.MaterialRenderProxy->UpdateUniformExpressionCacheIfNeeded(FeatureLevel);
		FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

		//Apply the minimal forward lighting resources
		extern FForwardLightingViewResources* GetMinimalDummyForwardLightingResources();
		View.ForwardLightingResources = GetMinimalDummyForwardLightingResources();

		FSinglePrimitiveStructuredBuffer SinglePrimitiveStructuredBuffer;

		if (Mesh.VertexFactory->GetPrimitiveIdStreamIndex(true) >= 0)
		{
			FMeshBatchElement& MeshElement = Mesh.Elements[0];

			checkf(Mesh.Elements.Num() == 1, TEXT("Only 1 batch element currently supported by DrawTileMesh"));
			checkf(MeshElement.PrimitiveUniformBuffer == nullptr, TEXT("DrawTileMesh does not currently support an explicit primitive uniform buffer on vertex factories which manually fetch primitive data.  Use PrimitiveUniformBufferResource instead."));

			if (MeshElement.PrimitiveUniformBufferResource)
			{
				checkf(MeshElement.NumInstances == 1, TEXT("DrawTileMesh does not currently support instancing"));
				// Force PrimitiveId to be 0 in the shader
				MeshElement.PrimitiveIdMode = PrimID_ForceZero;
				
				// Set the LightmapID to 0, since that's where our light map data resides for this primitive
				FPrimitiveUniformShaderParameters PrimitiveParams = *(const FPrimitiveUniformShaderParameters*)MeshElement.PrimitiveUniformBufferResource->GetContents();
				PrimitiveParams.LightmapDataIndex = 0;

				// Now we just need to fill out the first entry of primitive data in a buffer and bind it
				SinglePrimitiveStructuredBuffer.PrimitiveSceneData = FPrimitiveSceneShaderData(PrimitiveParams);

				// Set up the parameters for the LightmapSceneData from the given LCI data 
				FPrecomputedLightingUniformParameters LightmapParams;
				GetPrecomputedLightingParameters(FeatureLevel, LightmapParams, Mesh.LCI);
				SinglePrimitiveStructuredBuffer.LightmapSceneData = FLightmapSceneShaderData(LightmapParams);

				SinglePrimitiveStructuredBuffer.InitResource();
				View.PrimitiveSceneDataOverrideSRV = SinglePrimitiveStructuredBuffer.PrimitiveSceneDataBufferSRV;
				View.LightmapSceneDataOverrideSRV = SinglePrimitiveStructuredBuffer.LightmapSceneDataBufferSRV;
			}
		}
		
		View.InitRHIResources();
		DrawRenderState.SetViewUniformBuffer(View.ViewUniformBuffer);

		if (ShadingPath == EShadingPath::Mobile)
		{
			View.MobileDirectionalLightUniformBuffers[0] = TUniformBufferRef<FMobileDirectionalLightShaderParameters>::CreateUniformBufferImmediate(FMobileDirectionalLightShaderParameters(), UniformBuffer_SingleFrame);
		}

		const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel);

		//get the blend mode of the material
		const EBlendMode MaterialBlendMode = Material->GetBlendMode();

		GSystemTextures.InitializeTextures(RHICmdList, FeatureLevel);
		FMemMark Mark(FMemStack::Get());

		// handle translucent material blend modes, not relevant in MaterialTexCoordScalesAnalysis since it outputs the scales.
		if (View.Family->GetDebugViewShaderMode() == DVSM_OutputMaterialTextureScales)
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// make sure we are doing opaque drawing
			DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
			
			// is this path used on mobile?
			if (ShadingPath == EShadingPath::Deferred)
			{
				FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
				FDebugViewModePassPassUniformParameters PassParameters;
				SetupDebugViewModePassUniformBuffer(SceneContext, View.GetFeatureLevel(), PassParameters);
				TUniformBufferRef<FDebugViewModePassPassUniformParameters> DebugViewModePassUniformBuffer = TUniformBufferRef<FDebugViewModePassPassUniformParameters>::CreateUniformBufferImmediate(
					PassParameters,
					UniformBuffer_SingleFrame
				);

				DrawDynamicMeshPass(View, RHICmdList,
					[&View, &DrawRenderState, &DebugViewModePassUniformBuffer, &Mesh](FMeshPassDrawListContext* InDrawListContext)
				{
					FDebugViewModeMeshProcessor PassMeshProcessor(nullptr, View.GetFeatureLevel(), &View, DebugViewModePassUniformBuffer, false, InDrawListContext);
					const uint64 DefaultBatchElementMask = ~0ull;
					PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
				});
			}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}
		else if (IsTranslucentBlendMode(MaterialBlendMode))
		{
			FUniformBufferRHIRef BasePassUniformBuffer;

			if (ShadingPath == EShadingPath::Deferred)
			{
				// Crash fix - reflection capture shader parameter is bound but we have no buffer during Build Texture Streaming
				if(!View.ReflectionCaptureUniformBuffer.IsValid())
				{
					View.ReflectionCaptureUniformBuffer = GDummyReflectionCaptureUniformBuffer.GetUniformBufferRef();
				}
			
				TUniformBufferRef<FTranslucentBasePassUniformParameters> TranslucentBasePassUniformBuffer;
				CreateTranslucentBasePassUniformBuffer(RHICmdList, View, nullptr, ESceneTextureSetupMode::None, TranslucentBasePassUniformBuffer, 0);
				BasePassUniformBuffer = TranslucentBasePassUniformBuffer;
				
				DrawRenderState.SetPassUniformBuffer(BasePassUniformBuffer);
				
				DrawDynamicMeshPass(View, RHICmdList,
					[&View, &DrawRenderState, &Mesh](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
				{
					FBasePassMeshProcessor PassMeshProcessor(
															 nullptr,
															 View.GetFeatureLevel(),
															 &View,
															 DrawRenderState,
															 DynamicMeshPassContext,
															 ETranslucencyPass::TPT_AllTranslucency);
					
					const uint64 DefaultBatchElementMask = ~0ull;
					PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
				});
			}
			else // Mobile
			{
				TUniformBufferRef<FMobileBasePassUniformParameters> MobileBasePassUniformBuffer;
				CreateMobileBasePassUniformBuffer(RHICmdList, View, true, MobileBasePassUniformBuffer);
				BasePassUniformBuffer = MobileBasePassUniformBuffer;
				
				DrawRenderState.SetPassUniformBuffer(BasePassUniformBuffer);
				
				DrawDynamicMeshPass(View, RHICmdList,
					[&View, &DrawRenderState, &Mesh](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
				{
					FMobileBasePassMeshProcessor PassMeshProcessor(
															 nullptr,
															 View.GetFeatureLevel(),
															 &View,
															 DrawRenderState,
															 DynamicMeshPassContext,
															 false,
															 ETranslucencyPass::TPT_AllTranslucency); // No CSM?
					
					const uint64 DefaultBatchElementMask = ~0ull;
					PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
				});
			}

			
		}
		// handle opaque materials
		else
		{
			// make sure we are doing opaque drawing
			DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());

			// draw the mesh
			if (bIsHitTesting)
			{
				ensureMsgf(HitProxyId == Mesh.BatchHitProxyId, TEXT("Only Mesh.BatchHitProxyId is used for hit testing."));

#if WITH_EDITOR
				DrawDynamicMeshPass(View, RHICmdList,
					[&View, &DrawRenderState, &Mesh](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
				{
					FHitProxyMeshProcessor PassMeshProcessor(
						nullptr,
						&View,
						false,
						DrawRenderState,
						DynamicMeshPassContext);

					const uint64 DefaultBatchElementMask = ~0ull;
					PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
				});
#endif
			}
			else
			{
				FUniformBufferRHIRef BasePassUniformBuffer;

				if (ShadingPath == EShadingPath::Deferred)
				{
					TUniformBufferRef<FOpaqueBasePassUniformParameters> OpaqueBasePassUniformBuffer;
					CreateOpaqueBasePassUniformBuffer(RHICmdList, View, nullptr, OpaqueBasePassUniformBuffer);
					BasePassUniformBuffer = OpaqueBasePassUniformBuffer;
					
					DrawRenderState.SetPassUniformBuffer(BasePassUniformBuffer);
					
					DrawDynamicMeshPass(View, RHICmdList,
						[&View, &DrawRenderState, &Mesh](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
					{
						FBasePassMeshProcessor PassMeshProcessor(
																 nullptr,
																 View.GetFeatureLevel(),
																 &View,
																 DrawRenderState,
																 DynamicMeshPassContext);
						
						const uint64 DefaultBatchElementMask = ~0ull;
						PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
					});
				}
				else // Mobile
				{
					TUniformBufferRef<FMobileBasePassUniformParameters> MobileBasePassUniformBuffer;
					CreateMobileBasePassUniformBuffer(RHICmdList, View, false, MobileBasePassUniformBuffer);
					BasePassUniformBuffer = MobileBasePassUniformBuffer;
					
					DrawRenderState.SetPassUniformBuffer(BasePassUniformBuffer);
					
					DrawDynamicMeshPass(View, RHICmdList,
						[&View, &DrawRenderState, &Mesh](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
					{
						FMobileBasePassMeshProcessor PassMeshProcessor(
																 nullptr,
																 View.GetFeatureLevel(),
																 &View,
																 DrawRenderState,
																 DynamicMeshPassContext, true);
						
						const uint64 DefaultBatchElementMask = ~0ull;
						PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, nullptr);
					});
				}

				
			}
		}

		SinglePrimitiveStructuredBuffer.ReleaseResource();
	}
}

void FRendererModule::RenderTargetPoolFindFreeElement(FRHICommandListImmediate& RHICmdList, const FPooledRenderTargetDesc& Desc, TRefCountPtr<IPooledRenderTarget> &Out, const TCHAR* InDebugName)
{
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Out, InDebugName);
}

void FRendererModule::TickRenderTargetPool()
{
	GRenderTargetPool.TickPoolElements();
}

void FRendererModule::DebugLogOnCrash()
{
	GVisualizeTexture.SortOrder = 1;
	GVisualizeTexture.bFullList = true;
	FVisualizeTexturePresent::DebugLog(false);
	
	GEngine->Exec(NULL, TEXT("rhi.DumpMemory"), *GLog);

	// execute on main thread
	{
		struct FTest
		{
			void Thread()
			{
				GEngine->Exec(NULL, TEXT("Mem FromReport"), *GLog);
			}
		} Test;

		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.DumpDataAfterCrash"),
			STAT_FSimpleDelegateGraphTask_DumpDataAfterCrash,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(&Test, &FTest::Thread),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DumpDataAfterCrash), nullptr, ENamedThreads::GameThread
		);
	}
}

void FRendererModule::GPUBenchmark(FSynthBenchmarkResults& InOut, float WorkScale)
{
	check(IsInGameThread());

	FSceneViewInitOptions ViewInitOptions;
	FIntRect ViewRect(0, 0, 1, 1);

	FBox LevelBox(FVector(-WORLD_MAX), FVector(+WORLD_MAX));
	ViewInitOptions.SetViewRectangle(ViewRect);

	// Initialize Projection Matrix and ViewMatrix since FSceneView initialization is doing some math on them.
	// Otherwise it trips NaN checks.
	const FVector ViewPoint = LevelBox.GetCenter();
	ViewInitOptions.ViewOrigin = FVector(ViewPoint.X, ViewPoint.Y, 0.0f);
	ViewInitOptions.ViewRotationMatrix = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, -1, 0, 0),
		FPlane(0, 0, -1, 0),
		FPlane(0, 0, 0, 1));

	const float ZOffset = WORLD_MAX;
	ViewInitOptions.ProjectionMatrix = FReversedZOrthoMatrix(
		LevelBox.GetSize().X / 2.f,
		LevelBox.GetSize().Y / 2.f,
		0.5f / ZOffset,
		ZOffset
		);

	FSceneView DummyView(ViewInitOptions);
	FlushRenderingCommands();
	FSynthBenchmarkResults* InOutPtr = &InOut;
	ENQUEUE_RENDER_COMMAND(RendererGPUBenchmarkCommand)(
		[DummyView, WorkScale, InOutPtr](FRHICommandListImmediate& RHICmdList)
		{
			RendererGPUBenchmark(RHICmdList, *InOutPtr, DummyView, WorkScale);
		});
	FlushRenderingCommands();
}

static void VisualizeTextureExec( const TCHAR* Cmd, FOutputDevice &Ar )
{
	check(IsInGameThread());

	FlushRenderingCommands();

	uint32 ParameterCount = 0;

	// parse parameters
	for(;;)
	{
		FString Parameter = FParse::Token(Cmd, 0);

		if(Parameter.IsEmpty())
		{
			break;
		}

		// FULL flag
		if(Parameter == TEXT("fulllist") || Parameter == TEXT("full"))
		{
			GVisualizeTexture.bFullList = true;
			// this one doesn't count as parameter so we can do "vis full"
			continue;
		}
		// SORT0 flag
		else if(Parameter == TEXT("sort0"))
		{
			GVisualizeTexture.SortOrder = 0;
			// this one doesn't count as parameter so we can do "vis full"
			continue;
		}
		// SORT1 flag
		else if(Parameter == TEXT("sort1"))
		{
			GVisualizeTexture.SortOrder = 1;
			// this one doesn't count as parameter so we can do "vis full"
			continue;
		}
		else if(ParameterCount == 0)
		{
			// Init
			GVisualizeTexture.RGBMul = 1;
			GVisualizeTexture.SingleChannelMul = 0.0f;
			GVisualizeTexture.SingleChannel = -1;
			GVisualizeTexture.AMul = 0;
			GVisualizeTexture.UVInputMapping = 3;
			GVisualizeTexture.Flags = 0;
			GVisualizeTexture.Mode = 0;
			GVisualizeTexture.CustomMip = 0;
			GVisualizeTexture.ArrayIndex = 0;
			GVisualizeTexture.bOutputStencil = false;

			// e.g. "VisualizeTexture Name" or "VisualizeTexture 5"
			bool bIsDigit = FChar::IsDigit(**Parameter);

			if (bIsDigit)
			{
				GVisualizeTexture.Mode = FCString::Atoi(*Parameter);
			}

			if(!bIsDigit)
			{
				// the name was specified as string
				const TCHAR* AfterAt = *Parameter;

				while(*AfterAt != 0 && *AfterAt != TCHAR('@'))
				{
					++AfterAt;
				}

				if(*AfterAt == TCHAR('@'))
				{
					// user specified a reuse goal
					FString NameWithoutAt = Parameter.Left(AfterAt - *Parameter);
					GVisualizeTexture.SetRenderTargetNameToObserve(*NameWithoutAt, FCString::Atoi(AfterAt + 1));
				}
				else
				{
					// we take the last one
					GVisualizeTexture.SetRenderTargetNameToObserve(*Parameter);
				}
			}
			else
			{
				// the index was used
				GVisualizeTexture.SetRenderTargetNameToObserve(TEXT(""));
			}
		}
		// GRenderTargetPoolInputMapping mode
		else if(Parameter == TEXT("uv0"))
		{
			GVisualizeTexture.UVInputMapping = 0;
		}
		else if(Parameter == TEXT("uv1"))
		{
			GVisualizeTexture.UVInputMapping = 1;
		}
		else if(Parameter == TEXT("uv2"))
		{
			GVisualizeTexture.UVInputMapping = 2;
		}
		else if(Parameter == TEXT("pip"))
		{
			GVisualizeTexture.UVInputMapping = 3;
		}
		// BMP flag
		else if(Parameter == TEXT("bmp"))
		{
			GVisualizeTexture.bSaveBitmap = true;
		}
		else if (Parameter == TEXT("stencil"))
		{
			GVisualizeTexture.bOutputStencil = true;
		}
		// saturate flag
		else if(Parameter == TEXT("frac"))
		{
			// default already covers this
		}
		// saturate flag
		else if(Parameter == TEXT("sat"))
		{
			GVisualizeTexture.Flags |= 0x1;
		}
		// e.g. mip2 or mip0
		else if(Parameter.Left(3) == TEXT("mip"))
		{
			Parameter = Parameter.Right(Parameter.Len() - 3);
			GVisualizeTexture.CustomMip = FCString::Atoi(*Parameter);
		}
		// e.g. [0] or [2]
		else if(Parameter.Left(5) == TEXT("index"))
		{
			Parameter = Parameter.Right(Parameter.Len() - 5);
			GVisualizeTexture.ArrayIndex = FCString::Atoi(*Parameter);
		}
		// e.g. RGB*6, A, *22, /2.7, A*7
		else if(Parameter.Left(3) == TEXT("rgb")
			|| Parameter.Left(1) == TEXT("a")
			|| Parameter.Left(1) == TEXT("r")
			|| Parameter.Left(1) == TEXT("g")
			|| Parameter.Left(1) == TEXT("b")
			|| Parameter.Left(1) == TEXT("*")
			|| Parameter.Left(1) == TEXT("/"))
		{
			int SingleChannel = -1;

			if(Parameter.Left(3) == TEXT("rgb"))
			{
				Parameter = Parameter.Right(Parameter.Len() - 3);
			}
			else if(Parameter.Left(1) == TEXT("r")) SingleChannel = 0;
			else if(Parameter.Left(1) == TEXT("g")) SingleChannel = 1;
			else if(Parameter.Left(1) == TEXT("b")) SingleChannel = 2;
			else if(Parameter.Left(1) == TEXT("a")) SingleChannel = 3;
			if ( SingleChannel >= 0 )
			{
				Parameter = Parameter.Right(Parameter.Len() - 1);
				GVisualizeTexture.SingleChannel = SingleChannel;
				GVisualizeTexture.SingleChannelMul = 1;
				GVisualizeTexture.RGBMul = 0;
			}

			float Mul = 1.0f;

			// * or /
			if(Parameter.Left(1) == TEXT("*"))
			{
				Parameter = Parameter.Right(Parameter.Len() - 1);
				Mul = FCString::Atof(*Parameter);
			}
			else if(Parameter.Left(1) == TEXT("/"))
			{
				Parameter = Parameter.Right(Parameter.Len() - 1);
				Mul = 1.0f / FCString::Atof(*Parameter);
			}
			GVisualizeTexture.RGBMul *= Mul;
			GVisualizeTexture.SingleChannelMul *= Mul;
			GVisualizeTexture.AMul *= Mul;
		}
		else
		{
			Ar.Logf(TEXT("Error: parameter \"%s\" not recognized"), *Parameter);
		}

		++ParameterCount;
	}

	if(!ParameterCount)
	{
		// show help
		Ar.Logf(TEXT("VisualizeTexture/Vis <TextureId/CheckpointName> [<Mode>] [PIP/UV0/UV1/UV2] [BMP] [FRAC/SAT] [FULL]:"));

		Ar.Logf(TEXT("Mode (examples):"));
		Ar.Logf(TEXT("  RGB      = RGB in range 0..1 (default)"));
		Ar.Logf(TEXT("  *8       = RGB * 8"));
		Ar.Logf(TEXT("  A        = alpha channel in range 0..1"));
		Ar.Logf(TEXT("  R        = red channel in range 0..1"));
		Ar.Logf(TEXT("  G        = green channel in range 0..1"));
		Ar.Logf(TEXT("  B        = blue channel in range 0..1"));
		Ar.Logf(TEXT("  A*16     = Alpha * 16"));
		Ar.Logf(TEXT("  RGB/2    = RGB / 2"));
		Ar.Logf(TEXT("SubResource:"));
		Ar.Logf(TEXT("  MIP5     = Mip level 5 (0 is default)"));
		Ar.Logf(TEXT("  INDEX5   = Array Element 5 (0 is default)"));
		Ar.Logf(TEXT("InputMapping:"));
		Ar.Logf(TEXT("  PIP      = like UV1 but as picture in picture with normal rendering  (default)"));
		Ar.Logf(TEXT("  UV0      = UV in left top"));
		Ar.Logf(TEXT("  UV1      = full texture"));
		Ar.Logf(TEXT("  UV2      = pixel perfect centered"));
		Ar.Logf(TEXT("Flags:"));
		Ar.Logf(TEXT("  BMP      = save out bitmap to the screenshots folder (not on console, normalized)"));
		Ar.Logf(TEXT("STENCIL    = Stencil normally displayed in alpha channel of depth.  This option is used for BMP to get a stencil only BMP."));
		Ar.Logf(TEXT("  FRAC     = use frac() in shader (default)"));
		Ar.Logf(TEXT("  SAT      = use saturate() in shader"));
		Ar.Logf(TEXT("  FULLLIST = show full list, otherwise we hide some textures in the printout"));
		Ar.Logf(TEXT("  SORT0    = sort list by name"));
		Ar.Logf(TEXT("  SORT1    = show list by size"));
		Ar.Logf(TEXT("TextureId:"));
		Ar.Logf(TEXT("  0        = <off>"));

		FVisualizeTexturePresent::DebugLog(true);
	}
	//		Ar.Logf(TEXT("VisualizeTexture %d"), GVisualizeTexture.Mode);
}

static bool RendererExec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (FParse::Command(&Cmd, TEXT("VisualizeTexture")) || FParse::Command(&Cmd, TEXT("Vis")))
	{
		VisualizeTextureExec(Cmd, Ar);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("ShowMipLevels")))
	{
		extern bool GVisualizeMipLevels;
		GVisualizeMipLevels = !GVisualizeMipLevels;
		Ar.Logf( TEXT( "Showing mip levels: %s" ), GVisualizeMipLevels ? TEXT("ENABLED") : TEXT("DISABLED") );
		return true;
	}
	else if(FParse::Command(&Cmd,TEXT("DumpUnbuiltLightInteractions")))
	{
		InWorld->Scene->DumpUnbuiltLightInteractions(Ar);
		return true;
	}
#endif

	return false;
}

ICustomCulling* GCustomCullingImpl = nullptr;

void FRendererModule::RegisterCustomCullingImpl(ICustomCulling* impl)
{
	check(GCustomCullingImpl == nullptr);
	GCustomCullingImpl = impl;
}

void FRendererModule::UnregisterCustomCullingImpl(ICustomCulling* impl)
{
	check(GCustomCullingImpl == impl);
	GCustomCullingImpl = nullptr;
}

FStaticSelfRegisteringExec RendererExecRegistration(RendererExec);

void FRendererModule::ExecVisualizeTextureCmd( const FString& Cmd )
{
	// @todo: Find a nicer way to call this
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	VisualizeTextureExec(*Cmd, *GLog);
#endif
}
