// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphResources.h"
#include "ShaderParameterMacros.h"


/** Whether visualize texture tool is supported. */
#define SUPPORTS_VISUALIZE_TEXTURE (WITH_ENGINE && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))


/** Whether render graph should support draw events or not.
 * RENDER_GRAPH_DRAW_EVENTS == 0 means there is no string processing at all.
 * RENDER_GRAPH_DRAW_EVENTS == 1 means const TCHAR* is only passdown.
 * RENDER_GRAPH_DRAW_EVENTS == 2 means complex formated FString is passdown.
 */
#if WITH_PROFILEGPU
	#define RENDER_GRAPH_DRAW_EVENTS 2
#else
	#define RENDER_GRAPH_DRAW_EVENTS 0
#endif


/** Opaque object to store a draw event. */
class RENDERCORE_API FRDGEventName
{
public:
	inline FRDGEventName() 
	{ }

	#if RENDER_GRAPH_DRAW_EVENTS == 2

	explicit FRDGEventName(const TCHAR* EventFormat, ...);

	#elif RENDER_GRAPH_DRAW_EVENTS == 1

	explicit inline FRDGEventName(const TCHAR* EventFormat)
		: EventName(EventFormat)
	{ }

	#endif

	const TCHAR* GetTCHAR() const
	{
		#if RENDER_GRAPH_DRAW_EVENTS == 2
			return *EventName;
		#elif RENDER_GRAPH_DRAW_EVENTS == 1
			return EventName;
		#else
			return TEXT("UnknownRDVEvent");
		#endif
	}

private:
	#if RENDER_GRAPH_DRAW_EVENTS == 2
		FString EventName;
	#elif RENDER_GRAPH_DRAW_EVENTS == 1
		const TCHAR* EventName;
	#endif
};


/** Hierarchical scope for draw events of passes. */
class RENDERCORE_API FRDGEventScope
{
private:
	// Pointer towards this one is contained in.
	const FRDGEventScope* const ParentScope;

	// Name of the event.
	const FRDGEventName Name;


	FRDGEventScope(const FRDGEventScope* InParentScope, FRDGEventName&& InName)
		: ParentScope(InParentScope), Name(InName)
	{ }


	friend class FRDGBuilder;
	friend class FStackRDGEventScopeRef;
};


/** Flags to anotate passes. */
enum class ERenderGraphPassFlags
{
	None = 0,

	/** Pass uses compute only */
	Compute = 1 << 0,

	//#todo-rco: Remove this when we can do split/per mip layout transitions.
	/** Hint to some RHIs this pass will be generating mips to optimize transitions. */
	GenerateMips = 1 << 1,
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
	FRenderGraphPass(FRDGEventName&& InName, const FRDGEventScope* InParentScope, FShaderParameterStructRef InParameterStruct, ERenderGraphPassFlags InPassFlags)
		: Name(static_cast<FRDGEventName&&>(InName))
		, ParentScope(InParentScope)
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

	const TCHAR* GetName() const {
		return Name.GetTCHAR();
	}

	const ERenderGraphPassFlags& GetFlags() const
	{
		return PassFlags;
	}

	bool IsCompute() const {
		return (PassFlags & ERenderGraphPassFlags::Compute) == ERenderGraphPassFlags::Compute;
	}
	
	FShaderParameterStructRef GetParameters() const { return ParameterStruct; }

protected:
	const FRDGEventName Name;
	const FRDGEventScope* const ParentScope;
	const FShaderParameterStructRef ParameterStruct;
	const ERenderGraphPassFlags PassFlags;

	friend class FRDGBuilder;
};

/** 
 * Render graph pass with lambda execute function
 */
template <typename ParameterStructType, typename ExecuteLambdaType>
struct TLambdaRenderPass final : public FRenderGraphPass
{
	TLambdaRenderPass(FRDGEventName&& InName, const FRDGEventScope* InParentScope, FShaderParameterStructRef InParameterStruct, ERenderGraphPassFlags InPassFlags, ExecuteLambdaType&& InExecuteLambda)
		: FRenderGraphPass(static_cast<FRDGEventName&&>(InName), InParentScope, InParameterStruct, InPassFlags)
		, ExecuteLambda(static_cast<ExecuteLambdaType&&>(InExecuteLambda))
	{ }

