// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalConstantBuffer.cpp: Metal Constant buffer implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "MetalBuffer.h"
#include "MetalCommandBuffer.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/ScopeRWLock.h"

struct FMetalRHICommandInitialiseUniformBufferIAB : public FRHICommand<FMetalRHICommandInitialiseUniformBufferIAB>
{
	TRefCountPtr<FMetalUniformBuffer> Buffer;
	
	FORCEINLINE_DEBUGGABLE FMetalRHICommandInitialiseUniformBufferIAB(FMetalUniformBuffer* InBuffer)
	: Buffer(InBuffer)
	{
	}
	
	virtual ~FMetalRHICommandInitialiseUniformBufferIAB()
	{
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		Buffer->InitIAB();
	}
};

struct FMetalArgumentDesc
{
	FMetalArgumentDesc()
	: DataType((mtlpp::DataType)0)
	, Index(0)
	, ArrayLength(0)
	, Access(mtlpp::ArgumentAccess::ReadOnly)
	, TextureType((mtlpp::TextureType)0)
	, ConstantBlockAlignment(0)
	{
		
	}
	
	mtlpp::DataType DataType;
	NSUInteger Index;
	NSUInteger ArrayLength;
	mtlpp::ArgumentAccess Access;
	mtlpp::TextureType TextureType;
	NSUInteger ConstantBlockAlignment;
	
	void FillDescriptor(mtlpp::ArgumentDescriptor& Desc) const
	{
		Desc.SetDataType(DataType);
		Desc.SetIndex(Index);
		Desc.SetArrayLength(ArrayLength);
		Desc.SetAccess(Access);
		Desc.SetTextureType(TextureType);
		Desc.SetConstantBlockAlignment(ConstantBlockAlignment);
	}
	
	void SetDataType(mtlpp::DataType Type)
	{
		DataType = Type;
	}
	
	void SetIndex(NSUInteger i)
	{
		Index = i;
	}
	
	void SetArrayLength(NSUInteger Len)
	{
		ArrayLength = Len;
	}
	
	void SetAccess(mtlpp::ArgumentAccess InAccess)
	{
		Access = InAccess;
	}
	
	void SetTextureType(mtlpp::TextureType Type)
	{
		TextureType = Type;
	}
	
	void SetConstantBlockAlignment(NSUInteger Align)
	{
		ConstantBlockAlignment = Align;
	}
	
	bool operator==(FMetalArgumentDesc const& Other) const
	{
		if (this != &Other)
		{
			return (DataType == Other.DataType && Index == Other.Index && ArrayLength == Other.ArrayLength && Access == Other.Access && TextureType == Other.TextureType && ConstantBlockAlignment == Other.ConstantBlockAlignment);
		}
		return true;
	}
	
	friend uint32 GetTypeHash(FMetalArgumentDesc const& Desc)
	{
		uint32 Hash = 0;
		Hash ^= ((uint32)Desc.DataType * (uint32)Desc.TextureType * (uint32)Desc.Access * Desc.ArrayLength) << Desc.Index;
		return Hash;
	}
};

struct FMetalArgumentEncoderMapFuncs : TDefaultMapKeyFuncs<TArray<FMetalArgumentDesc>, mtlpp::ArgumentEncoder, /*bInAllowDuplicateKeys*/false>
{
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		uint32 Hash = 0;
		
