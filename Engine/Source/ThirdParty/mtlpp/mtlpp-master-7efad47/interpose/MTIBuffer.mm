// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#import <Metal/MTLBuffer.h>
#include "MTIBuffer.hpp"
#include "MTITexture.hpp"

#include <execinfo.h>
#include <sys/mman.h>
#include <objc/runtime.h>

MTLPP_BEGIN

void* MTIBufferTrace::ContentsImpl(id Obj, SEL Cmd, Super::ContentsType::DefinedIMP Original)
{
	return Original(Obj, Cmd);
}

void MTIBufferTrace::DidModifyRangeImpl(id Obj, SEL Cmd, Super::DidModifyRangeType::DefinedIMP Original, NSRange Range)
{
	Original(Obj, Cmd, Range);
}

id<MTLTexture> MTIBufferTrace::NewTextureWithDescriptorOffsetBytesPerRowImpl(id Obj, SEL Cmd, Super::NewTextureWithDescriptorOffsetBytesPerRowType::DefinedIMP Original, MTLTextureDescriptor* Desc, NSUInteger Offset, NSUInteger BPR)
{
	return MTITextureTrace::Register(Original(Obj, Cmd, Desc, Offset, BPR));
}

INTERPOSE_DEFINITION(MTIBufferTrace, AddDebugMarkerRange, void, NSString* Str, NSRange R)
{
	Original(Obj, Cmd, Str, R);
}
INTERPOSE_DEFINITION_VOID(MTIBufferTrace, RemoveAllDebugMarkers, void)
{
	Original(Obj, Cmd);
}

struct FMTLPPObjectCallStacks
{
	FMTLPPObjectCallStacks* Next;
	void* Callstack[16];
	int Num;
};

@interface FMTLPPObjectZombie : NSObject
{
@public
	Class OriginalClass;
	OSQueueHead* CallStacks;
}
@end

@implementation FMTLPPObjectZombie

-(id)init
{
	self = (FMTLPPObjectZombie*)[super init];
	if (self)
	{
		OriginalClass = nil;
		CallStacks = nullptr;
	}
	return self;
}

-(void)dealloc
{
	// Denied!
	return;
	
	[super dealloc];
}

- (nullable NSMethodSignature *)methodSignatureForSelector:(SEL)sel
{
	NSLog(@"Selector %@ sent to deallocated instance %p of class %@", NSStringFromSelector(sel), self, OriginalClass);
	
	if (CallStacks)
	{
		int j = 0;
		while (FMTLPPObjectCallStacks* Stack = (FMTLPPObjectCallStacks*)OSAtomicDequeue(CallStacks, offsetof(FMTLPPObjectCallStacks, Next)))
		{
			if (Stack->Num)
			{
				NSLog(@"Callstack: %d", j);
				char** Symbols = backtrace_symbols(Stack->Callstack, Stack->Num);
				for (int i = 0; i < Stack->Num && Symbols; i++)
				{
					NSLog(@"\t%d: %s", i, Symbols[i] ? Symbols[i] : "Unknown");
				}
			}
			j++;
		}
	}
	
	abort();
}
@end

static OSQueueHead* GetCallStackQueueHead(id Object)
{
	OSQueueHead* Head = (OSQueueHead*)objc_getAssociatedObject(Object, (void const*)&MTIBufferTrace::RetainImpl);
	if (!Head)
	{
		Head = new OSQueueHead;
		Head->opaque1 = nullptr;
		Head->opaque2 = 0;
		objc_setAssociatedObject(Object, (void const*)&MTIBufferTrace::RetainImpl, (id)Head, OBJC_ASSOCIATION_ASSIGN);
	}
	return Head;
}

#define MAKE_CALLSTACK(Name, Object) FMTLPPObjectCallStacks* Name = new FMTLPPObjectCallStacks;	\
	Stack->Next = nullptr;	\
	Stack->Num = backtrace(Stack->Callstack, 16);	\
	OSAtomicEnqueue(GetCallStackQueueHead(Object), Stack, offsetof(FMTLPPObjectCallStacks, Next))

void MTIBufferTrace::RetainImpl(id Object, SEL Selector, void(*RetainPtr)(id,SEL))
{
	MAKE_CALLSTACK(Stack, Object);
	
	RetainPtr(Object, Selector);
}

void MTIBufferTrace::ReleaseImpl(id Object, SEL Selector, void(*ReleasePtr)(id,SEL))
{
	MAKE_CALLSTACK(Stack, Object);
	
	ReleasePtr(Object, Selector);
}

void MTIBufferTrace::DeallocImpl(id Object, SEL Selector, void(*DeallocPtr)(id,SEL))
{
	MAKE_CALLSTACK(Stack, Object);
	OSQueueHead* Head = GetCallStackQueueHead(Object);
	
	// First call the destructor and then release the memory - like C++ placement new/delete
	objc_destructInstance(Object);
	
	Class CurrentClass = [Object class];
	object_setClass(Object, [FMTLPPObjectZombie class]);
	FMTLPPObjectZombie* ZombieSelf = (FMTLPPObjectZombie*)Object;
	ZombieSelf->OriginalClass = CurrentClass;
	ZombieSelf->CallStacks = Head;
//	DeallocPtr(Object, Selector);
}

INTERPOSE_PROTOCOL_REGISTER(MTIBufferTrace, id<MTLBuffer>);


MTLPP_END