	~TLambdaRenderPass()
	{
		// Manually call the destructor of the pass parameter, to make sure RHI references are released since the pass parameters are allocated on FMemStack.
		// TODO(RDG): this may lead to RHI resource leaks if a struct allocated in FMemStack does not actually get used through FRDGBuilder::AddPass().
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
class RENDERCORE_API FRDGBuilder
{
public:
	/** A RHI cmd list is required, if using the immediate mode. */
	FRDGBuilder(FRHICommandListImmediate& InRHICmdList)
		: RHICmdList(InRHICmdList)
	{
		for (int32 i = 0; i < kMaxScopeCount; i++)
			ScopesStack[i] = nullptr;
	}

	~FRDGBuilder();

	/** Register a external texture to be tracked by the render graph. */
	inline FRDGTextureRef RegisterExternalTexture(const TRefCountPtr<IPooledRenderTarget>& ExternalPooledTexture, const TCHAR* DebugName = TEXT("External"))
	{
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(ExternalPooledTexture.IsValid(), TEXT("Attempted to register NULL external texture: %s"), DebugName);
			checkf(DebugName, TEXT("Externally allocated texture requires a debug name when registering them to render graph."));
		}
		#endif
		FRDGTexture* OutTexture = AllocateForRHILifeTime<FRDGTexture>(DebugName, ExternalPooledTexture->GetDesc());
		OutTexture->PooledRenderTarget = ExternalPooledTexture;
		OutTexture->CachedRHI.Texture = ExternalPooledTexture->GetRenderTargetItem().ShaderResourceTexture;
		AllocatedTextures.Add(OutTexture, ExternalPooledTexture);
		#if RENDER_GRAPH_DEBUGGING
		{
			OutTexture->bHasEverBeenProduced = true;
			Resources.Add(OutTexture);
		}
		#endif
		return OutTexture;
	}

	/** Create graph tracked resource from a descriptor with a debug name.
	 *
	 * The debug name is the name used for GPU debugging tools, but also for the VisualizeTexture/Vis command.
	 */
	inline FRDGTextureRef CreateTexture(const FPooledRenderTargetDesc& Desc, const TCHAR* DebugName)
	{
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(!bHasExecuted, TEXT("Render graph texture %s needs to be created before the builder execution."), DebugName);
			checkf(DebugName, TEXT("Creating a render graph texture requires a valid debug name."));
			checkf(Desc.Format != PF_Unknown, TEXT("Illegal to create texture %s with an invalid pixel format."), DebugName);
		}
		#endif
		FRDGTexture* Texture = AllocateForRHILifeTime<FRDGTexture>(DebugName, Desc);
		#if RENDER_GRAPH_DEBUGGING
			Resources.Add(Texture);
		#endif
		return Texture;
	}

