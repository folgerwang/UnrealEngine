// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphResources.h"
#include "ShaderParameterMacros.h"
#include "RendererInterface.h"


/** Whether render graph debugging is compiled. */
#define RENDER_GRAPH_DEBUGGING (DO_CHECK)


/** Flags to anotate passes. */
enum class ERenderGraphPassFlags
{
	None = 0,

	/** Pass uses compute only */
	Compute = 1 << 0,
};

ENUM_CLASS_FLAGS(ERenderGraphPassFlags)


// TODO(RDG): remove from global scope?
struct RENDERCORE_API FShaderParameterStructRef
{
	const void*						Contents;
	const FRHIUniformBufferLayout*	Layout;

	template<typename MemberType>
	MemberType* GetMemberPtrAtOffset(uint16 Offset) const
	{
		return reinterpret_cast<MemberType*>(((uint8*)Contents) + Offset);
	}
};


/** 
 * Base class of a render graph pass
 */
struct RENDERCORE_API FRenderGraphPass
{
	FRenderGraphPass(const TCHAR* InName, FShaderParameterStructRef InParameterStruct, ERenderGraphPassFlags InPassFlags)
		: Name(InName)
		, ParameterStruct(InParameterStruct)
		, PassFlags(InPassFlags)
	{
		if (IsCompute())
		{
			ensureMsgf(ParameterStruct.Layout->NumRenderTargets() == 0, TEXT("Pass %s was declared as ERenderGraphPassFlags::Compute yet has RenderTargets in its ResourceTable"), GetName());
		}
	}

	virtual ~FRenderGraphPass() {}

	virtual void Execute(FRHICommandListImmediate& RHICmdList) const = 0;

	const TCHAR*			GetName() const { return Name; }
	ERenderGraphPassFlags	GetFlags() const { return PassFlags; }
	bool					IsCompute() const { return (PassFlags & ERenderGraphPassFlags::Compute) == ERenderGraphPassFlags::Compute; }
	
	FShaderParameterStructRef GetParameters() const { return ParameterStruct; }

protected:
	const TCHAR* const Name;
	FShaderParameterStructRef ParameterStruct;
	const ERenderGraphPassFlags PassFlags;
};

/** 
 * Render graph pass with lambda execute function
 */
template <typename ParameterStructType, typename ExecuteLambdaType>
struct TLambdaRenderPass final : public FRenderGraphPass
{
	TLambdaRenderPass(const TCHAR* InName, FShaderParameterStructRef InParameterStruct, ERenderGraphPassFlags InPassFlags, ExecuteLambdaType&& InExecuteLambda)
		: FRenderGraphPass(InName, InParameterStruct, InPassFlags)
		, ExecuteLambda(static_cast<ExecuteLambdaType&&>(InExecuteLambda))
	{
	}

	~TLambdaRenderPass()
	{
		// Manually call the destructor of the pass parameter, to make sure RHI references are released since the pass parameters are allocated on FMemStack.
		// TODO(RDG): this may lead to RHI resource leaks if a struct allocated in FMemStack does not actually get used through FRenderGraphBuilder::AddPass().
		ParameterStructType* Struct = reinterpret_cast<ParameterStructType*>(const_cast<void*>(FRenderGraphPass::ParameterStruct.Contents));
		Struct->~ParameterStructType();
	}

	virtual void Execute(FRHICommandListImmediate& RHICmdList) const override
	{
		ExecuteLambda(RHICmdList);
	}

	ExecuteLambdaType ExecuteLambda;
};

/** 
 * Builds the per-frame render graph.
 * Resources must be created from the builder before they can be bound to Pass ResourceTables.
 * These resources are descriptors only until the graph is executed, where RHI resources are allocated as needed.
 */
class RENDERCORE_API FRenderGraphBuilder
{
public:
	/** A RHI cmd list is required, if using the immediate mode. */
	FRenderGraphBuilder(FRHICommandListImmediate& InRHICmdList)
		: RHICmdList(InRHICmdList)
	{ }

