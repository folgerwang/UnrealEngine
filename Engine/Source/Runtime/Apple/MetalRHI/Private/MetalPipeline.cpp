// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalPipeline.cpp: Metal shader pipeline RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalPipeline.h"
#include "MetalShaderResources.h"
#include "MetalResources.h"
#include "MetalProfiler.h"
#include "MetalCommandQueue.h"
#include "MetalCommandBuffer.h"
#include "RenderUtils.h"
#include "Misc/ScopeRWLock.h"
#include <objc/runtime.h>

static int32 GMetalTessellationForcePartitionMode = 0;
static FAutoConsoleVariableRef CVarMetalTessellationForcePartitionMode(
	TEXT("rhi.Metal.TessellationForcePartitionMode"),
	GMetalTessellationForcePartitionMode,
	TEXT("The partition mode (+1) to force Metal to use for debugging or off (0). (Default: 0)"));

static uint32 BlendBitOffsets[] = { Offset_BlendState0, Offset_BlendState1, Offset_BlendState2, Offset_BlendState3, Offset_BlendState4, Offset_BlendState5, Offset_BlendState6, Offset_BlendState7 };
static uint32 RTBitOffsets[] = { Offset_RenderTargetFormat0, Offset_RenderTargetFormat1, Offset_RenderTargetFormat2, Offset_RenderTargetFormat3, Offset_RenderTargetFormat4, Offset_RenderTargetFormat5, Offset_RenderTargetFormat6, Offset_RenderTargetFormat7 };
static_assert(Offset_RasterEnd < 64 && Offset_End < 128, "Offset_RasterEnd must be < 64 && Offset_End < 128");

static float RoundUpNearestEven(const float f)
{
	const float ret = FMath::CeilToFloat(f);
	const float isOdd = (float)(((int)ret) & 1);
	return ret + isOdd;
}

static float RoundTessLevel(float TessFactor, mtlpp::TessellationPartitionMode PartitionMode)
{
	switch(PartitionMode)
	{
		case mtlpp::TessellationPartitionMode::ModePow2:
			return FMath::RoundUpToPowerOfTwo((uint32)TessFactor);
		case mtlpp::TessellationPartitionMode::ModeInteger:
			return FMath::CeilToFloat(TessFactor);
		case mtlpp::TessellationPartitionMode::ModeFractionalEven:
		case mtlpp::TessellationPartitionMode::ModeFractionalOdd: // these are handled the same way
			return RoundUpNearestEven(TessFactor);
		default:
			check(false);
			return 0.0f;
	}
}

@implementation FMetalShaderPipeline

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalShaderPipeline)

- (void)dealloc
{
	[super dealloc];
}

#if METAL_DEBUG_OPTIONS
- (instancetype)init
{
	id Self = [super init];
	if (Self)
	{
		RenderPipelineReflection = mtlpp::RenderPipelineReflection(nil);
		ComputePipelineReflection = mtlpp::ComputePipelineReflection(nil);
		RenderDesc = mtlpp::RenderPipelineDescriptor(nil);
		ComputeDesc = mtlpp::ComputePipelineDescriptor(nil);
	}
	return Self;
}

- (void)initResourceMask
{
	if (RenderPipelineReflection)
	{
		[self initResourceMask:EMetalShaderVertex];
		[self initResourceMask:EMetalShaderFragment];
	}
	if (ComputePipelineReflection)
	{
		[self initResourceMask:EMetalShaderCompute];
	}
}
- (void)initResourceMask:(EMetalShaderFrequency)Frequency
{
	NSArray<MTLArgument*>* Arguments = nil;
	switch(Frequency)
	{
		case EMetalShaderVertex:
		{
			MTLRenderPipelineReflection* Reflection = RenderPipelineReflection;
			check(Reflection);
			
			Arguments = Reflection.vertexArguments;
			break;
		}
		case EMetalShaderFragment:
		{
			MTLRenderPipelineReflection* Reflection = RenderPipelineReflection;
			check(Reflection);
			
			Arguments = Reflection.fragmentArguments;
			break;
		}
		case EMetalShaderCompute:
		{
			MTLComputePipelineReflection* Reflection = ComputePipelineReflection;
			check(Reflection);
			
			Arguments = Reflection.arguments;
			break;
		}
		default:
			check(false);
			break;
	}
	
	for (uint32 i = 0; i < Arguments.count; i++)
	{
		MTLArgument* Arg = [Arguments objectAtIndex:i];
		check(Arg);
		switch(Arg.type)
		{
			case MTLArgumentTypeBuffer:
			{
				checkf(Arg.index < ML_MaxBuffers, TEXT("Metal buffer index exceeded!"));
				ResourceMask[Frequency].BufferMask |= (1 << Arg.index);
				break;
			}
			case MTLArgumentTypeThreadgroupMemory:
			{
				break;
			}
			case MTLArgumentTypeTexture:
			{
				checkf(Arg.index < ML_MaxTextures, TEXT("Metal texture index exceeded!"));
				ResourceMask[Frequency].TextureMask |= (1 << Arg.index);
				break;
			}
			case MTLArgumentTypeSampler:
			{
				checkf(Arg.index < ML_MaxSamplers, TEXT("Metal sampler index exceeded!"));
				ResourceMask[Frequency].SamplerMask |= (1 << Arg.index);
				break;
			}
			default:
				check(false);
				break;
		}
	}
}
#endif
@end

struct FMetalGraphicsPipelineKey
{
	FMetalRenderPipelineHash RenderPipelineHash;
	FMetalHashedVertexDescriptor VertexDescriptorHash;
	FSHAHash VertexFunction;
	FSHAHash DomainFunction;
	FSHAHash PixelFunction;
	uint32 VertexBufferHash;
	uint32 DomainBufferHash;
	uint32 PixelBufferHash;

	template<typename Type>
	inline void SetHashValue(uint32 Offset, uint32 NumBits, Type Value)
	{
		if (Offset < Offset_RasterEnd)
		{
			uint64 BitMask = ((((uint64)1ULL) << NumBits) - 1) << Offset;
			RenderPipelineHash.RasterBits = (RenderPipelineHash.RasterBits & ~BitMask) | (((uint64)Value << Offset) & BitMask);
		}
		else
		{
			Offset -= Offset_RenderTargetFormat0;
			uint64 BitMask = ((((uint64)1ULL) << NumBits) - 1) << Offset;
			RenderPipelineHash.TargetBits = (RenderPipelineHash.TargetBits & ~BitMask) | (((uint64)Value << Offset) & BitMask);
		}
	}

	bool operator==(FMetalGraphicsPipelineKey const& Other) const
	{
		return (RenderPipelineHash == Other.RenderPipelineHash
		&& VertexDescriptorHash == Other.VertexDescriptorHash
		&& VertexFunction == Other.VertexFunction
		&& DomainFunction == Other.DomainFunction
		&& PixelFunction == Other.PixelFunction
		&& VertexBufferHash == Other.VertexBufferHash
		&& DomainBufferHash == Other.DomainBufferHash
		&& PixelBufferHash == Other.PixelBufferHash);
	}
	