	/** Create graph tracked resource from a descriptor with a debug name.
	 *
	 * The debug name is the name used for GPU debugging tools, but also for the VisualizeTexture/Vis command.
	 */
	inline FRDGBufferRef CreateBuffer(const FRDGBufferDesc& Desc, const TCHAR* DebugName)
	{
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(!bHasExecuted, TEXT("Render graph buffer %s needs to be created before the builder execution."), DebugName);
			checkf(DebugName, TEXT("Creating a render graph buffer requires a valid debug name."));
		}
		#endif
		FRDGBuffer* Buffer = AllocateForRHILifeTime<FRDGBuffer>(DebugName, Desc);
		#if RENDER_GRAPH_DEBUGGING
			Resources.Add(Buffer);
		#endif
		return Buffer;
	}

	/** Create graph tracked SRV for a texture from a descriptor. */
	inline FRDGTextureSRVRef CreateSRV(const FRDGTextureSRVDesc& Desc)
	{
		check(Desc.Texture);
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(!bHasExecuted, TEXT("Render graph SRV %s needs to be created before the builder execution."), Desc.Texture->Name);
			ensureMsgf(Desc.Texture->Desc.TargetableFlags & TexCreate_ShaderResource, TEXT("Attempted to create SRV from texture %s which was not created with TexCreate_ShaderResource"), Desc.Texture->Name);
		}
		#endif
		
		FRDGTextureSRV* SRV = AllocateForRHILifeTime<FRDGTextureSRV>(Desc.Texture->Name, Desc);
		#if RENDER_GRAPH_DEBUGGING
			Resources.Add(SRV);
		#endif
		return SRV;
	}

	/** Create graph tracked SRV for a buffer from a descriptor. */
	inline FRDGBufferSRVRef CreateSRV(const FRDGBufferSRVDesc& Desc)
	{
		check(Desc.Buffer);
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(!bHasExecuted, TEXT("Render graph SRV %s needs to be created before the builder execution."), Desc.Buffer->Name);
		}
		#endif
		
		FRDGBufferSRV* SRV = AllocateForRHILifeTime<FRDGBufferSRV>(Desc.Buffer->Name, Desc);
		#if RENDER_GRAPH_DEBUGGING
			Resources.Add(SRV);
		#endif
		return SRV;
	}

	/** Create graph tracked UAV for a texture from a descriptor. */
	inline FRDGTextureUAVRef CreateUAV(const FRDGTextureUAVDesc& Desc)
	{
		check(Desc.Texture);
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(!bHasExecuted, TEXT("Render graph UAV %s needs to be created before the builder execution."), Desc.Texture->Name);
			ensureMsgf(Desc.Texture->Desc.TargetableFlags & TexCreate_UAV, TEXT("Attempted to create UAV from texture %s which was not created with TexCreate_UAV"), Desc.Texture->Name);
		}
		#endif
		
		FRDGTextureUAV* UAV = AllocateForRHILifeTime<FRDGTextureUAV>(Desc.Texture->Name, Desc);
		#if RENDER_GRAPH_DEBUGGING
			Resources.Add(UAV);
		#endif
		return UAV;
	}

	/** Create graph tracked UAV for a buffer from a descriptor. */
	inline FRDGBufferUAVRef CreateUAV(const FRDGBufferUAVDesc& Desc)
	{
		check(Desc.Buffer);
		#if RENDER_GRAPH_DEBUGGING
		{
			ensureMsgf(!bHasExecuted, TEXT("Render graph UAV %s needs to be created before the builder execution."), Desc.Buffer->Name);
		}
		#endif
		
		FRDGBufferUAV* UAV = AllocateForRHILifeTime<FRDGBufferUAV>(Desc.Buffer->Name, Desc);
		#if RENDER_GRAPH_DEBUGGING
			Resources.Add(UAV);
		#endif
		return UAV;
	}

	inline FRDGBufferUAVRef CreateUAV(FRDGBufferRef Buffer, EPixelFormat Format)
	{
		return CreateUAV( FRDGBufferUAVDesc(Buffer, Format) );
	}

	/** Allocates parameter struct specifically to survive through the life time of the render graph. */
	template< typename ParameterStructType >
	inline ParameterStructType* AllocParameters()
	{
		// TODO(RDG): could allocate using AllocateForRHILifeTime() to avoid the copy done when using FRHICommandList::BuildLocalUniformBuffer()
		ParameterStructType* OutParameterPtr = new(FMemStack::Get()) ParameterStructType;
		FMemory::Memzero(OutParameterPtr, sizeof(ParameterStructType));
		#if RENDER_GRAPH_DEBUGGING
		{
			AllocatedUnusedPassParameters.Add(static_cast<void *>(OutParameterPtr));
		}
		#endif
		return OutParameterPtr;
	}

	/** Adds a hard coded lambda pass to the graph.
	 *
	 * The Name of the pass should be generated with enough information to identify it's purpose and GPU cost, to be clear
	 * for GPU profiling tools.
	 *
	 * Caution: The pass parameter will be validated, and should not longer be modified after this call, since the pass may be executed
	 * right away with the immediate debugging mode.
	 * TODO(RDG): Verify with hashing.
	 */
	template<typename ParameterStructType, typename ExecuteLambdaType>
	void AddPass(
		FRDGEventName&& Name, 
		ParameterStructType* ParameterStruct,
		ERenderGraphPassFlags Flags,
		ExecuteLambdaType&& ExecuteLambda)
	{
		#if RENDER_GRAPH_DEBUGGING
		{
			checkf(!bHasExecuted, TEXT("Render graph pass %s needs to be added before the builder execution."), Name.GetTCHAR());

			/** A pass parameter structure requires a correct life time until the pass execution, and therefor needs to be
			 * allocated with FRDGBuilder::AllocParameters().
			 *
			 * Moreover, because the destructor of this parameter structure will be done after the pass execution, a it can
			 * only be used by a single AddPass().
			 */
			checkf(
				AllocatedUnusedPassParameters.Contains(static_cast<void *>(ParameterStruct)),
				TEXT("The pass parameter structure has not been alloctaed for correct life time FRDGBuilder::AllocParameters() or has already ")
				TEXT("been used by another previous FRDGBuilder::AddPass()."));

			AllocatedUnusedPassParameters.Remove(static_cast<void *>(ParameterStruct));
		}
		#endif

		auto NewPass = new(FMemStack::Get()) TLambdaRenderPass<ParameterStructType, ExecuteLambdaType>(
			static_cast<FRDGEventName&&>(Name), CurrentScope,
			{ ParameterStruct, &ParameterStructType::FTypeInfo::GetStructMetadata()->GetLayout() },
			Flags,
			static_cast<ExecuteLambdaType&&>(ExecuteLambda) );
		Passes.Emplace(NewPass);

		#if RENDER_GRAPH_DEBUGGING || SUPPORTS_VISUALIZE_TEXTURE
		{
			DebugPass(NewPass);
		}
		#endif
	}

	/** Adds a procedurally created pass to the render graph.
	 *
	 * Note: You want to use this only when the layout of the pass might be procedurally generated from data driven, as opose to AddPass() that have,
	 * constant hard coded pass layout.
	 *
	 * Caution: You are on your own to have correct memory lifetime of the FRenderGraphPass.
	 */
	void AddProcedurallyCreatedPass(FRenderGraphPass* NewPass)
	{
		#if RENDER_GRAPH_DEBUGGING
		{
			checkf(!bHasExecuted, TEXT("Render graph pass %s needs to be added before the builder execution."), NewPass->GetName());
		}
		#endif

		// TODO(RDG): perhaps the CurrentScope could be set here instead of GetCurrentScope(), to not allow user code to start adding pass to random scopes. 
		Passes.Emplace(NewPass);

		#if RENDER_GRAPH_DEBUGGING || SUPPORTS_VISUALIZE_TEXTURE
		{
			DebugPass(NewPass);
		}
		#endif
	}

	/** Queue a texture extraction. This will set *OutTexturePtr with the internal pooled render target at the Execute().
	 *
	 * Note: even when the render graph uses the immediate debugging mode (executing passes as they get added), the texture extrations
	 * will still happen in the Execute(), to ensure there is no bug caused in code outside the render graph on whether this mode is used or not.
	 */
	inline void QueueTextureExtraction(FRDGTextureRef Texture, TRefCountPtr<IPooledRenderTarget>* OutTexturePtr, bool bTransitionToRead = true)
	{
		check(Texture);
		check(OutTexturePtr);
		#if RENDER_GRAPH_DEBUGGING
		{
			checkf(!bHasExecuted,
				TEXT("Accessing render graph internal texture %s with QueueTextureExtraction() needs to happen before the builder's execution."),
				Texture->Name);

			checkf(Texture->bHasEverBeenProduced,
				TEXT("Unable to queue the extraction of the texture %s because it has not been produced by any pass."),
				Texture->Name);
		}
		#endif
		FDeferredInternalTextureQuery Query;
		Query.Texture = Texture;
		Query.OutTexturePtr = OutTexturePtr;
		Query.bTransitionToRead = bTransitionToRead;
		DeferredInternalTextureQueries.Emplace(Query);
	}

	/** Flag a texture that is only produced by only 1 pass, but never used or extracted, to avoid generating a warning at runtime. */
	FORCEINLINE_DEBUGGABLE void RemoveUnusedTextureWarning(FRDGTextureRef Texture)
	{
		check(Texture);
		#if RENDER_GRAPH_DEBUGGING
		{
			checkf(!bHasExecuted,
				TEXT("Flaging texture %s with FlagUnusedTexture() needs to happen before the builder's execution."),
				Texture->Name);
			
			// Increment the number of time the texture has been accessed to avoid warning on produced but never used resources that were produced
			// only to be extracted for the graph.
			Texture->DebugPassAccessCount += 1;
		}
		#endif
	}

	/** 
	 * Executes the queued passes, managing setting of render targets (RHI RenderPasses), resource transitions and queued texture extraction.
	 */
	void Execute();

	/** Returns the draw event scope, where passes are currently being added in. */
	const FRDGEventScope* GetCurrentScope() const
	{
		return CurrentScope;
	}

