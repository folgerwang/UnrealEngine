// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUScene.cpp
=============================================================================*/

#include "GPUScene.h"
#include "CoreMinimal.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "ByteBuffer.h"
#include "SpriteIndexBuffer.h"
#include "SceneFilterRendering.h"
#include "ClearQuad.h"
#include "RendererModule.h"

int32 GGPUSceneUploadEveryFrame = 0;
FAutoConsoleVariableRef CVarGPUSceneUploadEveryFrame(
	TEXT("r.GPUScene.UploadEveryFrame"),
	GGPUSceneUploadEveryFrame,
	TEXT("Whether to upload the entire scene's primitive data every frame.  Useful for debugging."),
	ECVF_RenderThreadSafe
	);

int32 GGPUSceneValidatePrimitiveBuffer = 0;
FAutoConsoleVariableRef CVarGPUSceneValidatePrimitiveBuffer(
	TEXT("r.GPUScene.ValidatePrimitiveBuffer"),
	GGPUSceneValidatePrimitiveBuffer,
	TEXT("Whether to readback the GPU primitive data and assert if it doesn't match the RT primitive data.  Useful for debugging."),
	ECVF_RenderThreadSafe
	);

int32 GGPUSceneMaxPooledUploadBufferSize = 256000;
FAutoConsoleVariableRef CVarGGPUSceneMaxPooledUploadBufferSize(
	TEXT("r.GPUScene.MaxPooledUploadBufferSize"),
	GGPUSceneMaxPooledUploadBufferSize,
	TEXT("Maximum size of GPU Scene upload buffer size to pool."),
	ECVF_RenderThreadSafe
	);

// Allocate a range.  Returns allocated StartOffset.
int32 FGrowOnlySpanAllocator::Allocate(int32 Num)
{
	const int32 FoundIndex = SearchFreeList(Num);

	// Use an existing free span if one is found
	if (FoundIndex != INDEX_NONE)
	{
		FLinearAllocation FreeSpan = FreeSpans[FoundIndex];

		if (FreeSpan.Num > Num)
		{
			// Update existing free span with remainder
			FreeSpans[FoundIndex] = FLinearAllocation(FreeSpan.StartOffset + Num, FreeSpan.Num - Num);
		}
		else
		{
			// Fully consumed the free span
			FreeSpans.RemoveAtSwap(FoundIndex);
		}
				
		return FreeSpan.StartOffset;
	}

	// New allocation
	int32 StartOffset = MaxSize;
	MaxSize = MaxSize + Num;

	return StartOffset;
}

// Free an already allocated range.  
void FGrowOnlySpanAllocator::Free(int32 BaseOffset, int32 Num)
{
	check(BaseOffset + Num <= MaxSize);

	FLinearAllocation NewFreeSpan(BaseOffset, Num);

#if DO_CHECK
	// Detect double delete
	for (int32 i = 0; i < FreeSpans.Num(); i++)
	{
		FLinearAllocation CurrentSpan = FreeSpans[i];
		check(!(CurrentSpan.Contains(NewFreeSpan)));
	}
#endif

	bool bMergedIntoExisting = false;

	int32 SpanBeforeIndex = INDEX_NONE;
	int32 SpanAfterIndex = INDEX_NONE;

	// Search for existing free spans we can merge with
	for (int32 i = 0; i < FreeSpans.Num(); i++)
	{
		FLinearAllocation CurrentSpan = FreeSpans[i];

		if (CurrentSpan.StartOffset == NewFreeSpan.StartOffset + NewFreeSpan.Num)
		{
			SpanAfterIndex = i;
		}

		if (CurrentSpan.StartOffset + CurrentSpan.Num == NewFreeSpan.StartOffset)
		{
			SpanBeforeIndex = i;
		}
	}

	if (SpanBeforeIndex != INDEX_NONE)
	{
		// Merge span before with new free span
		FLinearAllocation& SpanBefore = FreeSpans[SpanBeforeIndex];
		SpanBefore.Num += NewFreeSpan.Num;

		if (SpanAfterIndex != INDEX_NONE)
		{
			// Also merge span after with span before
			FLinearAllocation SpanAfter = FreeSpans[SpanAfterIndex];
			SpanBefore.Num += SpanAfter.Num;
			FreeSpans.RemoveAtSwap(SpanAfterIndex);
		}
	}
	else if (SpanAfterIndex != INDEX_NONE)
	{
		// Merge span after with new free span
		FLinearAllocation& SpanAfter = FreeSpans[SpanAfterIndex];
		SpanAfter.StartOffset = NewFreeSpan.StartOffset;
		SpanAfter.Num += NewFreeSpan.Num;
	}
	else 
	{
		// Couldn't merge, store new free span
		FreeSpans.Add(NewFreeSpan);
	}
}