	friend uint32 GetTypeHash(FMetalGraphicsPipelineKey const& Key)
	{
		uint32 H = FCrc::MemCrc32(&Key.RenderPipelineHash, sizeof(Key.RenderPipelineHash), GetTypeHash(Key.VertexDescriptorHash));
		H = FCrc::MemCrc32(Key.VertexFunction.Hash, sizeof(Key.VertexFunction.Hash), H);
		H = FCrc::MemCrc32(Key.DomainFunction.Hash, sizeof(Key.DomainFunction.Hash), H);
		H = FCrc::MemCrc32(Key.PixelFunction.Hash, sizeof(Key.PixelFunction.Hash), H);
		H = HashCombine(H, GetTypeHash(Key.VertexBufferHash));
		H = HashCombine(H, GetTypeHash(Key.DomainBufferHash));
		H = HashCombine(H, GetTypeHash(Key.PixelBufferHash));
		return H;
	}
	
	friend void InitMetalGraphicsPipelineKey(FMetalGraphicsPipelineKey& Key, const FGraphicsPipelineStateInitializer& Init, EMetalIndexType const IndexType, EPixelFormat const* const VertexBufferTypes, EPixelFormat const* const PixelBufferTypes, EPixelFormat const* const DomainBufferTypes)
	{
		uint32 const NumActiveTargets = Init.ComputeNumValidRenderTargets();
		check(NumActiveTargets <= MaxSimultaneousRenderTargets);
	
		FMetalBlendState* BlendState = (FMetalBlendState*)Init.BlendState;
		
		FMemory::Memzero(Key.RenderPipelineHash);
		
		bool bHasActiveTargets = false;
		for (uint32 i = 0; i < NumActiveTargets; i++)
		{
			EPixelFormat TargetFormat = Init.RenderTargetFormats[i];
			if (TargetFormat == PF_Unknown) { continue; }

			mtlpp::PixelFormat MetalFormat = (mtlpp::PixelFormat)GPixelFormats[TargetFormat].PlatformFormat;
			uint32 Flags = Init.RenderTargetFlags[i];
			if (Flags & TexCreate_SRGB)
			{
#if PLATFORM_MAC // Expand as R8_sRGB is iOS only.
				if (MetalFormat == mtlpp::PixelFormat::R8Unorm)
				{
					MetalFormat = mtlpp::PixelFormat::RGBA8Unorm;
				}
#endif
				MetalFormat = ToSRGBFormat(MetalFormat);
			}
			
			uint8 FormatKey = GetMetalPixelFormatKey(MetalFormat);;
			Key.SetHashValue(RTBitOffsets[i], NumBits_RenderTargetFormat, FormatKey);
			Key.SetHashValue(BlendBitOffsets[i], NumBits_BlendState, BlendState->RenderTargetStates[i].BlendStateKey);
			
			bHasActiveTargets |= true;
		}
		
		uint8 DepthFormatKey = 0;
		uint8 StencilFormatKey = 0;
		switch(Init.DepthStencilTargetFormat)
		{
			case PF_DepthStencil:
			{
				mtlpp::PixelFormat MetalFormat = (mtlpp::PixelFormat)GPixelFormats[PF_DepthStencil].PlatformFormat;
				if (Init.DepthTargetLoadAction != ERenderTargetLoadAction::ENoAction || Init.DepthTargetStoreAction != ERenderTargetStoreAction::ENoAction)
				{
					DepthFormatKey = GetMetalPixelFormatKey(MetalFormat);
				}
				if (Init.StencilTargetLoadAction != ERenderTargetLoadAction::ENoAction || Init.StencilTargetStoreAction != ERenderTargetStoreAction::ENoAction)
				{
					StencilFormatKey = GetMetalPixelFormatKey(mtlpp::PixelFormat::Stencil8);
				}
				bHasActiveTargets |= true;
				break;
			}
			case PF_ShadowDepth:
			{
				DepthFormatKey = GetMetalPixelFormatKey((mtlpp::PixelFormat)GPixelFormats[PF_ShadowDepth].PlatformFormat);
				bHasActiveTargets |= true;
				break;
			}
			default:
			{
				break;
			}
		}
		
		// If the pixel shader writes depth then we must compile with depth access, so we may bind the dummy depth.
		// If the pixel shader writes to UAVs but not target is bound we must also bind the dummy depth.
		FMetalPixelShader* PixelShader = (FMetalPixelShader*)Init.BoundShaderState.PixelShaderRHI;
		if (PixelShader && (((PixelShader->Bindings.InOutMask & 0x8000) && (DepthFormatKey == 0)) || (bHasActiveTargets == false && PixelShader->Bindings.NumUAVs > 0)))
		{
			mtlpp::PixelFormat MetalFormat = (mtlpp::PixelFormat)GPixelFormats[PF_DepthStencil].PlatformFormat;
			DepthFormatKey = GetMetalPixelFormatKey(MetalFormat);
		}
			
		Key.SetHashValue(Offset_DepthFormat, NumBits_DepthFormat, DepthFormatKey);
		Key.SetHashValue(Offset_StencilFormat, NumBits_StencilFormat, StencilFormatKey);

		Key.SetHashValue(Offset_SampleCount, NumBits_SampleCount, Init.NumSamples);
		
#if PLATFORM_MAC		
		Key.SetHashValue(Offset_PrimitiveTopology, NumBits_PrimitiveTopology, TranslatePrimitiveTopology(Init.PrimitiveType));
#endif

		FMetalVertexDeclaration* VertexDecl = (FMetalVertexDeclaration*)Init.BoundShaderState.VertexDeclarationRHI;
		Key.VertexDescriptorHash = VertexDecl->Layout;
		
		FMetalVertexShader* VertexShader = (FMetalVertexShader*)Init.BoundShaderState.VertexShaderRHI;
		FMetalDomainShader* DomainShader = (FMetalDomainShader*)Init.BoundShaderState.DomainShaderRHI;
		
        Key.VertexFunction = VertexShader->GetHash();
		Key.VertexBufferHash = VertexShader->GetBindingHash(VertexBufferTypes);
		if (DomainShader)
		{
			Key.DomainFunction = DomainShader->GetHash();
			Key.SetHashValue(Offset_IndexType, NumBits_IndexType, IndexType);
			Key.DomainBufferHash = DomainShader->GetBindingHash(DomainBufferTypes);
		}
		else
		{
			Key.SetHashValue(Offset_IndexType, NumBits_IndexType, EMetalIndexType_None);
			Key.DomainBufferHash = 0;
		}
		if (PixelShader)
		{
			Key.PixelFunction = PixelShader->GetHash();
			Key.PixelBufferHash = PixelShader->GetBindingHash(PixelBufferTypes);
		}
		else
		{
			Key.PixelBufferHash = 0;
		}
	}
};