public:
	/** The RHI command list used for the render graph. */
	FRHICommandListImmediate& RHICmdList;


private:
	static constexpr int32 kMaxScopeCount = 8;

	/** Array of all pass created */
	TArray<FRenderGraphPass*, SceneRenderingAllocator> Passes;

	/** Keep the references over the pooled render target, since FRDGTexture is allocated on FMemStack. */
	TMap<const FRDGTexture*, TRefCountPtr<IPooledRenderTarget>, SceneRenderingSetAllocator> AllocatedTextures;

	/** Keep the references over the pooled render target, since FRDGTexture is allocated on FMemStack. */
	TMap<const FRDGBuffer*, TRefCountPtr<FPooledRDGBuffer>, SceneRenderingSetAllocator> AllocatedBuffers;

	/** Array of all deferred access to internal textures. */
	struct FDeferredInternalTextureQuery
	{
		const FRDGTexture* Texture;
		TRefCountPtr<IPooledRenderTarget>* OutTexturePtr;
		bool bTransitionToRead;
	};
	TArray<FDeferredInternalTextureQuery, SceneRenderingAllocator> DeferredInternalTextureQueries;

	#if RENDER_GRAPH_DRAW_EVENTS == 2
		/** All scopes allocated that needs to be arround to call destructors. */
		TArray<FRDGEventScope*, SceneRenderingAllocator> EventScopes;
	#endif

	/** The current event scope as creating passes. */
	const FRDGEventScope* CurrentScope = nullptr;

	/** Stacks of scopes pushed to the RHI command list. */
	TStaticArray<const FRDGEventScope*, kMaxScopeCount> ScopesStack;

	#if RENDER_GRAPH_DEBUGGING
		/** Whether the Execute() has already been called. */
		bool bHasExecuted = false;

		/** Lists of all created resources */
		TArray<const FRDGResource*, SceneRenderingAllocator> Resources;

		// All recently allocated pass parameter structure, but not used by a AddPass() yet.
		TSet<void*> AllocatedUnusedPassParameters;
	#endif

	void DebugPass(const FRenderGraphPass* Pass);
	void ValidatePass(const FRenderGraphPass* Pass) const;
	void CaptureAnyInterestingPassOutput(const FRenderGraphPass* Pass);

	void WalkGraphDependencies();
	
	template<class Type, class ...ConstructorParameterTypes>
	Type* AllocateForRHILifeTime(ConstructorParameterTypes&&... ConstructorParameters)
	{
		check(IsInRenderingThread());
		// When bypassing the RHI command queuing, can allocate directly on render thread memory stack allocator, otherwise allocate
		// on the RHI's stack allocator so RHICreateUniformBuffer() can dereference render graph resources.
		if (RHICmdList.Bypass() || 1) // TODO: UE-68018
		{
			return new (FMemStack::Get()) Type(Forward<ConstructorParameterTypes>(ConstructorParameters)...);
		}
		else
		{
			void* UnitializedType = RHICmdList.Alloc<Type>();
			return new (UnitializedType) Type(Forward<ConstructorParameterTypes>(ConstructorParameters)...);
		}
	}

	void AllocateRHITextureIfNeeded(const FRDGTexture* Texture, bool bComputePass);
	void AllocateRHITextureUAVIfNeeded(const FRDGTextureUAV* UAV, bool bComputePass);
	void AllocateRHIBufferSRVIfNeeded(const FRDGBufferSRV* SRV, bool bComputePass);
	void AllocateRHIBufferUAVIfNeeded(const FRDGBufferUAV* UAV, bool bComputePass);


	void TransitionTexture( const FRDGTexture* Texture, EResourceTransitionAccess TransitionAccess, bool bRequiredCompute ) const;
	void TransitionUAV(FUnorderedAccessViewRHIParamRef UAV, const FRDGResource* UnderlyingResource, EResourceTransitionAccess TransitionAccess, bool bRequiredCompute ) const;

	void PushDrawEventStack(const FRenderGraphPass* Pass);
	void ExecutePass( const FRenderGraphPass* Pass );
	void AllocateAndTransitionPassResources(const FRenderGraphPass* Pass, struct FRHIRenderPassInfo* OutRPInfo, bool* bOutHasRenderTargets);
	static void WarnForUselessPassDependencies(const FRenderGraphPass* Pass);

	void ReleaseRHITextureIfPossible(const FRDGTexture* Texture);
	void ReleaseRHIBufferIfPossible(const FRDGBuffer* Buffer);
	void ReleaseUnecessaryResources(const FRenderGraphPass* Pass);

	void ProcessDeferredInternalResourceQueries();
	void DestructPasses();

	friend class FStackRDGEventScopeRef;

	/** To allow greater flexibility in the user code, the RHI can dereferenced RDG resource when creating uniform buffer. */
	// TODO(RDG): Make this a little more explicit in RHI code.
	static_assert(STRUCT_OFFSET(FRDGResource, CachedRHI) == 0, "FRDGResource::CachedRHI requires to be at offset 0 so the RHI can dereferenced them.");
}; // class FRDGBuilder


