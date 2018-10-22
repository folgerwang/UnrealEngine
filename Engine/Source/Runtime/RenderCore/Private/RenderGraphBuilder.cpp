// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RenderGraphBuilder.h"
#include "RenderCore.h"
#include "RenderTargetPool.h"


#if RENDER_GRAPH_DEBUGGING

static int32 GRenderGraphImmediateMode = 0;
static FAutoConsoleVariableRef CVarImmediateMode(
	TEXT("r.Graph.ImmediateMode"),
	GRenderGraphImmediateMode,
	TEXT("Executes passes as they get created. Extremely useful to have a callstack of the wiring code when crashing in the pass' lambda."),
	ECVF_RenderThreadSafe);

#else

static const int32 GRenderGraphImmediateMode = 0;

#endif


void FRDGBuilder::Execute()
{
	#if RENDER_GRAPH_DEBUGGING
	{
		checkf(!bHasExecuted, TEXT("Render graph execution should only happen once."));
	}
	#endif

	if (!GRenderGraphImmediateMode)
	{
		WalkGraphDependencies();

		QUICK_SCOPE_CYCLE_COUNTER(STAT_FRDGBuilder_Execute);
		for (const FRenderGraphPass* Pass : Passes)
		{
			ExecutePass(Pass);
		}
	}

	ProcessDeferredInternalResourceQueries();

	DestructPasses();

	#if RENDER_GRAPH_DEBUGGING
	{
		bHasExecuted = true;
	}
	#endif
}

void FRDGBuilder::DebugPass(const FRenderGraphPass* Pass)
{
	ValidatePass(Pass);

	if (GRenderGraphImmediateMode)
	{
		ExecutePass(Pass);
	}
}

void FRDGBuilder::ValidatePass(const FRenderGraphPass* Pass) const
{
	FRenderTargetBindingSlots* RESTRICT RenderTargets = nullptr;
	FShaderParameterStructRef ParameterStruct = Pass->GetParameters();

	bool bIsCompute = Pass->IsCompute();
	bool bCanUseUAVs = bIsCompute;
	bool bRequiresRenderTargetSlots = !bIsCompute;

	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		uint8  Type = ParameterStruct.Layout->Resources[ResourceIndex];
		uint16 Offset = ParameterStruct.Layout->ResourceOffsets[ResourceIndex];

		switch (Type)
		{
		case UBMT_GRAPH_TRACKED_UAV:
		{
			FRDGTextureUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureUAV*>(Offset);
			if (UAV && !bCanUseUAVs)
			{
				UE_LOG(LogRendererCore, Warning, TEXT("UAV can only been bound to compute shaders, therefore UAV %s is certainly useless for passs %s."), UAV->Name, Pass->GetName());
			}
		}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			if (RenderTargets)
			{
				UE_LOG(LogRendererCore, Warning, TEXT("Pass %s have duplicated render target binding slots."), Pass->GetName());
			}
			else
			{
				RenderTargets = ParameterStruct.GetMemberPtrAtOffset<FRenderTargetBindingSlots>(Offset);
			}
		}
		break;
		default:
			break;
		}
	}

	if (RenderTargets)
	{
		checkf(bRequiresRenderTargetSlots, TEXT("Render pass %s does not need render target binging slots"), Pass->GetName());

		uint32 NumRenderTargets = 0;
		for (int i = 0; i < RenderTargets->Output.Num(); i++)
		{
			const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
			if (!RenderTarget.GetTexture())
			{
				NumRenderTargets = i;
				break;
			}
		}
		for (int i = NumRenderTargets; i < RenderTargets->Output.Num(); i++)
		{
			const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
			checkf(RenderTarget.GetTexture() == nullptr, TEXT("Render targets must be packed. No empty spaces in the array."));
		}
	}
	else
	{
		checkf(!bRequiresRenderTargetSlots, TEXT("Render pass %s requires render target binging slots"), Pass->GetName());
	}
}