static MTLVertexDescriptor* GetMaskedVertexDescriptor(MTLVertexDescriptor* InputDesc, uint32 InOutMask)
{
	for (uint32 Attr = 0; Attr < MaxMetalStreams; Attr++)
	{
		if (!(InOutMask & (1 << Attr)) && [InputDesc.attributes objectAtIndexedSubscript:Attr] != nil)
		{
			MTLVertexDescriptor* Desc = [[InputDesc copy] autorelease];
			uint32 BuffersUsed = 0;
			for (uint32 i = 0; i < MaxMetalStreams; i++)
			{
				if (!(InOutMask & (1 << i)))
				{
					[Desc.attributes setObject:nil atIndexedSubscript:i];
				}
				else
				{
					BuffersUsed |= (1 << [Desc.attributes objectAtIndexedSubscript:i].bufferIndex);
				}
			}
			for (uint32 i = 0; i < ML_MaxBuffers; i++)
			{
				if (!(BuffersUsed & (1 << i)))
				{
					[Desc.layouts setObject:nil atIndexedSubscript:i];
				}
			}
			return Desc;
		}
	}
	
	return InputDesc;
}

static FMetalShaderPipeline* CreateMTLRenderPipeline(bool const bSync, FMetalGraphicsPipelineKey const& Key, const FGraphicsPipelineStateInitializer& Init, EMetalIndexType const IndexType, EPixelFormat const* const VertexBufferTypes, EPixelFormat const* const PixelBufferTypes, EPixelFormat const* const DomainBufferTypes)
{
    FMetalVertexShader* VertexShader = (FMetalVertexShader*)Init.BoundShaderState.VertexShaderRHI;
    FMetalDomainShader* DomainShader = (FMetalDomainShader*)Init.BoundShaderState.DomainShaderRHI;
    FMetalPixelShader* PixelShader = (FMetalPixelShader*)Init.BoundShaderState.PixelShaderRHI;
    
    mtlpp::Function vertexFunction = VertexShader->GetFunction(IndexType, VertexBufferTypes, Key.VertexBufferHash);
    mtlpp::Function fragmentFunction = PixelShader ? PixelShader->GetFunction(EMetalIndexType_None, PixelBufferTypes, Key.PixelBufferHash) : nil;
    mtlpp::Function domainFunction = DomainShader ? DomainShader->GetFunction(EMetalIndexType_None, DomainBufferTypes, Key.DomainBufferHash) : nil;
    
    FMetalShaderPipeline* Pipeline = nil;
    if (vertexFunction && ((PixelShader != nullptr) == (fragmentFunction != nil)) && ((DomainShader != nullptr) == (domainFunction != nil)))
    {
		ns::Error Error;
		mtlpp::Device Device = GetMetalDeviceContext().GetDevice();

		uint32 const NumActiveTargets = Init.ComputeNumValidRenderTargets();
        check(NumActiveTargets <= MaxSimultaneousRenderTargets);
        if (PixelShader)
        {
			if ((PixelShader->Bindings.InOutMask & 0x8000) == 0 && (PixelShader->Bindings.InOutMask & 0x7fff) == 0 && PixelShader->Bindings.NumUAVs == 0 && PixelShader->Bindings.bDiscards == false)
			{
				UE_LOG(LogMetal, Error, TEXT("Pixel shader has no outputs which is not permitted. No Discards, In-Out Mask: %x\nNumber UAVs: %d\nSource Code:\n%s"), PixelShader->Bindings.InOutMask, PixelShader->Bindings.NumUAVs, *FString(PixelShader->GetSourceCode()));
				return nil;
			}
            
            UE_CLOG((NumActiveTargets < __builtin_popcount(PixelShader->Bindings.InOutMask & 0x7fff)), LogMetal, Verbose, TEXT("NumActiveTargets doesn't match pipeline's pixel shader output mask: %u, %hx"), NumActiveTargets, PixelShader->Bindings.InOutMask);
        }
        
		Pipeline = [FMetalShaderPipeline new];
		METAL_DEBUG_OPTION(FMemory::Memzero(Pipeline->ResourceMask, sizeof(Pipeline->ResourceMask)));

		mtlpp::RenderPipelineDescriptor RenderPipelineDesc;
		mtlpp::ComputePipelineDescriptor ComputePipelineDesc(nil);
		
        FMetalBlendState* BlendState = (FMetalBlendState*)Init.BlendState;
		
		ns::Array<mtlpp::RenderPipelineColorAttachmentDescriptor> ColorAttachments = RenderPipelineDesc.GetColorAttachments();
		
        for (uint32 i = 0; i < NumActiveTargets; i++)
        {
            EPixelFormat TargetFormat = Init.RenderTargetFormats[i];
            if (TargetFormat == PF_Unknown && PixelShader && (((PixelShader->Bindings.InOutMask & 0x7fff) & (1 << i))))
            {
				UE_LOG(LogMetal, Fatal, TEXT("Pipeline pixel shader expects target %u to be bound but it isn't: %s."), i, *FString(PixelShader->GetSourceCode()));
                continue;
            }
            
            mtlpp::PixelFormat MetalFormat = (mtlpp::PixelFormat)GPixelFormats[TargetFormat].PlatformFormat;
            uint32 Flags = Init.RenderTargetFlags[i];
            if (Flags & TexCreate_SRGB)
            {
        #if PLATFORM_MAC // Expand as R8_sRGB is iOS only.
                if (MetalFormat == mtlpp::PixelFormat::R8Unorm)
                {
                    MetalFormat = mtlpp::PixelFormat::RGBA8Unorm;
                }
        #endif
                MetalFormat = ToSRGBFormat(MetalFormat);
            }
            
            mtlpp::RenderPipelineColorAttachmentDescriptor Attachment = ColorAttachments[i];
            Attachment.SetPixelFormat(MetalFormat);
            
            mtlpp::RenderPipelineColorAttachmentDescriptor Blend = BlendState->RenderTargetStates[i].BlendState;
            if(TargetFormat != PF_Unknown)
            {
                // assign each property manually, would be nice if this was faster
                Attachment.SetBlendingEnabled(Blend.IsBlendingEnabled());
                Attachment.SetSourceRgbBlendFactor(Blend.GetSourceRgbBlendFactor());
                Attachment.SetDestinationRgbBlendFactor(Blend.GetDestinationRgbBlendFactor());
                Attachment.SetRgbBlendOperation(Blend.GetRgbBlendOperation());
                Attachment.SetSourceAlphaBlendFactor(Blend.GetSourceAlphaBlendFactor());
                Attachment.SetDestinationAlphaBlendFactor(Blend.GetDestinationAlphaBlendFactor());
                Attachment.SetAlphaBlendOperation(Blend.GetAlphaBlendOperation());
                Attachment.SetWriteMask(Blend.GetWriteMask());
            }
            else
            {
                Attachment.SetBlendingEnabled(NO);
				Attachment.SetWriteMask(mtlpp::ColorWriteMask::None);
            }
        }
        
        switch(Init.DepthStencilTargetFormat)
        {
            case PF_DepthStencil:
            {
                mtlpp::PixelFormat MetalFormat = (mtlpp::PixelFormat)GPixelFormats[PF_DepthStencil].PlatformFormat;
                if(MetalFormat == mtlpp::PixelFormat::Depth32Float)
                {
                    if (Init.DepthTargetLoadAction != ERenderTargetLoadAction::ENoAction || Init.DepthTargetStoreAction != ERenderTargetStoreAction::ENoAction)
                    {
                        RenderPipelineDesc.SetDepthAttachmentPixelFormat(MetalFormat);
                    }
                    if (Init.StencilTargetLoadAction != ERenderTargetLoadAction::ENoAction || Init.StencilTargetStoreAction != ERenderTargetStoreAction::ENoAction)
                    {
                        RenderPipelineDesc.SetStencilAttachmentPixelFormat(mtlpp::PixelFormat::Stencil8);
                    }
                }
                else
                {
                    RenderPipelineDesc.SetDepthAttachmentPixelFormat(MetalFormat);
                    RenderPipelineDesc.SetStencilAttachmentPixelFormat(MetalFormat);
                }
                break;
            }
            case PF_ShadowDepth:
            {
                RenderPipelineDesc.SetDepthAttachmentPixelFormat((mtlpp::PixelFormat)GPixelFormats[PF_ShadowDepth].PlatformFormat);
                break;
            }
            default:
            {
                break;
            }
        }
        
        check(Init.BoundShaderState.VertexShaderRHI != nullptr);
        check(Init.BoundShaderState.GeometryShaderRHI == nullptr);

        FMetalHullShader* HullShader = (FMetalHullShader*)Init.BoundShaderState.HullShaderRHI;
        
        if(RenderPipelineDesc.GetDepthAttachmentPixelFormat() == mtlpp::PixelFormat::Invalid && PixelShader && ((PixelShader->Bindings.InOutMask & 0x8000) || (NumActiveTargets == 0 && (PixelShader->Bindings.NumUAVs > 0))))
        {
            RenderPipelineDesc.SetDepthAttachmentPixelFormat((mtlpp::PixelFormat)GPixelFormats[PF_DepthStencil].PlatformFormat);
            RenderPipelineDesc.SetStencilAttachmentPixelFormat((mtlpp::PixelFormat)GPixelFormats[PF_DepthStencil].PlatformFormat);
        }
        
        RenderPipelineDesc.SetSampleCount(FMath::Max(Init.NumSamples, (uint16)1u));
    #if PLATFORM_MAC
        RenderPipelineDesc.SetInputPrimitiveTopology(TranslatePrimitiveTopology(Init.PrimitiveType));
    #endif
        
        FMetalVertexDeclaration* VertexDecl = (FMetalVertexDeclaration*)Init.BoundShaderState.VertexDeclarationRHI;
        
        if (Init.BoundShaderState.HullShaderRHI == nullptr)
        {
            check(Init.BoundShaderState.DomainShaderRHI == nullptr);
            RenderPipelineDesc.SetVertexDescriptor(GetMaskedVertexDescriptor(VertexDecl->Layout.VertexDesc, VertexShader->Bindings.InOutMask));
            RenderPipelineDesc.SetVertexFunction(vertexFunction);
            RenderPipelineDesc.SetFragmentFunction(fragmentFunction);
#if ENABLE_METAL_GPUPROFILE
			ns::String VertexName = vertexFunction.GetName();
			ns::String FragmentName = fragmentFunction ? fragmentFunction.GetName() : @"";
			RenderPipelineDesc.SetLabel([NSString stringWithFormat:@"%@+%@", VertexName.GetPtr(), FragmentName.GetPtr()]);
#endif
        }
        else
        {
            check(Init.BoundShaderState.DomainShaderRHI != nullptr);
            
            RenderPipelineDesc.SetTessellationPartitionMode(GMetalTessellationForcePartitionMode == 0 ? DomainShader->TessellationPartitioning : (mtlpp::TessellationPartitionMode)(GMetalTessellationForcePartitionMode - 1));
			RenderPipelineDesc.SetTessellationFactorStepFunction(mtlpp::TessellationFactorStepFunction::PerPatch);
            RenderPipelineDesc.SetTessellationOutputWindingOrder(DomainShader->TessellationOutputWinding);
            int FixedMaxTessFactor = (int)RoundTessLevel(VertexShader->TessellationMaxTessFactor, RenderPipelineDesc.GetTessellationPartitionMode());
            RenderPipelineDesc.SetMaxTessellationFactor(FixedMaxTessFactor);
			RenderPipelineDesc.SetTessellationFactorScaleEnabled(NO);
			RenderPipelineDesc.SetTessellationFactorFormat(mtlpp::TessellationFactorFormat::Half);
			RenderPipelineDesc.SetTessellationControlPointIndexType(mtlpp::TessellationControlPointIndexType::None);
            
            RenderPipelineDesc.SetVertexFunction(domainFunction);
            RenderPipelineDesc.SetFragmentFunction(fragmentFunction);
#if ENABLE_METAL_GPUPROFILE
			{
				ns::String VertexName = domainFunction.GetName();
				ns::String FragmentName = fragmentFunction ? fragmentFunction.GetName() : @"";
				RenderPipelineDesc.SetLabel([NSString stringWithFormat:@"%@+%@", VertexName.GetPtr(), FragmentName.GetPtr()]);
			}
#endif
            
			ComputePipelineDesc = mtlpp::ComputePipelineDescriptor();
            check(ComputePipelineDesc);
			
			mtlpp::VertexDescriptor DomainVertexDesc;
			mtlpp::StageInputOutputDescriptor ComputeStageInOut;
			ComputeStageInOut.SetIndexBufferIndex(VertexShader->TessellationControlPointIndexBuffer);
			
            FMetalTessellationPipelineDesc& TessellationDesc = Pipeline->TessellationPipelineDesc;
            TessellationDesc.TessellationInputControlPointBufferIndex = DomainShader->TessellationControlPointOutBuffer;
            TessellationDesc.TessellationOutputControlPointBufferIndex = VertexShader->TessellationControlPointOutBuffer;
            TessellationDesc.TessellationInputPatchConstBufferIndex = DomainShader->TessellationHSOutBuffer;
            TessellationDesc.TessellationPatchConstBufferIndex = VertexShader->TessellationHSOutBuffer;
            TessellationDesc.TessellationFactorBufferIndex = VertexShader->TessellationHSTFOutBuffer;
            TessellationDesc.TessellationPatchCountBufferIndex = VertexShader->TessellationPatchCountBuffer;
            TessellationDesc.TessellationIndexBufferIndex = VertexShader->TessellationIndexBuffer;
            TessellationDesc.TessellationPatchConstOutSize = VertexShader->TessellationOutputAttribs.HSOutSize;
			TessellationDesc.TessellationControlPointIndexBufferIndex = VertexShader->TessellationControlPointIndexBuffer;
			TessellationDesc.DomainVertexDescriptor = DomainVertexDesc;
            TessellationDesc.DSNumUniformBuffers = DomainShader->Bindings.NumUniformBuffers;
            TessellationDesc.TessellationPatchControlPointOutSize = VertexShader->TessellationOutputAttribs.PatchControlPointOutSize;
            TessellationDesc.TessellationTessFactorOutSize = VertexShader->TessellationOutputAttribs.HSTFOutSize;
            
            check(TessellationDesc.TessellationOutputControlPointBufferIndex < ML_MaxBuffers);
            check(TessellationDesc.TessellationFactorBufferIndex < ML_MaxBuffers);
            check(TessellationDesc.TessellationPatchCountBufferIndex < ML_MaxBuffers);
            check(TessellationDesc.TessellationTessFactorOutSize == 2*4 || TessellationDesc.TessellationTessFactorOutSize == 2*6);
            
            mtlpp::VertexStepFunction stepFunction = mtlpp::VertexStepFunction::PerPatch;
            
            static mtlpp::VertexFormat Formats[(uint8)EMetalComponentType::Max][4] = {
                {mtlpp::VertexFormat::UInt, mtlpp::VertexFormat::UInt2, mtlpp::VertexFormat::UInt3, mtlpp::VertexFormat::UInt4},
                {mtlpp::VertexFormat::Int, mtlpp::VertexFormat::Int2, mtlpp::VertexFormat::Int3, mtlpp::VertexFormat::Int4},
                {mtlpp::VertexFormat::Invalid, mtlpp::VertexFormat::Half2, mtlpp::VertexFormat::Half3, mtlpp::VertexFormat::Half4},
                {mtlpp::VertexFormat::Float, mtlpp::VertexFormat::Float2, mtlpp::VertexFormat::Float3, mtlpp::VertexFormat::Float4},
                {mtlpp::VertexFormat::Invalid, mtlpp::VertexFormat::UChar2, mtlpp::VertexFormat::UChar3, mtlpp::VertexFormat::UChar4},
            };
			
			ns::Array<mtlpp::VertexBufferLayoutDescriptor> DomainVertexLayouts = DomainVertexDesc.GetLayouts();
			
            if (DomainShader->TessellationHSOutBuffer != UINT_MAX)
            {
                check(DomainShader->TessellationHSOutBuffer < ML_MaxBuffers);
                uint32 bufferIndex = DomainShader->TessellationHSOutBuffer;
                uint32 bufferSize = VertexShader->TessellationOutputAttribs.HSOutSize;
               
                DomainVertexLayouts[bufferIndex].SetStride(bufferSize);
                DomainVertexLayouts[bufferIndex].SetStepFunction(stepFunction);
                DomainVertexLayouts[bufferIndex].SetStepRate(1);
				
				ns::Array<mtlpp::VertexAttributeDescriptor> Attribs = DomainVertexDesc.GetAttributes();
				
                for (FMetalAttribute const& Attrib : VertexShader->TessellationOutputAttribs.HSOut)
                {
                    int attributeIndex = Attrib.Index;
                    check(attributeIndex >= 0 && attributeIndex <= 31);
                    check(Attrib.Components > 0 && Attrib.Components <= 4);
                    mtlpp::VertexFormat format = Formats[(uint8)Attrib.Type][Attrib.Components-1];
                    check(format != mtlpp::VertexFormat::Invalid); // TODO support more cases
                    Attribs[attributeIndex].SetFormat(format);
                    Attribs[attributeIndex].SetOffset(Attrib.Offset);
                    Attribs[attributeIndex].SetBufferIndex(bufferIndex);
                }
            }
			
            stepFunction = mtlpp::VertexStepFunction::PerPatchControlPoint;
            uint32 bufferIndex = DomainShader->TessellationControlPointOutBuffer;
            uint32 bufferSize = VertexShader->TessellationOutputAttribs.PatchControlPointOutSize;
            
            DomainVertexLayouts[bufferIndex].SetStride(bufferSize);
            DomainVertexLayouts[bufferIndex].SetStepFunction(stepFunction);
            DomainVertexLayouts[bufferIndex].SetStepRate(1);
			
			ns::Array<mtlpp::VertexAttributeDescriptor> DomainVertexAttribs = DomainVertexDesc.GetAttributes();
            for (FMetalAttribute const& Attrib : VertexShader->TessellationOutputAttribs.PatchControlPointOut)
            {
                int attributeIndex = Attrib.Index;
                check(attributeIndex >= 0 && attributeIndex <= 31);
                check(Attrib.Components > 0 && Attrib.Components <= 4);
                mtlpp::VertexFormat format = Formats[(uint8)Attrib.Type][Attrib.Components-1];
                check(format != mtlpp::VertexFormat::Invalid); // TODO support more cases
				DomainVertexAttribs[attributeIndex].SetFormat(format);
				DomainVertexAttribs[attributeIndex].SetOffset(Attrib.Offset);
				DomainVertexAttribs[attributeIndex].SetBufferIndex(bufferIndex);
            }
			
			RenderPipelineDesc.SetVertexDescriptor(DomainVertexDesc);
            
            bool const bIsIndexed = (IndexType == EMetalIndexType_UInt16 || IndexType == EMetalIndexType_UInt32);
            
			mtlpp::VertexDescriptor VertexDesc = GetMaskedVertexDescriptor(VertexDecl->Layout.VertexDesc, VertexShader->Bindings.InOutMask);
			ns::Array<mtlpp::VertexBufferLayoutDescriptor> VertexLayouts = VertexDesc.GetLayouts();
			ns::Array<mtlpp::VertexAttributeDescriptor> VertexAttribs = VertexDesc.GetAttributes();
			ns::Array<mtlpp::BufferLayoutDescriptor> ComputeLayouts = ComputeStageInOut.GetLayouts();
			ns::Array<mtlpp::AttributeDescriptor> ComputeAttribs = ComputeStageInOut.GetAttributes();
			for(int onIndex = 0; onIndex < MaxMetalStreams; onIndex++)
            {
                // NOTE: accessing the VertexDesc like this will end up allocating layouts/attributes
                auto stride = VertexLayouts[onIndex].GetStride();
                if(stride)
                {
                    ComputeLayouts[onIndex].SetStride(stride);
                    auto InnerStepFunction = VertexLayouts[onIndex].GetStepFunction();
                    switch(InnerStepFunction)
                    {
                        case mtlpp::VertexStepFunction::Constant:
							ComputeLayouts[onIndex].SetStepFunction(mtlpp::StepFunction::Constant);
                            break;
                        case mtlpp::VertexStepFunction::PerVertex:
                            ComputeLayouts[onIndex].SetStepFunction(bIsIndexed ? mtlpp::StepFunction::ThreadPositionInGridXIndexed : mtlpp::StepFunction::ThreadPositionInGridX);
                            break;
                        case mtlpp::VertexStepFunction::PerInstance:
                            ComputeLayouts[onIndex].SetStepFunction(mtlpp::StepFunction::ThreadPositionInGridY);
                            break;
                        default:
                            check(0);
                    }
                    ComputeLayouts[onIndex].SetStepRate(VertexLayouts[onIndex].GetStepRate());
                }
                auto format = VertexAttribs[onIndex].GetFormat();
                if(format == mtlpp::VertexFormat::Invalid) continue;
                {
					ComputeAttribs[onIndex].SetFormat((mtlpp::AttributeFormat)format); // TODO FIXME currently these align perfectly (at least assert that is the case)
                    ComputeAttribs[onIndex].SetOffset(VertexAttribs[onIndex].GetOffset());
                    ComputeAttribs[onIndex].SetBufferIndex(VertexAttribs[onIndex].GetBufferIndex());
                }
            }
            
            // Disambiguated function name.
            ComputePipelineDesc.SetComputeFunction(vertexFunction);
            check(ComputePipelineDesc.GetComputeFunction());

            // Don't set the index type if there isn't an index buffer.
            if (IndexType != EMetalIndexType_None)
            {
                ComputeStageInOut.SetIndexType(GetMetalIndexType(IndexType));
            }
			ComputePipelineDesc.SetStageInputDescriptor(ComputeStageInOut);
			
            {
				METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewComputePipelineState: %s"), TEXT("")/**FString([ComputePipelineDesc.GetPtr() description])*/)));
				ns::AutoReleasedError AutoError;
				NSUInteger ComputeOption = mtlpp::PipelineOption::NoPipelineOption;
#if ENABLE_METAL_GPUPROFILE
				{
					ns::String VertexName = vertexFunction.GetName();
					RenderPipelineDesc.SetLabel([NSString stringWithFormat:@"%@", VertexName.GetPtr()]);
				}
#endif
#if METAL_DEBUG_OPTIONS
				if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation METAL_STATISTIC(|| GetMetalDeviceContext().GetCommandQueue().GetStatistics()))
				{
					mtlpp::AutoReleasedComputePipelineReflection Reflection;
					ComputeOption = mtlpp::PipelineOption::ArgumentInfo|mtlpp::PipelineOption::BufferTypeInfo METAL_STATISTIC(|NSUInteger(EMTLPipelineStats));
					Pipeline->ComputePipelineState = Device.NewComputePipelineState(ComputePipelineDesc, (mtlpp::PipelineOption)ComputeOption, &Reflection, &AutoError);
					Pipeline->ComputePipelineReflection = Reflection;
				}
				else
#endif
				{
					Pipeline->ComputePipelineState = Device.NewComputePipelineState(ComputePipelineDesc, (mtlpp::PipelineOption)ComputeOption, nullptr, &AutoError);
				}
				Error = AutoError;
                
				UE_CLOG((Pipeline->ComputePipelineState == nil), LogMetal, Error, TEXT("Failed to generate a pipeline state object: %s"), *FString([Error.GetPtr() description]));
				UE_CLOG((Pipeline->ComputePipelineState == nil), LogMetal, Error, TEXT("Vertex shader: %s"), *FString(VertexShader->GetSourceCode()));
				UE_CLOG((Pipeline->ComputePipelineState == nil), LogMetal, Error, TEXT("Pixel shader: %s"), PixelShader ? *FString(PixelShader->GetSourceCode()) : TEXT("NULL"));
				UE_CLOG((Pipeline->ComputePipelineState == nil), LogMetal, Error, TEXT("Hull shader: %s"), *FString(HullShader->GetSourceCode()));
				UE_CLOG((Pipeline->ComputePipelineState == nil), LogMetal, Error, TEXT("Domain shader: %s"), *FString(DomainShader->GetSourceCode()));
				UE_CLOG((Pipeline->ComputePipelineState == nil), LogMetal, Error, TEXT("Descriptor: %s"), *FString(ComputePipelineDesc.GetPtr().description));
				UE_CLOG((Pipeline->ComputePipelineState == nil), LogMetal, Fatal, TEXT("Failed to generate a hull pipeline state object:\n\n %s\n\n"), *FString(Error.GetLocalizedDescription()));
				
#if METAL_DEBUG_OPTIONS
				if (Pipeline->ComputePipelineReflection)
				{
					Pipeline->ComputeDesc = ComputePipelineDesc;
					
					bool found__HSTFOut = false;
					for(mtlpp::Argument arg : Pipeline->ComputePipelineReflection.GetArguments())
					{
						bool addAttributes = false;
						mtlpp::VertexStepFunction StepFunction = (mtlpp::VertexStepFunction)-1; // invalid
						
						uint32 BufferIndex = UINT_MAX;
						
						if([arg.GetName() isEqualToString: @"PatchControlPointOutBuffer"])
						{
							check((arg.GetBufferAlignment() & (arg.GetBufferAlignment() - 1)) == 0); // must be pow2
							check((arg.GetBufferDataSize() & (arg.GetBufferAlignment() - 1)) == 0); // must be aligned
							
							check(arg.GetBufferDataSize() == VertexShader->TessellationOutputAttribs.PatchControlPointOutSize);
							
							addAttributes = true;
							BufferIndex = DomainShader->TessellationControlPointOutBuffer;
							StepFunction = mtlpp::VertexStepFunction::PerPatchControlPoint;
							check(arg.GetIndex() == VertexShader->TessellationControlPointOutBuffer);
						}
						else if([arg.GetName() isEqualToString: @"__HSOut"])
						{
							check((arg.GetBufferAlignment() & (arg.GetBufferAlignment() - 1)) == 0); // must be pow2
							check((arg.GetBufferDataSize() & (arg.GetBufferAlignment() - 1)) == 0); // must be aligned
							
							check(arg.GetBufferDataSize() == VertexShader->TessellationOutputAttribs.HSOutSize);
							
							addAttributes = true;
							BufferIndex = DomainShader->TessellationHSOutBuffer;
							StepFunction = mtlpp::VertexStepFunction::PerPatch;
							check(arg.GetIndex() == VertexShader->TessellationHSOutBuffer);
						}
						else if([arg.GetName() isEqualToString: @"__HSTFOut"])
						{
							found__HSTFOut = true;
							check((arg.GetBufferAlignment() & (arg.GetBufferAlignment() - 1)) == 0); // must be pow2
							check((arg.GetBufferDataSize() & (arg.GetBufferAlignment() - 1)) == 0); // must be aligned
							
							check(arg.GetBufferDataSize() == VertexShader->TessellationOutputAttribs.HSTFOutSize);
							
							check(arg.GetIndex() == VertexShader->TessellationHSTFOutBuffer);
						}
						else if([arg.GetName() isEqualToString:@"patchCount"])
						{
							check(arg.GetIndex() == VertexShader->TessellationPatchCountBuffer);
						}
						else if([arg.GetName() isEqualToString:@"indexBuffer"])
						{
							check(arg.GetIndex() == VertexShader->TessellationIndexBuffer);
						}
						
						// build the vertexDescriptor
						if(addAttributes)
						{
							check(DomainVertexLayouts[BufferIndex].GetStride() == arg.GetBufferDataSize());
							check(DomainVertexLayouts[BufferIndex].GetStepFunction() == StepFunction);
							check(DomainVertexLayouts[BufferIndex].GetStepRate() == 1);
							for(mtlpp::StructMember attribute : arg.GetBufferStructType().GetMembers())
							{
								int attributeIndex = -1;
								sscanf([attribute.GetName() UTF8String], "OUT_ATTRIBUTE%d_", &attributeIndex);
								check(attributeIndex >= 0 && attributeIndex <= 31);
								mtlpp::VertexFormat format = mtlpp::VertexFormat::Invalid;
								switch(attribute.GetDataType())
								{
									case mtlpp::DataType::Float:  format = mtlpp::VertexFormat::Float; break;
									case mtlpp::DataType::Float2: format = mtlpp::VertexFormat::Float2; break;
									case mtlpp::DataType::Float3: format = mtlpp::VertexFormat::Float3; break;
									case mtlpp::DataType::Float4: format = mtlpp::VertexFormat::Float4; break;
										
									case mtlpp::DataType::Int:  format = mtlpp::VertexFormat::Int; break;
									case mtlpp::DataType::Int2: format = mtlpp::VertexFormat::Int2; break;
									case mtlpp::DataType::Int3: format = mtlpp::VertexFormat::Int3; break;
									case mtlpp::DataType::Int4: format = mtlpp::VertexFormat::Int4; break;
										
									case mtlpp::DataType::UInt:  format = mtlpp::VertexFormat::UInt; break;
									case mtlpp::DataType::UInt2: format = mtlpp::VertexFormat::UInt2; break;
									case mtlpp::DataType::UInt3: format = mtlpp::VertexFormat::UInt3; break;
									case mtlpp::DataType::UInt4: format = mtlpp::VertexFormat::UInt4; break;
										
									default: check(0); // TODO support more cases
								}
								check(DomainVertexAttribs[attributeIndex].GetFormat() == format);
								check(DomainVertexAttribs[attributeIndex].GetOffset() == attribute.GetOffset());
								check(DomainVertexAttribs[attributeIndex].GetBufferIndex() == BufferIndex);
							}
						}
					}
					check(found__HSTFOut);
				}
#endif
            }
        }
        
        NSUInteger RenderOption = mtlpp::PipelineOption::NoPipelineOption;
		mtlpp::AutoReleasedRenderPipelineReflection* Reflection = nullptr;
#if METAL_DEBUG_OPTIONS
		mtlpp::AutoReleasedRenderPipelineReflection OutReflection;
		Reflection = &OutReflection;
        if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation METAL_STATISTIC(|| GetMetalDeviceContext().GetCommandQueue().GetStatistics()))
        {
        	RenderOption = mtlpp::PipelineOption::ArgumentInfo|mtlpp::PipelineOption::BufferTypeInfo METAL_STATISTIC(|NSUInteger(EMTLPipelineStats));
        }