#if RENDER_GRAPH_DRAW_EVENTS

/** Stack reference of render graph scope. */
class RENDERCORE_API FStackRDGEventScopeRef
{
public:
	FStackRDGEventScopeRef() = delete;
	FStackRDGEventScopeRef(const FStackRDGEventScopeRef&) = delete;
	FStackRDGEventScopeRef(FStackRDGEventScopeRef&&) = delete;
	void operator = (const FStackRDGEventScopeRef&) = delete;

	inline FStackRDGEventScopeRef(FRDGBuilder& InGraphBuilder, FRDGEventName&& ScopeName)
		: GraphBuilder(InGraphBuilder)
	{
		checkf(!GraphBuilder.bHasExecuted, TEXT("Render graph bulider has already been executed."));

		auto NewScope = new(FMemStack::Get()) FRDGEventScope(GraphBuilder.CurrentScope, Forward<FRDGEventName>(ScopeName));

		#if RENDER_GRAPH_DRAW_EVENTS == 2
		{
			GraphBuilder.EventScopes.Add(NewScope);
		}
		#endif

		GraphBuilder.CurrentScope = NewScope;
	}

	inline ~FStackRDGEventScopeRef()
	{
		check(GraphBuilder.CurrentScope != nullptr);
		GraphBuilder.CurrentScope = GraphBuilder.CurrentScope->ParentScope;
	}

private:
	FRDGBuilder& GraphBuilder;
};