void FRDGBuilder::WalkGraphDependencies()
{
	for (const FRenderGraphPass* Pass : Passes)
	{
		FShaderParameterStructRef ParameterStruct = Pass->GetParameters();

		/** Increments all the FRDGResource::ReferenceCount. */
		for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
		{
			uint8  Type = ParameterStruct.Layout->Resources[ResourceIndex];
			uint16 Offset = ParameterStruct.Layout->ResourceOffsets[ResourceIndex];

			switch (Type)
			{
			case UBMT_GRAPH_TRACKED_TEXTURE:
			{
				FRDGTexture* RESTRICT Texture = *ParameterStruct.GetMemberPtrAtOffset<FRDGTexture*>(Offset);
				if (Texture)
				{
					Texture->ReferenceCount++;
				}
			}
			break;
			case UBMT_GRAPH_TRACKED_SRV:
			{
				FRDGTextureSRV* RESTRICT SRV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureSRV*>(Offset);
				if (SRV)
				{
					SRV->Desc.Texture->ReferenceCount++;
				}
			}
			break;
			case UBMT_GRAPH_TRACKED_UAV:
			{
				FRDGTextureUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureUAV*>(Offset);
				if (UAV)
				{
					UAV->Desc.Texture->ReferenceCount++;
				}
			}
			break;
			case UBMT_RENDER_TARGET_BINDING_SLOTS:
			{
				FRenderTargetBindingSlots* RESTRICT RenderTargets = ParameterStruct.GetMemberPtrAtOffset<FRenderTargetBindingSlots>(Offset);

				for (int i = 0; i < RenderTargets->Output.Num(); i++)
				{
					const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
					if (RenderTarget.GetTexture())
					{
						RenderTarget.GetTexture()->ReferenceCount++;
					}
					else
					{
						break;
					}
				}

				const FDepthStencilBinding& DepthStencil = RenderTargets->DepthStencil;
				if (DepthStencil.Texture)
				{
					DepthStencil.Texture->ReferenceCount++;
				}
			}
			break;
			default:
				break;
			}
		}
	} // for (const FRenderGraphPass* Pass : Passes)

	// Add additional dependencies from deferred queries.
	for (const auto& Query : DeferredInternalTextureQueries)
	{
		Query.Texture->ReferenceCount++;
	}

	// Release external texture that have ReferenceCount == 0 and yet are already allocated.
	for (auto Pair : AllocatedTextures)
	{
		if (Pair.Key->ReferenceCount == 0)
		{
			Pair.Value = nullptr;
			Pair.Key->PooledRenderTarget = nullptr;
		}
	}
}

void FRDGBuilder::AllocateRHITextureIfNeeded(const FRDGTexture* Texture, bool bComputePass)
{
	check(Texture);

	if (Texture->PooledRenderTarget)
	{
		return;
	}

	check(Texture->ReferenceCount > 0 || GRenderGraphImmediateMode);

	// TODO(RDG): should avoid bDoWritableBarrier = true
	TRefCountPtr<IPooledRenderTarget>& PooledRenderTarget = AllocatedTextures.FindOrAdd(Texture);
	GRenderTargetPool.FindFreeElement(RHICmdList, Texture->Desc, PooledRenderTarget, Texture->Name, /* bDoWritableBarrier = */ true);

	Texture->PooledRenderTarget = PooledRenderTarget;
}

static EResourceTransitionPipeline CalcTransitionPipeline(bool bCurrentCompute, bool bTargetCompute)
{
	// TODO(RDG) convert table to math
	uint32 Bits;
	Bits  = (uint32)bCurrentCompute;
	Bits |= (uint32)bTargetCompute << 1;

	EResourceTransitionPipeline Table[] = {
		EResourceTransitionPipeline::EGfxToGfx,
		EResourceTransitionPipeline::EComputeToGfx,
		EResourceTransitionPipeline::EGfxToCompute,
		EResourceTransitionPipeline::EComputeToCompute
	};
	
	return static_cast< EResourceTransitionPipeline >( Table[ Bits ] );
}

void FRDGBuilder::TransitionTexture( const FRDGTexture* Texture, EResourceTransitionAccess TransitionAccess, bool bRequiredCompute ) const
{
	const bool bRequiredWritable = TransitionAccess != EResourceTransitionAccess::EReadable;

	if( Texture->bWritable != bRequiredWritable || Texture->bCompute != bRequiredCompute )
	{
		RHICmdList.TransitionResource( TransitionAccess, Texture->PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture );
		Texture->bWritable = bRequiredWritable;
		Texture->bCompute = bRequiredCompute;
	}
}

void FRDGBuilder::TransitionUAV( const FRDGTextureUAV* UAV, EResourceTransitionAccess TransitionAccess, bool bRequiredCompute ) const
{
	const bool bRequiredWritable = true;

	if( UAV->Desc.Texture->bWritable != bRequiredWritable || UAV->Desc.Texture->bCompute != bRequiredCompute )
	{
		EResourceTransitionPipeline TransitionPipeline = CalcTransitionPipeline( UAV->Desc.Texture->bCompute, bRequiredCompute );
		RHICmdList.TransitionResource( TransitionAccess, TransitionPipeline, UAV->GetRHIUnorderedAccessView());
		UAV->Desc.Texture->bWritable = bRequiredWritable;
		UAV->Desc.Texture->bCompute = bRequiredCompute;
	}
}