#endif

		{
			ns::AutoReleasedError RenderError;
			METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewRenderPipeline: %s"), TEXT("")/**FString([RenderPipelineDesc.GetPtr() description])*/)));
			Pipeline->RenderPipelineState = Device.NewRenderPipelineState(RenderPipelineDesc, (mtlpp::PipelineOption)RenderOption, Reflection, &RenderError);
#if METAL_DEBUG_OPTIONS
			if (Reflection)
			{
				Pipeline->RenderPipelineReflection = *Reflection;
				Pipeline->RenderDesc = RenderPipelineDesc;
			}
#endif
			Error = RenderError;
		}
		
		UE_CLOG((Pipeline->RenderPipelineState == nil), LogMetal, Error, TEXT("Failed to generate a pipeline state object: %s"), *FString(Error.GetPtr().description));
		UE_CLOG((Pipeline->RenderPipelineState == nil), LogMetal, Error, TEXT("Vertex shader: %s"), *FString(VertexShader->GetSourceCode()));
		UE_CLOG((Pipeline->RenderPipelineState == nil), LogMetal, Error, TEXT("Pixel shader: %s"), PixelShader ? *FString(PixelShader->GetSourceCode()) : TEXT("NULL"));
		UE_CLOG((Pipeline->RenderPipelineState == nil), LogMetal, Error, TEXT("Hull shader: %s"), HullShader ? *FString(HullShader->GetSourceCode()) : TEXT("NULL"));
		UE_CLOG((Pipeline->RenderPipelineState == nil), LogMetal, Error, TEXT("Domain shader: %s"), DomainShader ? *FString(DomainShader->GetSourceCode()) : TEXT("NULL"));
		UE_CLOG((Pipeline->RenderPipelineState == nil), LogMetal, Error, TEXT("Descriptor: %s"), *FString(RenderPipelineDesc.GetPtr().description));
		UE_CLOG((Pipeline->RenderPipelineState == nil), LogMetal, Error, TEXT("Failed to generate a render pipeline state object:\n\n %s\n\n"), *FString(Error.GetLocalizedDescription()));
		
		// We need to pass a failure up the chain, so we'll clean up here.
		if(Pipeline->RenderPipelineState == nil)
		{
			[Pipeline release];
			return nil;
		}
		
    #if METAL_DEBUG_OPTIONS
        Pipeline->ComputeSource = DomainShader ? VertexShader->GetSourceCode() : nil;
        Pipeline->VertexSource = DomainShader ? DomainShader->GetSourceCode() : VertexShader->GetSourceCode();
        Pipeline->FragmentSource = PixelShader ? PixelShader->GetSourceCode() : nil;
    #endif
        
    #if 0
        if (GFrameCounter > 3)
        {
            NSLog(@"===============================================================");
            NSLog(@"Creating a BSS at runtime frame %lld... this may hitch! [this = %p]", GFrameCounter, Pipeline);
            NSLog(@"Vertex declaration:");
            FVertexDeclarationElementList& Elements = VertexDecl->Elements;
            for (int32 i = 0; i < Elements.Num(); i++)
            {
                FVertexElement& Elem = Elements[i];
                NSLog(@"   Elem %d: attr: %d, stream: %d, type: %d, stride: %d, offset: %d", i, Elem.AttributeIndex, Elem.StreamIndex, (uint32)Elem.Type, Elem.Stride, Elem.Offset);
            }
            
            NSLog(@"\nVertexShader:");
            NSLog(@"%@", VertexShader ? VertexShader->GetSourceCode() : @"NONE");
            NSLog(@"\nPixelShader:");
            NSLog(@"%@", PixelShader ? PixelShader->GetSourceCode() : @"NONE");
            NSLog(@"\nDomainShader:");
            NSLog(@"%@", DomainShader ? DomainShader->GetSourceCode() : @"NONE");
            NSLog(@"===============================================================");
        }
    #endif
        