int32 FGrowOnlySpanAllocator::SearchFreeList(int32 Num)
{
	// Search free list for first matching
	for (int32 i = 0; i < FreeSpans.Num(); i++)
	{
		FLinearAllocation CurrentSpan = FreeSpans[i];

		if (CurrentSpan.Num >= Num)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void UpdateGPUScene(FRHICommandList& RHICmdList, FScene& Scene)
{
	if (UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateGPUScene);

		if (GGPUSceneUploadEveryFrame || Scene.GPUScene.bUpdateAllPrimitives)
		{
			for (int32 Index : Scene.GPUScene.PrimitivesToUpdate)
			{
				Scene.GPUScene.PrimitivesMarkedToUpdate[Index] = false;
			}
			Scene.GPUScene.PrimitivesToUpdate.Reset();

			for (int32 i = 0; i < Scene.Primitives.Num(); i++)
			{
				Scene.GPUScene.PrimitivesToUpdate.Add(i);
			}

			Scene.GPUScene.bUpdateAllPrimitives = false;
		}

		bool bResizedPrimitiveData = false;
		bool bResizedLightmapData = false;
		{
			const int32 NumPrimitiveEntries = Scene.Primitives.Num();
			const uint32 PrimitiveSceneNumFloat4s = NumPrimitiveEntries * FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s;
			// Reserve enough space
			bResizedPrimitiveData = ResizeBufferIfNeeded(RHICmdList, Scene.GPUScene.PrimitiveBuffer, FMath::RoundUpToPowerOfTwo(PrimitiveSceneNumFloat4s));
		}
		
		{
			const int32 NumLightmapDataEntries = Scene.GPUScene.LightmapDataAllocator.GetMaxSize();
			const uint32 LightmapDataNumFloat4s = NumLightmapDataEntries * FLightmapSceneShaderData::LightmapDataStrideInFloat4s;
			bResizedLightmapData = ResizeBufferIfNeeded(RHICmdList, Scene.GPUScene.LightmapDataBuffer, FMath::RoundUpToPowerOfTwo(LightmapDataNumFloat4s));
		}

		const int32 NumPrimitiveDataUploads = Scene.GPUScene.PrimitivesToUpdate.Num();

		if (NumPrimitiveDataUploads > 0)
		{
			SCOPED_DRAW_EVENTF(RHICmdList, UpdateGPUScene, TEXT("UpdateGPUScene PrimitivesToUpdate = %u"), NumPrimitiveDataUploads);

			FScatterUploadBuilder PrimitivesUploadBuilder(NumPrimitiveDataUploads, FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s, Scene.GPUScene.PrimitivesUploadScatterBuffer, Scene.GPUScene.PrimitivesUploadDataBuffer);

			int32 NumLightmapDataUploads = 0;

			for (int32 Index : Scene.GPUScene.PrimitivesToUpdate)
			{
				// PrimitivesToUpdate may contain a stale out of bounds index, as we don't remove update request on primitive removal from scene.
				if (Index < Scene.PrimitiveSceneProxies.Num())
				{
					FPrimitiveSceneProxy* PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[Index];
					NumLightmapDataUploads += PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetNumLightmapDataEntries();

					FPrimitiveSceneShaderData PrimitiveSceneData(PrimitiveSceneProxy);
					PrimitivesUploadBuilder.Add(Index, &PrimitiveSceneData.Data[0]);
				}

				Scene.GPUScene.PrimitivesMarkedToUpdate[Index] = false;
			}

			if (bResizedPrimitiveData)
			{
				RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, Scene.GPUScene.PrimitiveBuffer.UAV);
			}
			else
			{
				RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, Scene.GPUScene.PrimitiveBuffer.UAV);
			}
			
			PrimitivesUploadBuilder.UploadTo_Flush(RHICmdList, Scene.GPUScene.PrimitiveBuffer);

			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, Scene.GPUScene.PrimitiveBuffer.UAV);

			if (GGPUSceneValidatePrimitiveBuffer && Scene.GPUScene.PrimitiveBuffer.NumBytes > 0)
			{
				UE_LOG(LogRenderer, Warning, TEXT("r.GPUSceneValidatePrimitiveBuffer enabled, doing slow readback from GPU"));
				FPrimitiveSceneShaderData* InstanceBufferCopy = (FPrimitiveSceneShaderData*)RHILockStructuredBuffer(Scene.GPUScene.PrimitiveBuffer.Buffer, 0, Scene.GPUScene.PrimitiveBuffer.NumBytes, RLM_ReadOnly);

				for (int32 Index = 0; Index < Scene.PrimitiveSceneProxies.Num(); Index++)
				{
					FPrimitiveSceneShaderData PrimitiveSceneData(Scene.PrimitiveSceneProxies[Index]);

					check(FMemory::Memcmp(&InstanceBufferCopy[Index], &PrimitiveSceneData, sizeof(FPrimitiveSceneShaderData)) == 0);
				}

				RHIUnlockStructuredBuffer(Scene.GPUScene.PrimitiveBuffer.Buffer);
			}

			if (NumLightmapDataUploads > 0)
			{
				FScatterUploadBuilder LightmapDataUploadBuilder(NumLightmapDataUploads, FLightmapSceneShaderData::LightmapDataStrideInFloat4s, Scene.GPUScene.LightmapUploadScatterBuffer, Scene.GPUScene.LightmapUploadDataBuffer);

				for (int32 Index : Scene.GPUScene.PrimitivesToUpdate)
				{
					// PrimitivesToUpdate may contain a stale out of bounds index, as we don't remove update request on primitive removal from scene.
					if (Index < Scene.PrimitiveSceneProxies.Num())
					{
						FPrimitiveSceneProxy* PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[Index];

						FPrimitiveSceneProxy::FLCIArray LCIs;
						PrimitiveSceneProxy->GetLCIs(LCIs);
					
						check(LCIs.Num() == PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetNumLightmapDataEntries());
						const int32 LightmapDataOffset = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetLightmapDataOffset();

						for (int32 i = 0; i < LCIs.Num(); i++)
						{
							FLightmapSceneShaderData LightmapSceneData(LCIs[i], Scene.GetFeatureLevel());
							LightmapDataUploadBuilder.Add(LightmapDataOffset + i, &LightmapSceneData.Data[0]);
						}
					}
				}

				if (bResizedLightmapData)
				{
					RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, Scene.GPUScene.LightmapDataBuffer.UAV);
				}
				else
				{
					RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, Scene.GPUScene.LightmapDataBuffer.UAV);
				}

				LightmapDataUploadBuilder.UploadTo(RHICmdList, Scene.GPUScene.LightmapDataBuffer);

				RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, Scene.GPUScene.LightmapDataBuffer.UAV);
			}

			Scene.GPUScene.PrimitivesToUpdate.Reset();

			if (Scene.GPUScene.PrimitivesUploadDataBuffer.NumBytes > (uint32)GGPUSceneMaxPooledUploadBufferSize || Scene.GPUScene.PrimitivesUploadScatterBuffer.NumBytes > (uint32)GGPUSceneMaxPooledUploadBufferSize)
			{
				Scene.GPUScene.PrimitivesUploadDataBuffer.Release();
				Scene.GPUScene.PrimitivesUploadScatterBuffer.Release();
			}

			if (Scene.GPUScene.LightmapUploadDataBuffer.NumBytes > (uint32)GGPUSceneMaxPooledUploadBufferSize || Scene.GPUScene.LightmapUploadScatterBuffer.NumBytes > (uint32)GGPUSceneMaxPooledUploadBufferSize)
			{
				Scene.GPUScene.LightmapUploadDataBuffer.Release();
				Scene.GPUScene.LightmapUploadScatterBuffer.Release();
			}
		}
	}

	checkSlow(Scene.GPUScene.PrimitivesToUpdate.Num() == 0);
}