static bool IsBoundAsReadable( const FRDGTexture* Texture, FShaderParameterStructRef ParameterStruct )
{
	for( int i = 0, Num = ParameterStruct.Layout->Resources.Num(); i < Num; i++ )
	{
		uint8  Type   = ParameterStruct.Layout->Resources[i];
		uint16 Offset = ParameterStruct.Layout->ResourceOffsets[i];

		switch( Type )
		{
			case UBMT_GRAPH_TRACKED_TEXTURE:
			{
				const FRDGTexture* InputTexture = *ParameterStruct.GetMemberPtrAtOffset<const FRDGTexture*>(Offset);
				if( Texture == InputTexture)
				{
					return true;
				}
			}
			break;
		case UBMT_GRAPH_TRACKED_SRV:
			{
				const FRDGTextureSRV* InputSRV = *ParameterStruct.GetMemberPtrAtOffset<const FRDGTextureSRV*>(Offset);
				if (InputSRV && Texture == InputSRV->Desc.Texture)
				{
					return true;
				}
			}
			break;
		default:
			break;
		}
	}

	return false;
}

void FRDGBuilder::ExecutePass( const FRenderGraphPass* Pass )
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FRDGBuilder_ExecutePass);

	FRHIRenderPassInfo RPInfo;
	bool bHasRenderTargets = false;

	AllocateAndTransitionPassResources(Pass, &RPInfo, &bHasRenderTargets);

	if( !Pass->IsCompute())
	{
		check(bHasRenderTargets);
		RHICmdList.BeginRenderPass( RPInfo, Pass->GetName() );
	}
	else
	{
		SetRenderTarget( RHICmdList, nullptr, nullptr );
	}
	
	Pass->Execute(RHICmdList);

	if( bHasRenderTargets )
	{
		RHICmdList.EndRenderPass();
	}

	if (RENDER_GRAPH_DEBUGGING)
	{
		WarnForUselessPassDependencies(Pass);
	}

	// Can't release resources with immediate mode, because don't know if whether they are gonna be used.
	if (!GRenderGraphImmediateMode)
	{
		ReleaseUnecessaryResources(Pass);
	}
}

