// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalConstantBuffer.cpp: Metal Constant buffer implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "MetalBuffer.h"
#include "MetalCommandBuffer.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/ScopeRWLock.h"

struct FMetalRHICommandInitialiseUniformdBuffer : public FRHICommand<FMetalRHICommandInitialiseUniformdBuffer>
{
	TRefCountPtr<FMetalUniformBuffer> Buffer;
	
	FORCEINLINE_DEBUGGABLE FMetalRHICommandInitialiseUniformdBuffer(FMetalUniformBuffer* InBuffer)
	: Buffer(InBuffer)
	{
	}
	
	virtual ~FMetalRHICommandInitialiseUniformdBuffer()
	{
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		if (Buffer->CPUBuffer)
		{
			GetMetalDeviceContext().AsyncCopyFromBufferToBuffer(Buffer->CPUBuffer, 0, Buffer->Buffer, 0, Buffer->Buffer.GetLength());
			LLM_SCOPE(ELLMTag::VertexBuffer);
			SafeReleaseMetalBuffer(Buffer->CPUBuffer);
		}
		else if (GMetalBufferZeroFill && !FMetalCommandQueue::SupportsFeature(EMetalFeaturesFences))
		{
			GetMetalDeviceContext().FillBuffer(Buffer->Buffer, ns::Range(0, Buffer->Buffer.GetLength()), 0);
		}
	}
};

struct FMetalArgumentEncoderMapFuncs : TDefaultMapKeyFuncs<ns::Array<mtlpp::ArgumentDescriptor>, mtlpp::ArgumentEncoder, /*bInAllowDuplicateKeys*/false>
{
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		uint32 Hash = 0;
		
		for (ns::AutoReleased<mtlpp::ArgumentDescriptor> const& Desc : Key)
		{
			Hash ^= ((uint32)Desc.GetDataType() * (uint32)Desc.GetTextureType() * (uint32)Desc.GetAccess() * Desc.GetArrayLength()) << Desc.GetIndex();
		}
		
		return Hash;
	}
};

class FMetalArgumentEncoderCache
{
public:
	FMetalArgumentEncoderCache()
	{
	}
	
	~FMetalArgumentEncoderCache()
	{
	}
	
	static FMetalArgumentEncoderCache& Get()
	{
		static FMetalArgumentEncoderCache sSelf;
		return sSelf;
	}
	
	mtlpp::ArgumentEncoder CreateEncoder(ns::Array<mtlpp::ArgumentDescriptor> const& Desc)
	{
		mtlpp::ArgumentEncoder Encoder;
		
		FRWScopeLock Lock(Mutex, SLT_ReadOnly);
		Encoder = Encoders.FindRef(Desc);
		
		if (!Encoder)
		{
			Encoder = GetMetalDeviceContext().GetDevice().NewArgumentEncoderWithArguments(Desc);
			
			// Now we are a writer as we want to create & add the new pipeline
			Lock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
			
			if (!Encoders.Find(Desc))
			{
				Encoders.Add(Desc, Encoder);
			}
		}
		
		return Encoder;
	}
private:
	FRWLock Mutex;
	TMap<ns::Array<mtlpp::ArgumentDescriptor>, mtlpp::ArgumentEncoder, FDefaultSetAllocator, FMetalArgumentEncoderMapFuncs> Encoders;
};