#endif // RENDER_GRAPH_DRAW_EVENTS


/** Macros for create render graph event names and scopes.
 *
 *		FRDGEventName Name = RDG_EVENT_NAME("MyPass %sx%s", ViewRect.Width(), ViewRect.Height());
 *
 *		RDG_EVENT_SCOPE(GraphBuilder, "MyProcessing %sx%s", ViewRect.Width(), ViewRect.Height());
 */
#if RENDER_GRAPH_DRAW_EVENTS == 2

#define RDG_EVENT_NAME(Format, ...) FRDGEventName(TEXT(Format), ##__VA_ARGS__)
#define RDG_EVENT_SCOPE(GraphBuilder, Format, ...) \
	FStackRDGEventScopeRef PREPROCESSOR_JOIN(__RDG_ScopeRef_,__LINE__) ((GraphBuilder), RDG_EVENT_NAME(Format, ##__VA_ARGS__))

#elif RENDER_GRAPH_DRAW_EVENTS == 1

#define RDG_EVENT_NAME(Format, ...) FRDGEventName(TEXT(Format))
#define RDG_EVENT_SCOPE(GraphBuilder, Format, ...) \
	FStackRDGEventScopeRef PREPROCESSOR_JOIN(__RDG_ScopeRef_,__LINE__) ((GraphBuilder), RDG_EVENT_NAME(Format, ##__VA_ARGS__))

#else // !RENDER_GRAPH_DRAW_EVENTS

#define RDG_EVENT_NAME(Format, ...) FRDGEventName()
#define RDG_EVENT_SCOPE(GraphBuilder, Format, ...) 

#endif // !RENDER_GRAPH_DRAW_EVENTS