void FRDGBuilder::AllocateAndTransitionPassResources(const FRenderGraphPass* Pass, struct FRHIRenderPassInfo* OutRPInfo, bool* bOutHasRenderTargets)
{
	bool bIsCompute = Pass->IsCompute();
	FShaderParameterStructRef ParameterStruct = Pass->GetParameters();

	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		uint8  Type = ParameterStruct.Layout->Resources[ResourceIndex];
		uint16 Offset = ParameterStruct.Layout->ResourceOffsets[ResourceIndex];

		switch (Type)
		{
		case UBMT_GRAPH_TRACKED_TEXTURE:
		{
			FRDGTexture* RESTRICT Texture = *ParameterStruct.GetMemberPtrAtOffset<FRDGTexture*>(Offset);
			if (Texture)
			{
				AllocateRHITextureIfNeeded(Texture, bIsCompute);
				TransitionTexture(Texture, EResourceTransitionAccess::EReadable, bIsCompute);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_SRV:
		{
			FRDGTextureSRV* RESTRICT SRV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureSRV*>(Offset);
			if (SRV)
			{
				check(SRV->Desc.Texture);

				const FRDGTexture* Texture = SRV->Desc.Texture;
				AllocateRHITextureIfNeeded(Texture, bIsCompute);
				TransitionTexture(Texture, EResourceTransitionAccess::EReadable, bIsCompute);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_UAV:
		{
			FRDGTextureUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureUAV*>(Offset);
			if (UAV)
			{
				AllocateRHITextureIfNeeded(UAV->Desc.Texture, bIsCompute);
				TransitionUAV(UAV, EResourceTransitionAccess::EWritable, bIsCompute);
			}
		}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			check(!bIsCompute);

			FRenderTargetBindingSlots* RESTRICT RenderTargets = ParameterStruct.GetMemberPtrAtOffset<FRenderTargetBindingSlots>(Offset);

			uint32 NumRenderTargets = 0;
			uint32 NumDepthStencilTargets = 0;
			uint32 NumSamples = 0;

			for (int i = 0; i < RenderTargets->Output.Num(); i++)
			{
				const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
				if (RenderTarget.GetTexture())
				{
					AllocateRHITextureIfNeeded(RenderTarget.GetTexture(), false);

					// TODO(RDG): should force TargetableTexture == ShaderResourceTexture with MSAA, and instead have an explicit MSAA resolve pass.
					OutRPInfo->ColorRenderTargets[i].RenderTarget = RenderTarget.GetTexture()->PooledRenderTarget->GetRenderTargetItem().TargetableTexture;
					OutRPInfo->ColorRenderTargets[i].ResolveTarget = nullptr;
					OutRPInfo->ColorRenderTargets[i].ArraySlice = -1;
					OutRPInfo->ColorRenderTargets[i].MipIndex = RenderTarget.GetMipIndex();
					OutRPInfo->ColorRenderTargets[i].Action = MakeRenderTargetActions(RenderTarget.GetLoadAction(), RenderTarget.GetStoreAction());

					TransitionTexture(RenderTarget.GetTexture(), EResourceTransitionAccess::EWritable, false);

					// TODO(RDG): There must be a better way to do this.
					OutRPInfo->bGeneratingMips = OutRPInfo->bGeneratingMips || IsBoundAsReadable(RenderTarget.GetTexture(), ParameterStruct);

					NumSamples |= OutRPInfo->ColorRenderTargets[i].RenderTarget->GetNumSamples();
					NumRenderTargets++;
				}
				else
				{
					break;
				}
			}

			const FDepthStencilBinding& DepthStencil = RenderTargets->DepthStencil;
			if (DepthStencil.Texture)
			{
				AllocateRHITextureIfNeeded(DepthStencil.Texture, false);

				OutRPInfo->DepthStencilRenderTarget.DepthStencilTarget = DepthStencil.Texture->PooledRenderTarget->GetRenderTargetItem().TargetableTexture;
				OutRPInfo->DepthStencilRenderTarget.ResolveTarget = nullptr;
				OutRPInfo->DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(
					MakeRenderTargetActions(DepthStencil.DepthLoadAction, DepthStencil.DepthStoreAction),
					MakeRenderTargetActions(DepthStencil.StencilLoadAction, DepthStencil.StencilStoreAction));
				OutRPInfo->DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;

				TransitionTexture(DepthStencil.Texture, EResourceTransitionAccess::EWritable, false);

				NumSamples |= OutRPInfo->DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples();
				NumDepthStencilTargets++;
			}

			OutRPInfo->bIsMSAA = NumSamples > 1;

			*bOutHasRenderTargets = NumRenderTargets + NumDepthStencilTargets > 0;
		}
		break;
		default:
			break;
		}
	}
}

// static 
void FRDGBuilder::WarnForUselessPassDependencies(const FRenderGraphPass* Pass)
{
	FShaderParameterStructRef ParameterStruct = Pass->GetParameters();

	int32 TrackedResourceCount = 0;
	int32 UsedResourceCount = 0;

	// First pass to count resources.
	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		uint8 Type = ParameterStruct.Layout->Resources[ResourceIndex];
		uint16 Offset = ParameterStruct.Layout->ResourceOffsets[ResourceIndex];

		if (Type != UBMT_GRAPH_TRACKED_TEXTURE && Type != UBMT_GRAPH_TRACKED_SRV && Type != UBMT_GRAPH_TRACKED_UAV)
			continue;

		const FRDGResource* Resource = *ParameterStruct.GetMemberPtrAtOffset<const FRDGResource*>(Offset);

		if (!Resource)
			continue;

		TrackedResourceCount++;
		UsedResourceCount += Resource->bIsActuallyUsedByPass ? 1 : 0;
	}

	if (TrackedResourceCount != UsedResourceCount)
	{
		UE_LOG(LogRendererCore, Warning, TEXT("%i of the %i resources of the pass %s where not actually used."), TrackedResourceCount - UsedResourceCount, TrackedResourceCount, Pass->GetName());

		for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
		{
			uint8 Type = ParameterStruct.Layout->Resources[ResourceIndex];
			uint16 Offset = ParameterStruct.Layout->ResourceOffsets[ResourceIndex];

			if (Type != UBMT_GRAPH_TRACKED_TEXTURE && Type != UBMT_GRAPH_TRACKED_SRV && Type != UBMT_GRAPH_TRACKED_UAV)
				continue;

			const FRDGResource* Resource = *ParameterStruct.GetMemberPtrAtOffset<const FRDGResource*>(Offset);

			if (!Resource)
				continue;

			UE_LOG(LogRendererCore, Warning, TEXT("	%s"), Resource->Name);
		}
	}

	// Last pass to clean the bIsActuallyUsedByPass flags.
	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		uint8 Type = ParameterStruct.Layout->Resources[ResourceIndex];
		uint16 Offset = ParameterStruct.Layout->ResourceOffsets[ResourceIndex];

		if (Type != UBMT_GRAPH_TRACKED_TEXTURE && Type != UBMT_GRAPH_TRACKED_SRV && Type != UBMT_GRAPH_TRACKED_UAV)
			continue;

		const FRDGResource* Resource = *ParameterStruct.GetMemberPtrAtOffset<const FRDGResource*>(Offset);

		if (!Resource)
			continue;

		Resource->bIsActuallyUsedByPass = false;
	}
}