FMetalUniformBuffer::FMetalUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage InUsage)
	: FRHIUniformBuffer(Layout)
    , FMetalRHIBuffer(Layout.ConstantBufferSize, (FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs) && Layout.Resources.Num() ? (EMetalBufferUsage_GPUOnly|BUF_Volatile) : BUF_Volatile), RRT_UniformBuffer)
{
	if (Layout.ConstantBufferSize > 0)
	{
		UE_CLOG(Layout.ConstantBufferSize > 65536, LogMetal, Fatal, TEXT("Trying to allocated a uniform layout of size %d that is greater than the maximum permitted 64k."), Layout.ConstantBufferSize);
		
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		if (!(InUsage & UniformBuffer_SingleDraw) && CPUBuffer && IsRunningRHIInSeparateThread() && !RHICmdList.Bypass() && !IsInRHIThread())
		{
			FMemory::Memcpy(CPUBuffer.GetContents(), Contents, Layout.ConstantBufferSize);

#if PLATFORM_MAC
			if(CPUBuffer.GetStorageMode() == mtlpp::StorageMode::Managed)
			{
				MTLPP_VALIDATE(mtlpp::Buffer, CPUBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, GMetalBufferZeroFill ? CPUBuffer.GetLength() : Layout.ConstantBufferSize)));
			}
#endif

			new (RHICmdList.AllocCommand<FMetalRHICommandInitialiseUniformdBuffer>()) FMetalRHICommandInitialiseUniformdBuffer(this);
		}
		else
		{
			void* Data = Lock(RLM_WriteOnly, 0);
			FMemory::Memcpy(Data, Contents, Layout.ConstantBufferSize);
			Unlock();
		}
	}

	// set up an SRT-style uniform buffer
	if (Layout.Resources.Num())
	{
		if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs))
		{
			TArray<id> Pointers;
			TArray<uint32> BufferSizes;
			NSMutableArray<mtlpp::ArgumentDescriptor::Type>* Arguments = [[NSMutableArray new] autorelease];
			
			int32 NumResources = Layout.Resources.Num();
			ResourceTable.Empty(NumResources);
			ResourceTable.AddZeroed(NumResources);
			int32 Index = 0;
			for (int32 i = 0; i < NumResources; ++i, ++Index)
			{
				FRHIResource* Resource = *(FRHIResource**)((uint8*)Contents + Layout.ResourceOffsets[i]);
				
				// Allow null SRV's in uniform buffers for feature levels that don't support SRV's in shaders
				if (!(GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 && Layout.Resources[i] == UBMT_SRV))
				{
					check(Resource);
				}
				ResourceTable[i] = Resource;
				
				switch(Layout.Resources[i])
				{
					case UBMT_GRAPH_TRACKED_SRV:
					case UBMT_GRAPH_TRACKED_BUFFER_SRV:
					case UBMT_SRV:
					{
						mtlpp::ArgumentDescriptor Desc;
						Desc.SetIndex(Index);
						Desc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
						[Arguments addObject:Desc.GetPtr()];
						
						FMetalShaderResourceView* SRV = (FMetalShaderResourceView*)Resource;
						FRHITexture* Texture = SRV->SourceTexture.GetReference();
						FMetalVertexBuffer* VB = SRV->SourceVertexBuffer.GetReference();
						FMetalIndexBuffer* IB = SRV->SourceIndexBuffer.GetReference();
						FMetalStructuredBuffer* SB = SRV->SourceStructuredBuffer.GetReference();
						if (Texture)
						{
							FMetalSurface* Surface = SRV->TextureView;
							check (Surface != nullptr);
							Desc.SetDataType(mtlpp::DataType::Texture);
							Desc.SetTextureType(Surface->Texture.GetTextureType());
							
							check(!Surface->Texture.IsAliasable());
							IndirectArgumentResources.Add(Argument(Surface->Texture, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Sample)));
							Pointers.Add(Surface->Texture.GetPtr());
						}
						else
						{
							check(VB || IB || SB);
							ns::AutoReleased<FMetalTexture> Tex = SRV->GetLinearTexture(false);
							Desc.SetDataType(mtlpp::DataType::Texture);
							Desc.SetTextureType(Tex.GetTextureType());
							Pointers.Add(Tex.GetPtr());
							IndirectArgumentResources.Add(Argument(Tex, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Sample)));
							
							mtlpp::ArgumentDescriptor BufferDesc;
							BufferDesc.SetIndex(++Index);
							BufferDesc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
							[Arguments addObject:BufferDesc.GetPtr()];
							
							if (VB)
							{
								BufferDesc.SetDataType(mtlpp::DataType::Pointer);
								Pointers.Add(VB->Buffer.GetPtr());
								IndirectArgumentResources.Add(Argument(VB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read)));
								
								check(VB->Buffer.GetStorageMode() == mtlpp::StorageMode::Private);
								BufferSizes.Add(VB->GetSize());
								BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
							}
							else if (IB)
							{
								BufferDesc.SetDataType(mtlpp::DataType::Pointer);
								Pointers.Add(IB->Buffer.GetPtr());
								IndirectArgumentResources.Add(Argument(IB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read)));
								
								check(IB->Buffer.GetStorageMode() == mtlpp::StorageMode::Private);
								BufferSizes.Add(IB->GetSize());
								BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
							}
							else if (SB)
							{
								BufferDesc.SetDataType(mtlpp::DataType::Pointer);
								Pointers.Add(SB->Buffer.GetPtr());
								IndirectArgumentResources.Add(Argument(SB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read)));
								
								BufferSizes.Add(SB->GetSize());
								BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
							}
						}
						break;
					}
					case UBMT_GRAPH_TRACKED_UAV:
					case UBMT_GRAPH_TRACKED_BUFFER_UAV:
					{
						mtlpp::ArgumentDescriptor Desc;
						Desc.SetIndex(Index);
						Desc.SetAccess(mtlpp::ArgumentAccess::ReadWrite);
						[Arguments addObject:Desc.GetPtr()];
						
						FMetalUnorderedAccessView* UAV = (FMetalUnorderedAccessView*)Resource;
						FMetalShaderResourceView* SRV = (FMetalShaderResourceView*)UAV->SourceView;
						FMetalStructuredBuffer* SB = UAV->SourceView->SourceStructuredBuffer.GetReference();
						FMetalVertexBuffer* VB = UAV->SourceView->SourceVertexBuffer.GetReference();
						FMetalIndexBuffer* IB = UAV->SourceView->SourceIndexBuffer.GetReference();
						FRHITexture* Texture = UAV->SourceView->SourceTexture.GetReference();
						FMetalSurface* Surface = UAV->SourceView->TextureView;
						if (Texture)
						{
							if (!Surface)
							{
								Surface = GetMetalSurfaceFromRHITexture(Texture);
							}
							check (Surface != nullptr);
							Desc.SetDataType(mtlpp::DataType::Texture);
							Desc.SetTextureType(Surface->Texture.GetTextureType());
							
							check(!Surface->Texture.IsAliasable());
							Pointers.Add(Surface->Texture.GetPtr());
							IndirectArgumentResources.Add(Argument(Surface->Texture, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Write)));
						}
						else
						{
							check(VB || IB || SB);
							ns::AutoReleased<FMetalTexture> Tex = SRV->GetLinearTexture(false);
							Desc.SetDataType(mtlpp::DataType::Texture);
							Desc.SetTextureType(Tex.GetTextureType());
							Pointers.Add(Tex.GetPtr());
							IndirectArgumentResources.Add(Argument(Tex, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Write)));
							
							mtlpp::ArgumentDescriptor BufferDesc;
							BufferDesc.SetIndex(++Index);
							BufferDesc.SetAccess(mtlpp::ArgumentAccess::ReadWrite);
							[Arguments addObject:BufferDesc.GetPtr()];
							
							if (VB)
							{
								BufferDesc.SetDataType(mtlpp::DataType::Pointer);
								Pointers.Add(VB->Buffer.GetPtr());
								IndirectArgumentResources.Add(Argument(VB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Write)));
								
								check(VB->Buffer.GetStorageMode() == mtlpp::StorageMode::Private);
								BufferSizes.Add(VB->GetSize());
								BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
							}
							else if (IB)
							{
								BufferDesc.SetDataType(mtlpp::DataType::Pointer);
								Pointers.Add(IB->Buffer.GetPtr());
								IndirectArgumentResources.Add(Argument(IB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Write)));
								
								check(IB->Buffer.GetStorageMode() == mtlpp::StorageMode::Private);
								BufferSizes.Add(IB->GetSize());
								BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
							}
							else if (SB)
							{
								BufferDesc.SetDataType(mtlpp::DataType::Pointer);
								Pointers.Add(SB->Buffer.GetPtr());
								IndirectArgumentResources.Add(Argument(SB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Write)));
								
								BufferSizes.Add(SB->GetSize());
								BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
							}
						}
						break;
					}
					case UBMT_SAMPLER:
					{
						mtlpp::ArgumentDescriptor Desc;
						Desc.SetIndex(Index);
						Desc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
						[Arguments addObject:Desc.GetPtr()];
						
						FMetalSamplerState* Sampler = (FMetalSamplerState*)Resource;
						Desc.SetDataType(mtlpp::DataType::Sampler);
						Pointers.Add(Sampler->State.GetPtr());
						IndirectArgumentResources.Add(Argument());
						break;
					}
					case UBMT_GRAPH_TRACKED_TEXTURE:
					case UBMT_TEXTURE:
					{
						mtlpp::ArgumentDescriptor Desc;
						Desc.SetIndex(Index);
						Desc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
						[Arguments addObject:Desc.GetPtr()];
						
						FRHITexture* Texture = (FRHITexture*)Resource;
						FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture);
						check (Surface != nullptr);
						Desc.SetDataType(mtlpp::DataType::Texture);
						Desc.SetTextureType(Surface->Texture.GetTextureType());
						
						check(!Surface->Texture.IsAliasable());
						Pointers.Add(Surface->Texture.GetPtr());
						IndirectArgumentResources.Add(Argument(Surface->Texture, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Sample)));
						break;
					}
					default:
						break;
				}
			}
			
			if (BufferSizes.Num() > 0)
			{
				mtlpp::ArgumentDescriptor Desc;
				Desc.SetIndex(Index++);
				Desc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
				Desc.SetDataType(mtlpp::DataType::Pointer);
				[Arguments addObject:Desc.GetPtr()];
				
				FMetalPooledBufferArgs Args(GetMetalDeviceContext().GetDevice(), BufferSizes.Num() * sizeof(uint32), BUFFER_STORAGE_MODE);
				IndirectArgumentBufferSideTable = GetMetalDeviceContext().CreatePooledBuffer(Args);
				
				FMemory::Memcpy(IndirectArgumentBufferSideTable.GetContents(), BufferSizes.GetData(), BufferSizes.Num() * sizeof(uint32));
				
#if PLATFORM_MAC
				if(IndirectArgumentBufferSideTable.GetStorageMode() == mtlpp::StorageMode::Managed)
				{
					MTLPP_VALIDATE(mtlpp::Buffer, IndirectArgumentBufferSideTable, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, BufferSizes.Num() * sizeof(uint32))));
				}
