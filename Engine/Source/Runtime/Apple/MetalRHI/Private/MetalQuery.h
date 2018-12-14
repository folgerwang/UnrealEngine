// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>
#include "Templates/SharedPointer.h"
#include "MetalBuffer.h"

struct FMetalQueryBufferPool
{
    enum
    {
        EQueryBufferAlignment = 8,
        EQueryResultMaxSize = 8,
        EQueryBufferMaxSize = 64 * 1024
    };
    
    FMetalQueryBufferPool(class FMetalContext* InContext)
    : Context(InContext)
    {
    }
    
    void Allocate(FMetalQueryResult& NewQuery);
    FMetalQueryBuffer* GetCurrentQueryBuffer();
	void ReleaseCurrentQueryBuffer();
    void ReleaseQueryBuffer(FMetalBuffer& Buffer);
    
    FMetalQueryBufferRef CurrentBuffer;
    TArray<FMetalBuffer> Buffers;
	class FMetalContext* Context;
};