void FRDGBuilder::ReleaseRHITextureIfPossible(const FRDGTexture* Texture)
{
	check(Texture->ReferenceCount > 0);
	Texture->ReferenceCount--;

	if (Texture->ReferenceCount == 0)
	{
		Texture->PooledRenderTarget = nullptr;
		AllocatedTextures.FindChecked(Texture) = nullptr;
	}
}

void FRDGBuilder::ReleaseUnecessaryResources(const FRenderGraphPass* Pass)
{
	FShaderParameterStructRef ParameterStruct = Pass->GetParameters();

	/** Increments all the FRDGResource::ReferenceCount. */
	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		uint8  Type = ParameterStruct.Layout->Resources[ResourceIndex];
		uint16 Offset = ParameterStruct.Layout->ResourceOffsets[ResourceIndex];

		switch (Type)
		{
		case UBMT_GRAPH_TRACKED_TEXTURE:
		{
			FRDGTexture* RESTRICT Texture = *ParameterStruct.GetMemberPtrAtOffset<FRDGTexture*>(Offset);
			if (Texture)
			{
				ReleaseRHITextureIfPossible(Texture);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_SRV:
		{
			FRDGTextureSRV* RESTRICT SRV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureSRV*>(Offset);
			if (SRV)
			{
				ReleaseRHITextureIfPossible(SRV->Desc.Texture);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_UAV:
		{
			FRDGTextureUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureUAV*>(Offset);
			if (UAV)
			{
				ReleaseRHITextureIfPossible(UAV->Desc.Texture);
			}
		}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			FRenderTargetBindingSlots* RESTRICT RenderTargets = ParameterStruct.GetMemberPtrAtOffset<FRenderTargetBindingSlots>(Offset);

			for (int i = 0; i < RenderTargets->Output.Num(); i++)
			{
				const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
				if (RenderTarget.GetTexture())
				{
					ReleaseRHITextureIfPossible(RenderTarget.GetTexture());
				}
				else
				{
					break;
				}
			}
			if (RenderTargets->DepthStencil.Texture)
			{
				ReleaseRHITextureIfPossible(RenderTargets->DepthStencil.Texture);
			}
		}
		break;
		default:
			break;
		}
	}
}

void FRDGBuilder::ProcessDeferredInternalResourceQueries()
{
	for (const auto& Query : DeferredInternalTextureQueries)
	{
		check(Query.Texture->PooledRenderTarget);

		if (Query.bTransitionToRead)
		{
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Query.Texture->PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture);
		}

		*Query.OutTexturePtr = AllocatedTextures.FindChecked(Query.Texture);

		// No need to manually release in immediate mode, since it is done directly when emptying AllocatedTextures in DestructPasses().
		if (!GRenderGraphImmediateMode)
		{
			ReleaseRHITextureIfPossible(Query.Texture);
		}
	}
}

void FRDGBuilder::DestructPasses()
{
	#if RENDER_GRAPH_DEBUGGING
	{
		// Make sure all resource references have been released to ensure no leaks happen.
		for (const FRDGResource* Resource : Resources)
		{
			check(Resource->ReferenceCount == 0);
		}
		Resources.Empty();
	}
	#endif

	// Passes are allocated on FMemStack, so need to call destructor manually.
	for (FRenderGraphPass* Pass : Passes)
	{
		Pass->~FRenderGraphPass();
	}

	Passes.Empty();
	DeferredInternalTextureQueries.Empty();
	AllocatedTextures.Empty();
}