#endif
				
				IndirectArgumentResources.Add(Argument(IndirectArgumentBufferSideTable, mtlpp::ResourceUsage::Read));
				Pointers.Add(IndirectArgumentBufferSideTable.GetPtr());
			}
			
			if (Layout.ConstantBufferSize > 0)
            {
                mtlpp::ArgumentDescriptor Desc;
                Desc.SetIndex(Index++);
                Desc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
                Desc.SetDataType(mtlpp::DataType::Pointer);
                [Arguments addObject:Desc.GetPtr()];
                
                Pointers.Add(Buffer.GetPtr());
                IndirectArgumentResources.Add(Argument(Buffer, mtlpp::ResourceUsage::Read));
			}
			
			ns::Array<mtlpp::ArgumentDescriptor> ArgArray(Arguments);
			mtlpp::ArgumentEncoder Encoder = FMetalArgumentEncoderCache::Get().CreateEncoder(ArgArray);
			
			FMetalPooledBufferArgs Args(GetMetalDeviceContext().GetDevice(), Encoder.GetEncodedLength(), BUFFER_STORAGE_MODE);
			IndirectArgumentBuffer = GetMetalDeviceContext().CreatePooledBuffer(Args);
			Encoder.SetArgumentBuffer(IndirectArgumentBuffer, 0);
			
			for (MTLArgumentDescriptor* Arg in Arguments)
			{
				mtlpp::ArgumentDescriptor Argument(Arg);
				NSUInteger NewIndex = Argument.GetIndex();
				switch(Argument.GetDataType())
				{
					case mtlpp::DataType::Pointer:
						Encoder.SetBuffer((id<MTLBuffer>)Pointers[NewIndex], 0, NewIndex);
						break;
					case mtlpp::DataType::Texture:
						Encoder.SetTexture((id<MTLTexture>)Pointers[NewIndex], NewIndex);
						break;
					case mtlpp::DataType::Sampler:
						Encoder.SetSamplerState((id<MTLSamplerState>)Pointers[NewIndex], NewIndex);
						break;
					case mtlpp::DataType::Struct:
					{
						void* Data = Encoder.GetConstantDataAtIndex(NewIndex);
						FMemory::Memcpy(Data, Contents, Layout.ConstantBufferSize);
						break;
					}
					default:
						break;
				}
			}
			
