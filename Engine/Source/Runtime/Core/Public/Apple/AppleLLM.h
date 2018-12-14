// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/LowLevelMemTracker.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

#define LLM_SCOPE_APPLE(Tag) LLM_SCOPE((ELLMTag)Tag)
#define LLM_PLATFORM_SCOPE_APPLE(Tag) LLM_PLATFORM_SCOPE((ELLMTag)Tag)

enum class ELLMTagApple : LLM_TAG_TYPE
{
	ObjectiveC = (LLM_TAG_TYPE)ELLMTag::PlatformTagStart, // Use Instruments for detailed breakdown!

	Count
};

static_assert((int32)ELLMTagApple::Count <= (int32)ELLMTag::PlatformTagEnd, "too many ELLMTagApple tags");

namespace AppleLLM
{
	void Initialise();
}

#else

#define LLM_SCOPE_APPLE(...)
#define LLM_PLATFORM_SCOPE_APPLE(...)

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

