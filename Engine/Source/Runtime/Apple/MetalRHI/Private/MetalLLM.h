// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"
#include "HAL/LowLevelMemTracker.h"

@interface FMetalDeallocHandler : FApplePlatformObject<NSObject>
{
	dispatch_block_t Block;
}
-(instancetype)initWithBlock:(dispatch_block_t)InBlock;
-(void)dealloc;
@end

#if ENABLE_LOW_LEVEL_MEM_TRACKER

#define LLM_SCOPE_METAL(Tag) LLM_SCOPE((ELLMTag)Tag)
#define LLM_PLATFORM_SCOPE_METAL(Tag) LLM_PLATFORM_SCOPE((ELLMTag)Tag)

enum class ELLMTagMetal : LLM_TAG_TYPE
{
	Buffers = (LLM_TAG_TYPE)ELLMTag::PlatformTagStart,
	Textures,
	
	Count
};

static_assert((int32)ELLMTagMetal::Count <= (int32)ELLMTag::PlatformTagEnd, "too many ELLMTagMetal tags");

namespace MetalLLM
{
	void Initialise();
}

#else

#define LLM_SCOPE_METAL(...)
#define LLM_PLATFORM_SCOPE_METAL(...)

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

// These work without the LLM module
namespace MetalLLM
{
	void LogAllocTexture(mtlpp::Device& Device, mtlpp::TextureDescriptor const& Desc, mtlpp::Texture const& Texture);
	void LogAllocBuffer(mtlpp::Device& Device, mtlpp::Buffer const& Buffer);
}