	~FRenderGraphBuilder()
	{
		DestructPasses();
	}

	/** Register a external texture to be tracked by the render graph. */
	inline const FGraphTexture* RegisterExternalTexture(const TRefCountPtr<IPooledRenderTarget>& ExternalPooledTexture, const TCHAR* Name = TEXT("External"))
	{
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(ExternalPooledTexture.IsValid(), TEXT("Attempted to register NULL external texture: %s"), Name);
		}
		#endif
		FGraphTexture* OutTexture = new(FMemStack::Get()) FGraphTexture(Name, ExternalPooledTexture->GetDesc());
		OutTexture->PooledRenderTarget = ExternalPooledTexture;
		AllocatedTextures.Add(OutTexture, ExternalPooledTexture);
		#if DO_CHECK
			Resources.Add(OutTexture);
		#endif
		return OutTexture;
	}

	/** Create graph tracked resource from a descriptor with a debug name. */
	inline const FGraphTexture* CreateTexture(const FPooledRenderTargetDesc& Desc, const TCHAR* DebugName)
	{
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(!bHasExecuted, TEXT("Render graph texture %s needs to be created before the builder execution."), DebugName);
		}
		#endif
		FGraphTexture* Texture = new(FMemStack::Get()) FGraphTexture(DebugName, Desc);
		#if DO_CHECK
			Resources.Add(Texture);
		#endif
		return Texture;
	}

	/** Create graph tracked SRV from a descriptor. */
	inline const FGraphSRV* CreateSRV(const FGraphSRVDesc& Desc)
	{
		check(Desc.Texture);
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(!bHasExecuted, TEXT("Render graph SRV %s needs to be created before the builder execution."), Desc.Texture->Name);
			ensureMsgf(Desc.Texture->Desc.TargetableFlags & TexCreate_ShaderResource, TEXT("Attempted to create SRV from texture %s which was not created with TexCreate_ShaderResource"), Desc.Texture->Name);
		}
		#endif
		
		FGraphSRV* SRV = new(FMemStack::Get()) FGraphSRV(Desc.Texture->Name, Desc);
		#if DO_CHECK
			Resources.Add(SRV);
		#endif
		return SRV;
	}

	/** Create graph tracked UAV from a descriptor. */
	inline const FGraphUAV* CreateUAV(const FGraphUAVDesc& Desc)
	{
		check(Desc.Texture);
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(!bHasExecuted, TEXT("Render graph UAV %s needs to be created before the builder execution."), Desc.Texture->Name);
			ensureMsgf(Desc.Texture->Desc.TargetableFlags & TexCreate_UAV, TEXT("Attempted to create UAV from texture %s which was not created with TexCreate_UAV"), Desc.Texture->Name);
		}
		#endif
		
		FGraphUAV* UAV = new(FMemStack::Get()) FGraphUAV(Desc.Texture->Name, Desc);
		#if DO_CHECK
			Resources.Add(UAV);
		#endif
		return UAV;
	}

	/** Allocates parameter struct specifically to survive through the life time of the render graph. */
	template< typename ParameterStructType >
	inline void CreateParameters(ParameterStructType** OutParameterPtr) const
	{
		// Check because destructor called by the pass's destructor.
		#if RENDER_GRAPH_DEBUGGING
		{
			checkf(!bHasExecuted, TEXT("Render graph allocated shader parameters %s needs to be allocated before the builder execution."), ParameterStructType::FTypeInfo::GetStructMetadata()->GetStructTypeName());
		}
		#endif
		*OutParameterPtr = new(FMemStack::Get()) ParameterStructType;
		FMemory::Memzero( *OutParameterPtr, sizeof( ParameterStructType ) );
	}

	/** 
	 * Adds a lambda pass to the graph.
	 */
	template<typename ParameterStructType, typename ExecuteLambdaType>
	void AddPass(
		const TCHAR* Name, 
		ParameterStructType* ParameterStruct,
		ERenderGraphPassFlags Flags,
		ExecuteLambdaType&& ExecuteLambda)
	{
		#if RENDER_GRAPH_DEBUGGING
		{
			checkf(!bHasExecuted, TEXT("Render graph pass %s needs to be added before the builder execution."), Name);
		}
		#endif
		auto NewPass = new(FMemStack::Get()) TLambdaRenderPass<ParameterStructType, ExecuteLambdaType>(
			Name,
			{ ParameterStruct, &ParameterStructType::FTypeInfo::GetStructMetadata()->GetLayout() },
			Flags,
			static_cast<ExecuteLambdaType&&>(ExecuteLambda) );
		Passes.Emplace(NewPass);
		if (DO_CHECK)
		{
			DebugPass(NewPass);
		}
	}

	/**
	 * Extracts an internal texture by handle.  Must be called before Execute.
	 */
	inline void GetInternalTexture(const FGraphTexture* Texture, TRefCountPtr<IPooledRenderTarget>* OutTexturePtr, bool bTransitionToRead = true)
	{
		check(Texture);
		check(OutTexturePtr);
		#if RENDER_GRAPH_DEBUGGING
		{
			checkf(!bHasExecuted, TEXT("Accessing render graph internal texture %s needs to happen before the builder's execution."), Texture->Name);
		}
		#endif
		FDeferredInternalTextureQuery Query;
		Query.Texture = Texture;
		Query.OutTexturePtr = OutTexturePtr;
		Query.bTransitionToRead = bTransitionToRead;
		DeferredInternalTextureQueries.Emplace(Query);
	}

	/** 
	 * Executes the queued passes, managing setting of render targets (RHI RenderPasses) and resource transitions.
	 */
	void Execute();