		for (FMetalArgumentDesc const& Desc : Key)
		{
			HashCombine(Hash, GetTypeHash(Desc));
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
	
	mtlpp::ArgumentEncoder CreateEncoder(TArray<FMetalArgumentDesc> const& Desc)
	{
		mtlpp::ArgumentEncoder Encoder;
		
		FRWScopeLock Lock(Mutex, SLT_ReadOnly);
		Encoder = Encoders.FindRef(Desc);
		
		if (!Encoder)
		{
			NSMutableArray<mtlpp::ArgumentDescriptor::Type>* Arguments = [[NSMutableArray new] autorelease];
			for (FMetalArgumentDesc const& Args : Desc)
			{
				mtlpp::ArgumentDescriptor Arg;
				Args.FillDescriptor(Arg);
				[Arguments addObject:Arg.GetPtr()];
			}
			
			Encoder = GetMetalDeviceContext().GetDevice().NewArgumentEncoderWithArguments(Arguments);
			
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
	TMap<TArray<FMetalArgumentDesc>, mtlpp::ArgumentEncoder, FDefaultSetAllocator, FMetalArgumentEncoderMapFuncs> Encoders;
};

FMetalUniformBuffer::FMetalUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage InUsage, EUniformBufferValidation Validation)
	: FRHIUniformBuffer(Layout)
    , FMetalRHIBuffer(Layout.ConstantBufferSize, (FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs) && Layout.Resources.Num() ? (EMetalBufferUsage_GPUOnly|BUF_Volatile) : BUF_Volatile), RRT_UniformBuffer)
	, UniformUsage(InUsage)
	, IAB(nullptr)
{
	uint32 NumResources = Layout.Resources.Num();
	if (NumResources)
	{
		ResourceTable.Empty(NumResources);
		ResourceTable.AddZeroed(NumResources);
	}
	
    Update(Contents, Validation);
	
    if (NumResources && FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs))
    {
        GetMetalDeviceContext().RegisterUB(this);
    }
}

FMetalUniformBuffer::~FMetalUniformBuffer()
{
	if (ResourceTable.Num() && FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs))
	{
		GetMetalDeviceContext().UnregisterUB(this);
	}
	
	if (IAB)
	{
		delete IAB;
		IAB = nullptr;
	}
}

FMetalUniformBuffer::FMetalIndirectArgumentBuffer::FMetalIndirectArgumentBuffer()
{
	
}

FMetalUniformBuffer::FMetalIndirectArgumentBuffer::~FMetalIndirectArgumentBuffer()
{
	SafeReleaseMetalBuffer(IndirectArgumentBuffer);
	SafeReleaseMetalBuffer(IndirectArgumentBufferSideTable);
}

FMetalUniformBuffer::FMetalIndirectArgumentBuffer& FMetalUniformBuffer::GetIAB()
{
	check(ResourceTable.Num() && FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs));

	InitIAB();
	check(IAB);
	
	return *IAB;
}