void UploadDynamicPrimitiveShaderDataForView(FRHICommandList& RHICmdList, FScene& Scene, FViewInfo& View)
{
	if (UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UploadDynamicPrimitiveShaderDataForView);

		FRWBufferStructured& ViewPrimitiveShaderDataBuffer = View.ViewState ? View.ViewState->PrimitiveShaderDataBuffer : View.OneFramePrimitiveShaderDataBuffer;

		const int32 NumPrimitiveEntries = Scene.Primitives.Num() + View.DynamicPrimitiveShaderData.Num();
		const uint32 PrimitiveSceneNumFloat4s = NumPrimitiveEntries * FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s;

		uint32 ViewPrimitiveSceneNumFloat4s = FMath::RoundUpToPowerOfTwo(PrimitiveSceneNumFloat4s);
		uint32 BytesPerElement = GPixelFormats[PF_A32B32G32R32F].BlockBytes;

		// Reserve enough space
		if (ViewPrimitiveSceneNumFloat4s * BytesPerElement != ViewPrimitiveShaderDataBuffer.NumBytes)
		{
			ViewPrimitiveShaderDataBuffer.Release();
			ViewPrimitiveShaderDataBuffer.Initialize(BytesPerElement, ViewPrimitiveSceneNumFloat4s, 0, TEXT("ViewPrimitiveShaderDataBuffer"));
		}

		// Copy scene primitive data into view primitive data
		MemcpyBuffer(RHICmdList, Scene.GPUScene.PrimitiveBuffer, ViewPrimitiveShaderDataBuffer, Scene.Primitives.Num() * FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s);
		
		const int32 NumPrimitiveDataUploads = View.DynamicPrimitiveShaderData.Num();

		// Append View.DynamicPrimitiveShaderData to the end of the view primitive data buffer
		if (NumPrimitiveDataUploads > 0)
		{
			FScatterUploadBuilder PrimitivesUploadBuilder(NumPrimitiveDataUploads, FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s, Scene.GPUScene.PrimitivesUploadScatterBuffer, Scene.GPUScene.PrimitivesUploadDataBuffer);

			for (int32 DynamicUploadIndex = 0; DynamicUploadIndex < View.DynamicPrimitiveShaderData.Num(); DynamicUploadIndex++)
			{
				FPrimitiveSceneShaderData PrimitiveSceneData(View.DynamicPrimitiveShaderData[DynamicUploadIndex]);
				// Place dynamic primitive shader data just after scene primitive data
				PrimitivesUploadBuilder.Add(Scene.Primitives.Num() + DynamicUploadIndex, &PrimitiveSceneData.Data[0]);
			}

			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ViewPrimitiveShaderDataBuffer.UAV);

			PrimitivesUploadBuilder.UploadTo(RHICmdList, ViewPrimitiveShaderDataBuffer);
		}

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, ViewPrimitiveShaderDataBuffer.UAV);

		// Update view uniform buffer
		View.CachedViewUniformShaderParameters->LightmapSceneData = Scene.GPUScene.LightmapDataBuffer.SRV;
		View.CachedViewUniformShaderParameters->PrimitiveSceneData = ViewPrimitiveShaderDataBuffer.SRV;
		View.ViewUniformBuffer.UpdateUniformBufferImmediate(*View.CachedViewUniformShaderParameters);
	}
}

void AddPrimitiveToUpdateGPU(FScene& Scene, int32 PrimitiveId)
{
	if (UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()))
	{ 
		if (PrimitiveId + 1 > Scene.GPUScene.PrimitivesMarkedToUpdate.Num())
		{
			const int32 NewSize = Align(PrimitiveId + 1, 64);
			Scene.GPUScene.PrimitivesMarkedToUpdate.Add(0, NewSize - Scene.GPUScene.PrimitivesMarkedToUpdate.Num());
		}

		// Make sure we aren't updating same primitive multiple times.
		if (!Scene.GPUScene.PrimitivesMarkedToUpdate[PrimitiveId])
		{
			Scene.GPUScene.PrimitivesToUpdate.Add(PrimitiveId);
			Scene.GPUScene.PrimitivesMarkedToUpdate[PrimitiveId] = true;
		}
	}
}