#if METAL_DEBUG_OPTIONS
        if (GFrameCounter > 3)
        {
            UE_LOG(LogMetal, Verbose, TEXT("Created a hitchy pipeline state for hash %llx %llx %llx"), (uint64)Key.RenderPipelineHash.RasterBits, (uint64)(Key.RenderPipelineHash.TargetBits), (uint64)Key.VertexDescriptorHash.VertexDescHash);
        }
#endif

    }
    return !bSync ? nil : Pipeline;
}

static FMetalShaderPipeline* GetMTLRenderPipeline(bool const bSync, FMetalGraphicsPipelineState const* State, const FGraphicsPipelineStateInitializer& Init, EMetalIndexType const IndexType, EPixelFormat const* const VertexBufferTypes, EPixelFormat const* const PixelBufferTypes, EPixelFormat const* const DomainBufferTypes)
{
	static FRWLock PipelineMutex;
	static TMap<FMetalGraphicsPipelineKey, FMetalShaderPipeline*> Pipelines;
	
	SCOPE_CYCLE_COUNTER(STAT_MetalPipelineStateTime);
	
	FMetalGraphicsPipelineKey Key;
	InitMetalGraphicsPipelineKey(Key, Init, IndexType, VertexBufferTypes, PixelBufferTypes, DomainBufferTypes);

	// By default there'll be more threads trying to read this than to write it.
	FRWScopeLock Lock(PipelineMutex, SLT_ReadOnly);

	// Try to find the entry in the cache.
	FMetalShaderPipeline* Desc = Pipelines.FindRef(Key);
	if (Desc == nil)
	{
		Desc = CreateMTLRenderPipeline(bSync, Key, Init, IndexType, VertexBufferTypes, PixelBufferTypes, DomainBufferTypes);
		
		// Bail cleanly if compilation fails.
		if(!Desc)
		{
			return nil;
		}

		// Now we are a writer as we want to create & add the new pipeline
		Lock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
		
		// Retest to ensure no-one beat us here!
		if (Pipelines.FindRef(Key) == nil)
		{
			Pipelines.Add(Key, Desc);
		}
	}
	check(Desc);
	
	return Desc;
}

