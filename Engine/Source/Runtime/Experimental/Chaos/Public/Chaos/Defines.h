// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#ifndef COMPILE_ID_TYPES_AS_INTS
#define COMPILE_ID_TYPES_AS_INTS 0
#endif

#include <functional>
#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "CoreTypes.h"
#include "Logging/MessageLog.h"
#include "Misc/CoreMiscDefines.h"

#else
#include <stdint.h>
#define PI 3.14159
#define check(condition)

typedef int32_t int32;
#endif

namespace Chaos
{
#if COMPILE_ID_TYPES_AS_INTS
typedef uint32 IslandId;

static FORCEINLINE uint32 ToValue(uint32 Id) { return Id; }
#else
#define CREATEIDTYPE(IDNAME) \
    class IDNAME \
    { \
      public: \
        IDNAME() {} \
        IDNAME(const uint32 InValue) : Value(InValue) {} \
        bool operator==(const IDNAME& Other) const { return Value == Other.Value; } \
        uint32 Value; \
    }

CREATEIDTYPE(IslandId);

template<class T_ID>
static uint32 ToValue(T_ID Id)
{
    return Id.Value;
}
#endif
}