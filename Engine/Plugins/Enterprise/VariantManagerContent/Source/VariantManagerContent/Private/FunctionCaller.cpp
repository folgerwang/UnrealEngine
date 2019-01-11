// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FunctionCaller.h"

#if WITH_EDITORONLY_DATA

#include "EdGraph/EdGraph.h"
#include "K2Node_FunctionEntry.h"
#include "EdGraphSchema_K2.h"
#include "UObject/PropertyPortFlags.h"

// This file is based on MovieSceneEvent.cpp

UK2Node_FunctionEntry* FFunctionCaller::GetFunctionEntry() const
{
	return CastChecked<UK2Node_FunctionEntry>(FunctionEntry.Get(), ECastCheckedType::NullAllowed);
}

void FFunctionCaller::SetFunctionEntry(UK2Node_FunctionEntry* InFunctionEntry)
{
	FunctionEntry = InFunctionEntry;
	CacheFunctionName();
}

bool FFunctionCaller::IsBoundToBlueprint() const
{
	return IsValidFunction(GetFunctionEntry());
}

bool FFunctionCaller::IsValidFunction(UK2Node_FunctionEntry* Function)
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

void FFunctionCaller::CacheFunctionName()
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


void FFunctionCaller::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading() && !Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE))
	{
		CacheFunctionName();
	}
#endif
}

bool FFunctionCaller::IsValidFunction(UFunction* Function)
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