void FMetalUniformBuffer::InitIAB()
{
	int32 NumResources = ResourceTable.Num();
	if (NumResources && FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs) && !IAB)
	{
		FMetalIndirectArgumentBuffer* NewIAB = new FMetalIndirectArgumentBuffer;
		
		TArray<uint32> BufferSizes;
		TArray<FMetalArgumentDesc> Arguments;
		
		int32 Index = 0;
		for (int32 i = 0; i < NumResources; ++i, ++Index)
		{
			FRHIResource* Resource = ResourceTable[i].GetReference();
			
			switch(GetLayout().Resources[i].MemberType)
			{
				case UBMT_RDG_TEXTURE_SRV:
				case UBMT_RDG_BUFFER_SRV:
				case UBMT_SRV:
				{
					FMetalArgumentDesc& Desc = Arguments.Emplace_GetRef();
					Desc.SetIndex(Index);
					Desc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
					
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
						NewIAB->IndirectArgumentResources.Add(Argument(Surface->Texture, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Sample)));
					}
					else
					{
						check(VB || IB || SB);
						ns::AutoReleased<FMetalTexture> Tex = SRV->GetLinearTexture(false);
						Desc.SetDataType(mtlpp::DataType::Texture);
						Desc.SetTextureType(Tex.GetTextureType());
						NewIAB->IndirectArgumentResources.Add(Argument(Tex, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Sample)));
						
						FMetalArgumentDesc& BufferDesc = Arguments.Emplace_GetRef();
						BufferDesc.SetIndex(++Index);
						BufferDesc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
						
						if (VB)
						{
							BufferDesc.SetDataType(mtlpp::DataType::Pointer);
							NewIAB->IndirectArgumentResources.Add(Argument(VB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read)));
							
							check(VB->Buffer.GetStorageMode() == mtlpp::StorageMode::Private);
							BufferSizes.Add(VB->GetSize());
							BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
						}
						else if (IB)
						{
							BufferDesc.SetDataType(mtlpp::DataType::Pointer);
							NewIAB->IndirectArgumentResources.Add(Argument(IB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read)));
							
							check(IB->Buffer.GetStorageMode() == mtlpp::StorageMode::Private);
							BufferSizes.Add(IB->GetSize());
							BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
						}
						else if (SB)
						{
							BufferDesc.SetDataType(mtlpp::DataType::Pointer);
							NewIAB->IndirectArgumentResources.Add(Argument(SB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read)));
							
							BufferSizes.Add(SB->GetSize());
							BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
						}
					}
					break;
				}
				case UBMT_RDG_TEXTURE_UAV:
				case UBMT_RDG_BUFFER_UAV:
				{
					FMetalArgumentDesc& Desc = Arguments.Emplace_GetRef();
					Desc.SetIndex(Index);
					Desc.SetAccess(mtlpp::ArgumentAccess::ReadWrite);
					
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
						NewIAB->IndirectArgumentResources.Add(Argument(Surface->Texture, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Write)));
					}
					else
					{
						check(VB || IB || SB);
						ns::AutoReleased<FMetalTexture> Tex = SRV->GetLinearTexture(false);
						Desc.SetDataType(mtlpp::DataType::Texture);
						Desc.SetTextureType(Tex.GetTextureType());
						NewIAB->IndirectArgumentResources.Add(Argument(Tex, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Write)));
						
						FMetalArgumentDesc& BufferDesc = Arguments.Emplace_GetRef();
						BufferDesc.SetIndex(++Index);
						BufferDesc.SetAccess(mtlpp::ArgumentAccess::ReadWrite);
						
						if (VB)
						{
							BufferDesc.SetDataType(mtlpp::DataType::Pointer);
							NewIAB->IndirectArgumentResources.Add(Argument(VB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Write)));
							
							check(VB->Buffer.GetStorageMode() == mtlpp::StorageMode::Private);
							BufferSizes.Add(VB->GetSize());
							BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
						}
						else if (IB)
						{
							BufferDesc.SetDataType(mtlpp::DataType::Pointer);
							NewIAB->IndirectArgumentResources.Add(Argument(IB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Write)));
							
							check(IB->Buffer.GetStorageMode() == mtlpp::StorageMode::Private);
							BufferSizes.Add(IB->GetSize());
							BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
						}
						else if (SB)
						{
							BufferDesc.SetDataType(mtlpp::DataType::Pointer);
							NewIAB->IndirectArgumentResources.Add(Argument(SB->Buffer, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Write)));
							
							BufferSizes.Add(SB->GetSize());
							BufferSizes.Add(GMetalBufferFormats[SRV->Format].DataFormat);
						}
					}
					break;
				}
				case UBMT_SAMPLER:
				{
					FMetalArgumentDesc& Desc = Arguments.Emplace_GetRef();
					Desc.SetIndex(Index);
					Desc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
					
					FMetalSamplerState* Sampler = (FMetalSamplerState*)Resource;
					Desc.SetDataType(mtlpp::DataType::Sampler);
					NewIAB->IndirectArgumentResources.Add(Argument(Sampler->State));
					break;
				}
				case UBMT_RDG_TEXTURE:
				case UBMT_TEXTURE:
				{
					FMetalArgumentDesc& Desc = Arguments.Emplace_GetRef();
					Desc.SetIndex(Index);
					Desc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
					
					FRHITexture* Texture = (FRHITexture*)Resource;
					FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture);
					check (Surface != nullptr);
					Desc.SetDataType(mtlpp::DataType::Texture);
					Desc.SetTextureType(Surface->Texture.GetTextureType());
					
					check(!Surface->Texture.IsAliasable());
					NewIAB->IndirectArgumentResources.Add(Argument(Surface->Texture, (mtlpp::ResourceUsage)(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Sample)));
					break;
				}
				default:
					break;
			}
		}
		
		if (BufferSizes.Num() > 0)
		{
			FMetalArgumentDesc& Desc = Arguments.Emplace_GetRef();
			Desc.SetIndex(Index++);
			Desc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
			Desc.SetDataType(mtlpp::DataType::Pointer);
			
			FMetalPooledBufferArgs Args(GetMetalDeviceContext().GetDevice(), BufferSizes.Num() * sizeof(uint32), BUFFER_STORAGE_MODE);
			NewIAB->IndirectArgumentBufferSideTable = GetMetalDeviceContext().CreatePooledBuffer(Args);
			
			FMemory::Memcpy(NewIAB->IndirectArgumentBufferSideTable.GetContents(), BufferSizes.GetData(), BufferSizes.Num() * sizeof(uint32));
			
#if PLATFORM_MAC
			if(NewIAB->IndirectArgumentBufferSideTable.GetStorageMode() == mtlpp::StorageMode::Managed)
			{
				MTLPP_VALIDATE(mtlpp::Buffer, NewIAB->IndirectArgumentBufferSideTable, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, BufferSizes.Num() * sizeof(uint32))));
			}