private:
	/** The RHI command list to use. */
	FRHICommandListImmediate& RHICmdList;

	/** Array of all pass created */
	TArray<FRenderGraphPass*, SceneRenderingAllocator> Passes;

	/** Keep the references over the pooled render target, since FGraphTexture is allocated on FMemStack. */
	TMap<const FGraphTexture*, TRefCountPtr<IPooledRenderTarget>, SceneRenderingSetAllocator> AllocatedTextures;

	/** Array of all deferred access to internal textures. */
	struct FDeferredInternalTextureQuery
	{
		const FGraphTexture* Texture;
		TRefCountPtr<IPooledRenderTarget>* OutTexturePtr;
		bool bTransitionToRead;
	};
	TArray<FDeferredInternalTextureQuery, SceneRenderingAllocator> DeferredInternalTextureQueries;


	#if RENDER_GRAPH_DEBUGGING
		/** Whether the Execute() has already been called. */
		bool bHasExecuted = false;

		/** Lists of all created resources */
		TArray<const FGraphResource*, SceneRenderingAllocator> Resources;
	#endif

	void DebugPass(const FRenderGraphPass* Pass);
	void ValidatePass(const FRenderGraphPass* Pass) const;

	void WalkGraphDependencies();

	void AllocateRHITextureIfNeeded(const FGraphTexture* Texture, bool bComputePass);

	void TransitionTexture( const FGraphTexture* Texture, EResourceTransitionAccess TransitionAccess, bool bRequiredCompute ) const;
	void TransitionUAV( const FGraphUAV* UAV, EResourceTransitionAccess TransitionAccess, bool bRequiredCompute ) const;

	void ExecutePass( const FRenderGraphPass* Pass );
	void AllocateAndTransitionPassResources(const FRenderGraphPass* Pass, struct FRHIRenderPassInfo* OutRPInfo, bool* bOutHasRenderTargets);
	static void WarnForUselessPassDependencies(const FRenderGraphPass* Pass);
	void ReleaseRHITextureIfPossible(const FGraphTexture* Texture);
	void ReleaseUnecessaryResources(const FRenderGraphPass* Pass);

	void ProcessDeferredInternalResourceQueries();
	void DestructPasses();
}; // class FRenderGraphBuilder