bool FMetalGraphicsPipelineState::Compile()
{
	FMemory::Memzero(PipelineStates);
	for (uint32 i = 0; i < EMetalIndexType_Num; i++)
	{
		PipelineStates[i][0][0][0] = [GetMTLRenderPipeline(true, this, Initializer, (EMetalIndexType)i, nullptr, nullptr, nullptr) retain];
		if(!PipelineStates[i][0][0][0])
		{
			return false;
		}
	}
	
	return true;
}

FMetalGraphicsPipelineState::~FMetalGraphicsPipelineState()
{
    static uint32 MaxBufferNum = (GMaxRHIFeatureLevel == ERHIFeatureLevel::SM5) ? EMetalBufferType_Num : 1u;
	for (uint32 i = 0; i < EMetalIndexType_Num; i++)
	{
        for (uint32 v = 0; v < MaxBufferNum; v++)
        {
            for (uint32 f = 0; f < MaxBufferNum; f++)
            {
                for (uint32 c = 0; c < MaxBufferNum; c++)
                {
                	if (PipelineStates[i][v][f][c])
                	{
	                    [PipelineStates[i][v][f][c] release];
	                    PipelineStates[i][v][f][c] = nil;
                    }
                }
            }
        }
	}
}

FMetalShaderPipeline* FMetalGraphicsPipelineState::GetPipeline(EMetalIndexType IndexType, uint32 VertexBufferHash, uint32 PixelBufferHash, uint32 DomainBufferHash, EPixelFormat const* const VertexBufferTypes, EPixelFormat const* const PixelBufferTypes, EPixelFormat const* const DomainBufferTypes)
{
	check(IndexType < EMetalIndexType_Num);

	EMetalBufferType Vertex = VertexShader && (VertexShader->BufferTypeHash && VertexShader->BufferTypeHash == VertexBufferHash) ? EMetalBufferType_Static : EMetalBufferType_Dynamic;
	EMetalBufferType Fragment = PixelShader && (PixelShader->BufferTypeHash && PixelShader->BufferTypeHash == PixelBufferHash) ? EMetalBufferType_Static : EMetalBufferType_Dynamic;
	EMetalBufferType Compute = DomainShader && (DomainShader->BufferTypeHash && DomainShader->BufferTypeHash == DomainBufferHash) ? EMetalBufferType_Static : EMetalBufferType_Dynamic;

    FMetalShaderPipeline* Pipe = (GMaxRHIFeatureLevel == ERHIFeatureLevel::SM5) ? PipelineStates[IndexType][Vertex][Fragment][Compute] : nullptr;
	if ((GMaxRHIFeatureLevel == ERHIFeatureLevel::SM5) && !Pipe)
	{
		Pipe = PipelineStates[IndexType][Vertex][Fragment][Compute] = [GetMTLRenderPipeline(true, this, Initializer, IndexType, VertexBufferTypes, PixelBufferTypes, DomainBufferTypes) retain];
	}
    if (!Pipe)
    {
    	if(!PipelineStates[IndexType][0][0][0])
    	{
    		PipelineStates[IndexType][0][0][0] = [GetMTLRenderPipeline(true, this, Initializer, IndexType, nullptr, nullptr, nullptr) retain];
    	}
        Pipe = PipelineStates[IndexType][0][0][0];
    }
	check(Pipe);
    return Pipe;
}