#endif
			
			NewIAB->IndirectArgumentResources.Add(Argument(NewIAB->IndirectArgumentBufferSideTable, mtlpp::ResourceUsage::Read));
		}
		
		if (GetLayout().ConstantBufferSize > 0)
		{
			FMetalArgumentDesc& Desc = Arguments.Emplace_GetRef();
			Desc.SetIndex(Index++);
			Desc.SetAccess(mtlpp::ArgumentAccess::ReadOnly);
			Desc.SetDataType(mtlpp::DataType::Pointer);
			
			NewIAB->IndirectArgumentResources.Add(Argument(Buffer, mtlpp::ResourceUsage::Read));
		}
		
		mtlpp::ArgumentEncoder Encoder = FMetalArgumentEncoderCache::Get().CreateEncoder(Arguments);
		
		NewIAB->IndirectArgumentBuffer = GetMetalDeviceContext().GetResourceHeap().CreateBuffer(Encoder.GetEncodedLength(), 16, mtlpp::ResourceOptions(BUFFER_CACHE_MODE | ((NSUInteger)BUFFER_STORAGE_MODE << mtlpp::ResourceStorageModeShift)), true);
		
		Encoder.SetArgumentBuffer(NewIAB->IndirectArgumentBuffer, 0);
		
		for (FMetalArgumentDesc const& Arg : Arguments)
		{
			NSUInteger NewIndex = Arg.Index;
			switch(Arg.DataType)
			{
				case mtlpp::DataType::Pointer:
					Encoder.SetBuffer(NewIAB->IndirectArgumentResources[NewIndex].Buffer, 0, NewIndex);
					break;
				case mtlpp::DataType::Texture:
					Encoder.SetTexture(NewIAB->IndirectArgumentResources[NewIndex].Texture, NewIndex);
					break;
				case mtlpp::DataType::Sampler:
					Encoder.SetSamplerState(NewIAB->IndirectArgumentResources[NewIndex].Sampler, NewIndex);
					break;
				default:
					break;
			}
		}
		
#if PLATFORM_MAC
		if(NewIAB->IndirectArgumentBuffer.GetStorageMode() == mtlpp::StorageMode::Managed)
		{
			MTLPP_VALIDATE(mtlpp::Buffer, NewIAB->IndirectArgumentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, Encoder.GetEncodedLength())));
		}
#endif
		
		// Atomically swap so that we don't explode if multiple threads attempt to initialise at the same time.
		void* Result = FPlatformAtomics::InterlockedCompareExchangePointer((void**)&IAB, (void*)NewIAB, nullptr);
		if (Result != nullptr)
		{
			check(Result != NewIAB);
			delete NewIAB;
		}
	}
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

