// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneEvent.h"

#if WITH_EDITORONLY_DATA

#include "EdGraph/EdGraph.h"
#include "K2Node_FunctionEntry.h"
#include "EdGraphSchema_K2.h"
#include "UObject/PropertyPortFlags.h"

UK2Node_FunctionEntry* FMovieSceneEvent::GetFunctionEntry() const
{
	if (SoftBlueprintPath.IsNull())
	{
		// The function entry used to be serialized but is now only stored transiently. We use this pointer for the current lifecycle until the asset is saved, when we do the data upgrade.
		UK2Node_FunctionEntry* FunctionEntryPtr = CastChecked<UK2Node_FunctionEntry>(FunctionEntry_DEPRECATED.Get(), ECastCheckedType::NullAllowed);
		if (FunctionEntryPtr)
		{
			return FunctionEntryPtr;
		}
	}

	UK2Node_FunctionEntry* Cached = CastChecked<UK2Node_FunctionEntry>(CachedFunctionEntry.Get(), ECastCheckedType::NullAllowed);
	if (Cached)
	{
		return Cached;
	}

	if (GraphGuid.IsValid())
	{
		if (UBlueprint* Blueprint = SoftBlueprintPath.LoadSynchronous())
		{
			for (UEdGraph* Graph : Blueprint->FunctionGraphs)
			{
				if (Graph->GraphGuid == GraphGuid)
				{
					for (UEdGraphNode* Node : Graph->Nodes)
					{
						if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
						{
							CachedFunctionEntry = FunctionEntry;
							return FunctionEntry;
						}
					}
				}
			}
		}
	}

	return nullptr;
}

void FMovieSceneEvent::SetFunctionEntry(UK2Node_FunctionEntry* InFunctionEntry)
{
	UEdGraph* EdGraph = InFunctionEntry ? InFunctionEntry->GetGraph() : nullptr;
	if (EdGraph)
	{
		CachedFunctionEntry = InFunctionEntry;
		SoftBlueprintPath   = EdGraph->GetTypedOuter<UBlueprint>();
		GraphGuid           = EdGraph->GraphGuid;
	}
	else
	{
		CachedFunctionEntry = nullptr;
		SoftBlueprintPath   = nullptr;
		GraphGuid           = FGuid();
	}
	CacheFunctionName();
}

bool FMovieSceneEvent::IsBoundToBlueprint() const
{
	return IsValidFunction(GetFunctionEntry());
}

bool FMovieSceneEvent::IsValidFunction(UK2Node_FunctionEntry* Function)
{
	// Must have a single non-reference parameter on the function
	if (!Function || Function->IsPendingKill() || !Function->GetGraph() || Function->GetGraph()->IsPendingKill())
	{
		return false;
	}
	else if (Function->UserDefinedPins.Num() == 0)
	{
		return true;
	}
	else if (Function->UserDefinedPins.Num() != 1 || Function->UserDefinedPins[0]->PinType.bIsReference)
	{
		return false;
	}

	// Pin must be an object or interface property
	FName PinCategory = Function->UserDefinedPins[0]->PinType.PinCategory;
	return PinCategory == UEdGraphSchema_K2::PC_Object || PinCategory == UEdGraphSchema_K2::PC_Interface;
}

void FMovieSceneEvent::CacheFunctionName()
{
	UK2Node_FunctionEntry* Node  = GetFunctionEntry();
	UEdGraph*              Graph = Node ? Node->GetGraph() : nullptr;

	FunctionName = NAME_None;

	if (Graph && IsValidFunction(Node))
	{
		FunctionName = Graph->GetFName();
	}
}

#endif // WITH_EDITORONLY_DATA


void FMovieSceneEvent::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading() && !Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE))
	{
		// Re-cache the function name when loading in-editor in case of renamed function graphs and the like
		CacheFunctionName();
	}
#endif
}

bool FMovieSceneEvent::Serialize(FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	if (Ar.IsSaving())
	{
		// ---------------------------------------------------------------------------------------
		// Data upgrade for content that was saved with FunctionEntry_DEPRECATED instead of SoftFunctionGraph
		// We do this on save because there is no reliable way to ensure that FunctionGraph is fully loaded here
		// since the track may live inside or outside of a blueprint. When not fully loaded, GraphGuid is not correct.
		if (SoftBlueprintPath.IsNull())
		{
			// The function entry used to be serialized but is now only stored transiently. If this is set without the soft function graph being set, copy the graph reference over
			UK2Node_FunctionEntry* FunctionEntryPtr = CastChecked<UK2Node_FunctionEntry>(FunctionEntry_DEPRECATED.Get(), ECastCheckedType::NullAllowed);
			UEdGraph*              FunctionGraph    = FunctionEntryPtr ? FunctionEntryPtr->GetGraph() : nullptr;

			if (FunctionGraph)
			{
				CachedFunctionEntry = FunctionEntryPtr;
				SoftBlueprintPath   = FunctionGraph->GetTypedOuter<UBlueprint>();
				GraphGuid           = FunctionGraph->GraphGuid;
			}
		}
	}
#endif

	// Return false to ensure that the struct receives default serialization
	return false;
}

bool FMovieSceneEvent::IsValidFunction(UFunction* Function)
{
	// Must have a single non-reference parameter on the function
	if (!Function)
	{
		return false;
	}
	else if (Function->NumParms == 0)
	{
		return true;
	}
	else if (Function->NumParms != 1 || !Function->PropertyLink || (Function->PropertyLink->GetPropertyFlags() & CPF_ReferenceParm))
	{
		return false;
	}

	// Parameter must be an object or interface property
	return Function->PropertyLink->IsA<UObjectProperty>() || Function->PropertyLink->IsA<UInterfaceProperty>();
}