FGraphicsPipelineStateRHIRef FMetalDynamicRHI::RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
	@autoreleasepool {
	FMetalGraphicsPipelineState* State = new FMetalGraphicsPipelineState(Initializer);
		
	if(!State->Compile())
	{
		// Compilation failures are propagated up to the caller.
		State->DoNoDeferDelete();
		delete State;
		return nullptr;
	}
	State->VertexDeclaration = ResourceCast(Initializer.BoundShaderState.VertexDeclarationRHI);
	State->VertexShader = ResourceCast(Initializer.BoundShaderState.VertexShaderRHI);
	State->PixelShader = ResourceCast(Initializer.BoundShaderState.PixelShaderRHI);
	State->HullShader = ResourceCast(Initializer.BoundShaderState.HullShaderRHI);
	State->DomainShader = ResourceCast(Initializer.BoundShaderState.DomainShaderRHI);
	State->GeometryShader = ResourceCast(Initializer.BoundShaderState.GeometryShaderRHI);
	State->DepthStencilState = ResourceCast(Initializer.DepthStencilState);
	State->RasterizerState = ResourceCast(Initializer.RasterizerState);
	return State;
	}
}

TRefCountPtr<FRHIComputePipelineState> FMetalDynamicRHI::RHICreateComputePipelineState(FRHIComputeShader* ComputeShader)
{
	@autoreleasepool {
	return new FMetalComputePipelineState(ResourceCast(ComputeShader));
	}
}
