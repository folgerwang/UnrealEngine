// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimNode_Root.h"
#include "AnimNode_StateResult.generated.h"

// Root node of an state machine state (sink node).
// We dont use AnimNode_Root to let us distinguish these nodes in the property list at link time.
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_StateResult : public FAnimNode_Root
{
	GENERATED_USTRUCT_BODY()

	/** Used to upgrade old FAnimNode_Roots to FAnimNode_StateResult */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);
};

template<>
struct TStructOpsTypeTraits<FAnimNode_StateResult>
	: public TStructOpsTypeTraitsBase2<FAnimNode_StateResult>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