void FMetalUniformBuffer::Update(const void* Contents, EUniformBufferValidation Validation)
{
    const FRHIUniformBufferLayout Layout = GetLayout();
    if (Layout.ConstantBufferSize > 0)
    {
        UE_CLOG(Layout.ConstantBufferSize > 65536, LogMetal, Fatal, TEXT("Trying to allocated a uniform layout of size %d that is greater than the maximum permitted 64k."), Layout.ConstantBufferSize);
        
        void* Data = Lock(RLM_WriteOnly, 0);
        FMemory::Memcpy(Data, Contents, Layout.ConstantBufferSize);
        Unlock();
    }
    
    // set up an SRT-style uniform buffer
    if (Layout.Resources.Num())
    {
        int32 NumResources = Layout.Resources.Num();
        for (int32 i = 0; i < NumResources; ++i)
        {
            FRHIResource* Resource = *(FRHIResource**)((uint8*)Contents + Layout.Resources[i].MemberOffset);
            
            // Allow null SRV's in uniform buffers for feature levels that don't support SRV's in shaders
            if (Validation == EUniformBufferValidation::ValidateResources && !(GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 && Layout.Resources[i].MemberType == UBMT_SRV))
            {
                check(Resource);
            }
            
            ResourceTable[i] = Resource;
            
            if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs) && Resource)
            {
                switch(Layout.Resources[i].MemberType)
                {
                    case UBMT_RDG_TEXTURE_SRV:
                    case UBMT_RDG_BUFFER_SRV:
                    case UBMT_SRV:
                    {
                        FMetalShaderResourceView* SRV = (FMetalShaderResourceView*)Resource;
                        FRHITexture* Texture = SRV->SourceTexture.GetReference();
                        if (Texture && Texture->GetTextureReference())
                        {
                            TextureReferences.Add(Texture->GetTextureReference());
                        }
                        break;
                    }
                    case UBMT_RDG_TEXTURE_UAV:
                    case UBMT_RDG_BUFFER_UAV:
                    {
                        FMetalUnorderedAccessView* UAV = (FMetalUnorderedAccessView*)Resource;
                        FRHITexture* Texture = UAV->SourceView->SourceTexture.GetReference();
                        if (Texture && Texture->GetTextureReference())
                        {
                            TextureReferences.Add(Texture->GetTextureReference());
                        }
                        break;
                    }
                    case UBMT_RDG_TEXTURE:
                    case UBMT_TEXTURE:
                    {
                        FRHITexture* Texture = (FRHITexture*)Resource;
                        if (Texture && Texture->GetTextureReference())
                        {
                            TextureReferences.Add(Texture->GetTextureReference());
                        }
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
        }
        
        if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs))
        {
            FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
            if (!(UniformUsage & UniformBuffer_SingleDraw) && IsRunningRHIInSeparateThread() && !RHICmdList.Bypass() && IsInRenderingThread())
            {
                new (RHICmdList.AllocCommand<FMetalRHICommandInitialiseUniformBufferIAB>()) FMetalRHICommandInitialiseUniformBufferIAB(this);
            }
        }
    }
}

FUniformBufferRHIRef FMetalDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation)
{
	@autoreleasepool {
	check(IsInRenderingThread() || IsInParallelRenderingThread() || IsInRHIThread());
		return new FMetalUniformBuffer(Contents, Layout, Usage, Validation);
	}
}

struct FMetalRHICommandUpateUniformBuffer : public FRHICommand<FMetalRHICommandUpateUniformBuffer>
{
	TRefCountPtr<FMetalUniformBuffer> Buffer;
	char* Contents;
	
	FORCEINLINE_DEBUGGABLE FMetalRHICommandUpateUniformBuffer(FMetalUniformBuffer* InBuffer, void const* Data)
	: Buffer(InBuffer)
	, Contents(nullptr)
	{
		const FRHIUniformBufferLayout Layout = Buffer->GetLayout();
		uint32 MaxLayoutSize = Layout.ConstantBufferSize;
		for (int32 i = 0; i < Layout.Resources.Num(); ++i)
		{
			MaxLayoutSize = FMath::Max(MaxLayoutSize, (uint32)(Layout.Resources[i].MemberOffset + sizeof(FRHIResource*)));
		}
		Contents = new char[MaxLayoutSize];
		FMemory::Memcpy(Contents, Data, MaxLayoutSize);
	}
	
	virtual ~FMetalRHICommandUpateUniformBuffer()
	{
		delete [] Contents;
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		Buffer->Update(Contents, EUniformBufferValidation::None);
	}
};

void FMetalDynamicRHI::RHIUpdateUniformBuffer(FUniformBufferRHIParamRef UniformBufferRHI, const void* Contents)
{
	@autoreleasepool {
	// check((IsInRenderingThread() || IsInRHIThread()) && !IsInParallelRenderingThread());

	FMetalUniformBuffer* UniformBuffer = ResourceCast(UniformBufferRHI);
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		UniformBuffer->Update(Contents, EUniformBufferValidation::None);
	}
	else
	{
		new (RHICmdList.AllocCommand<FMetalRHICommandUpateUniformBuffer>()) FMetalRHICommandUpateUniformBuffer(UniformBuffer, Contents);
		RHICmdList.RHIThreadFence(true);
	}
	}
}
