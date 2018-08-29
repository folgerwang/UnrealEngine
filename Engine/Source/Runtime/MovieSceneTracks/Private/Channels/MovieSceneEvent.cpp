// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneEvent.h"

#if WITH_EDITORONLY_DATA

#include "EdGraph/EdGraph.h"
#include "K2Node_FunctionEntry.h"
#include "EdGraphSchema_K2.h"
#include "UObject/PropertyPortFlags.h"

UK2Node_FunctionEntry* FMovieSceneEvent::GetFunctionEntry() const
{
	return CastChecked<UK2Node_FunctionEntry>(FunctionEntry.Get(), ECastCheckedType::NullAllowed);
}

void FMovieSceneEvent::SetFunctionEntry(UK2Node_FunctionEntry* InFunctionEntry)
{
	FunctionEntry = InFunctionEntry;
	CacheFunctionName();
}

bool FMovieSceneEvent::IsBoundToBlueprint() const
{
	return IsValidFunction(GetFunctionEntry());
}

bool FMovieSceneEvent::IsValidFunction(UK2Node_FunctionEntry* Function)
{
	// Must have a single non-reference parameter on the function
	if (!Function || Function->IsPendingKill() || Function->GetGraph()->IsPendingKill())
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
	UK2Node_FunctionEntry* Node = GetFunctionEntry();

	FunctionName = NAME_None;

	if (IsValidFunction(Node))
	{
		UEdGraph* Graph = Node->GetGraph();
		if (Graph)
		{
			FunctionName = Graph->GetFName();
		}
	}
}

#endif // WITH_EDITORONLY_DATA


void FMovieSceneEvent::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading() && !Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE))
	{
		CacheFunctionName();
	}
#endif
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