#if PLATFORM_MAC
			if(IndirectArgumentBuffer.GetStorageMode() == mtlpp::StorageMode::Managed)
			{
				MTLPP_VALIDATE(mtlpp::Buffer, IndirectArgumentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, Encoder.GetEncodedLength())));
			}
#endif
		}
		else
		{
			int32 NumResources = Layout.Resources.Num();
			ResourceTable.Empty(NumResources);
			ResourceTable.AddZeroed(NumResources);
			for (int32 i = 0; i < NumResources; ++i)
			{
				FRHIResource* Resource = *(FRHIResource**)((uint8*)Contents + Layout.ResourceOffsets[i]);
				
				// Allow null SRV's in uniform buffers for feature levels that don't support SRV's in shaders
				if (!(GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 && Layout.Resources[i] == UBMT_SRV))
				{
					check(Resource);
				}
				ResourceTable[i] = Resource;
			}
		}
	}
}

FMetalUniformBuffer::~FMetalUniformBuffer()
{
	SafeReleaseMetalBuffer(IndirectArgumentBuffer);
	SafeReleaseMetalBuffer(IndirectArgumentBufferSideTable);
}

void const* FMetalUniformBuffer::GetData()
{
	if (Data)
	{
		return Data->Data;
	}
	else if (Buffer)
	{
		return MTLPP_VALIDATE(mtlpp::Buffer, Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GetContents());
	}
	else
	{
		return nullptr;
	}
}

FUniformBufferRHIRef FMetalDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
{
	@autoreleasepool {
	check(IsInRenderingThread() || IsInParallelRenderingThread() || IsInRHIThread());
	return new FMetalUniformBuffer(Contents, Layout, Usage);
	}
}
