// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"
#include "MetalCommandEncoder.h"
#include "MetalState.h"
#include "MetalFence.h"

class FMetalCommandList;
class FMetalCommandQueue;

class FMetalRenderPass
{
public:
#pragma mark - Public C++ Boilerplate -

	/** Default constructor */
	FMetalRenderPass(FMetalCommandList& CmdList, FMetalStateCache& StateCache);
	
	/** Destructor */
	~FMetalRenderPass(void);
	
#pragma mark -
    void Begin(mtlpp::Fence Fence);
	
	void Wait(mtlpp::Fence Fence);

	void Update(mtlpp::Fence Fence);
	
    void BeginRenderPass(mtlpp::RenderPassDescriptor RenderPass);

    void RestartRenderPass(mtlpp::RenderPassDescriptor RenderPass);
    
    void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances);
    
    void DrawPrimitiveIndirect(uint32 PrimitiveType, FMetalVertexBuffer* VertexBuffer, uint32 ArgumentOffset);
    
    void DrawIndexedPrimitive(FMetalBuffer const& IndexBuffer, uint32 IndexStride, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
                         uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances);
    
    void DrawIndexedIndirect(FMetalIndexBuffer* IndexBufferRHI, uint32 PrimitiveType, FMetalStructuredBuffer* VertexBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances);
    
    void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FMetalIndexBuffer* IndexBufferRHI,FMetalVertexBuffer* VertexBufferRHI,uint32 ArgumentOffset);
    
    void DrawPatches(uint32 PrimitiveType, FMetalBuffer const& IndexBuffer, uint32 IndexBufferStride, int32 BaseVertexIndex, uint32 FirstInstance, uint32 StartIndex,
                     uint32 NumPrimitives, uint32 NumInstances);
    
    void Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
    
    void DispatchIndirect(FMetalVertexBuffer* ArgumentBufferRHI, uint32 ArgumentOffset);
    
    mtlpp::Fence EndRenderPass(void);
    
    void CopyFromTextureToBuffer(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalBuffer const& toBuffer, uint32 destinationOffset, uint32 destinationBytesPerRow, uint32 destinationBytesPerImage, mtlpp::BlitOption options);
    
    void CopyFromBufferToTexture(FMetalBuffer const& Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin, mtlpp::BlitOption options);
    
    void CopyFromTextureToTexture(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin);
	
	void CopyFromBufferToBuffer(FMetalBuffer const& SourceBuffer, NSUInteger SourceOffset, FMetalBuffer const& DestinationBuffer, NSUInteger DestinationOffset, NSUInteger Size);
	
	void PresentTexture(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin);
    
    void SynchronizeTexture(FMetalTexture const& Texture, uint32 Slice, uint32 Level);
    
	void SynchroniseResource(mtlpp::Resource const& Resource);
    
	void FillBuffer(FMetalBuffer const& Buffer, ns::Range Range, uint8 Value);
	
	bool AsyncCopyFromBufferToTexture(FMetalBuffer const& Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin, mtlpp::BlitOption options);
	
	bool AsyncCopyFromTextureToTexture(FMetalTexture const& Texture, uint32 sourceSlice, uint32 sourceLevel, mtlpp::Origin sourceOrigin, mtlpp::Size sourceSize, FMetalTexture const& toTexture, uint32 destinationSlice, uint32 destinationLevel, mtlpp::Origin destinationOrigin);
	
	void AsyncCopyFromBufferToBuffer(FMetalBuffer const& SourceBuffer, NSUInteger SourceOffset, FMetalBuffer const& DestinationBuffer, NSUInteger DestinationOffset, NSUInteger Size);
	
	void AsyncGenerateMipmapsForTexture(FMetalTexture const& Texture);
	
    mtlpp::Fence Submit(EMetalSubmitFlags SubmissionFlags);
    
    mtlpp::Fence End(void);
	
	void InsertCommandBufferFence(FMetalCommandBufferFence& Fence, mtlpp::CommandBufferHandler Handler);
	
	void AddCompletionHandler(mtlpp::CommandBufferHandler Handler);
	
	void AddAsyncCommandBufferHandlers(mtlpp::CommandBufferHandler Scheduled, mtlpp::CommandBufferHandler Completion);

#pragma mark - Public Debug Support -
	
    /*
     * Inserts a debug compute encoder into the command buffer. This is how we generate a timestamp when no encoder exists.
     */
    void InsertDebugEncoder();
	
	/*
	 * Inserts a debug string into the command buffer.  This does not change any API behavior, but can be useful when debugging.
	 * @param string The name of the signpost. 
	 */
	void InsertDebugSignpost(ns::String const& String);
	
	/*
	 * Push a new named string onto a stack of string labels.
	 * @param string The name of the debug group. 
	 */
	void PushDebugGroup(ns::String const& String);
	
	/* Pop the latest named string off of the stack. */
	void PopDebugGroup(void);
    
#pragma mark - Public Accessors -
	
	/*
	 * Get the current internal command buffer.
	 * @returns The current command buffer.
	 */
	mtlpp::CommandBuffer const& GetCurrentCommandBuffer(void) const;
	mtlpp::CommandBuffer& GetCurrentCommandBuffer(void);
	
	/*
	 * Get the internal ring-buffer used for temporary allocations.
	 * @returns The temporary allocation buffer for the command-pass.
	 */
	FMetalSubBufferRing& GetRingBuffer(void);
	
private:
#pragma mark -
    void ConditionalSwitchToRender(void);
    void ConditionalSwitchToTessellation(void);
    void ConditionalSwitchToCompute(void);
	void ConditionalSwitchToBlit(void);
	void ConditionalSwitchToAsyncBlit(void);
	
    void PrepareToRender(uint32 PrimType);
    void PrepareToTessellate(uint32 PrimType);
    void PrepareToDispatch(void);

    void CommitRenderResourceTables(void);
    void CommitTessellationResourceTables(void);
    void CommitDispatchResourceTables(void);
    
    void ConditionalSubmit();
private:
#pragma mark -
	FMetalCommandList& CmdList;
    FMetalStateCache& State;
    
    // Which of the buffers/textures/sampler slots are bound
    // The state cache is responsible for ensuring we bind the correct 
    FMetalTextureMask BoundTextures[SF_NumFrequencies];
    uint32 BoundBuffers[SF_NumFrequencies];
    uint16 BoundSamplers[SF_NumFrequencies];
    
    FMetalCommandEncoder CurrentEncoder;
    FMetalCommandEncoder PrologueEncoder;
	
	// To ensure that buffer uploads aren't overwritten before they are used track what is in flight
	// Disjoint ranges *are* permitted!
	TMap<id<MTLBuffer>, TArray<NSRange>> OutstandingBufferUploads;
    
    FMetalFence PassStartFence;
    FMetalFence CurrentEncoderFence;
    FMetalFence PrologueEncoderFence;
    
    mtlpp::RenderPassDescriptor RenderPassDesc;
    
    uint32 NumOutstandingOps;
    bool bWithinRenderPass;
};
