// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

class AActor;
class UActorComponent;
class UBehaviorTreeComponent;
class UBlackboardData;
class UBTNode;

namespace BlueprintNodeHelpers
{
	AIMODULE_API FString CollectPropertyDescription(const UObject* Ob, const UClass* StopAtClass, const TArray<UProperty*>& PropertyData);
	AIMODULE_API void CollectPropertyData(const UObject* Ob, const UClass* StopAtClass, TArray<UProperty*>& PropertyData);
	AIMODULE_API uint16 GetPropertiesMemorySize(const TArray<UProperty*>& PropertyData);

	AIMODULE_API void CollectBlackboardSelectors(const UObject* Ob, const UClass* StopAtClass, TArray<FName>& KeyNames);
	AIMODULE_API void ResolveBlackboardSelectors(UObject& Ob, const UClass& StopAtClass, const UBlackboardData& BlackboardAsset);
	AIMODULE_API bool HasAnyBlackboardSelectors(const UObject* Ob, const UClass* StopAtClass);

	AIMODULE_API FString DescribeProperty(const UProperty* Prop, const uint8* PropertyAddr);
	AIMODULE_API void DescribeRuntimeValues(const UObject* Ob, const TArray<UProperty*>& PropertyData, TArray<FString>& RuntimeValues);

	AIMODULE_API void CopyPropertiesToContext(const TArray<UProperty*>& PropertyData, uint8* ObjectMemory, uint8* ContextMemory);
	AIMODULE_API void CopyPropertiesFromContext(const TArray<UProperty*>& PropertyData, uint8* ObjectMemory, uint8* ContextMemory);

	AIMODULE_API bool FindNodeOwner(AActor* OwningActor, UBTNode* Node, UBehaviorTreeComponent*& OwningComp, int32& OwningInstanceIdx);

	AIMODULE_API void AbortLatentActions(UActorComponent& OwnerOb, const UObject& Ob);

	FORCEINLINE bool HasBlueprintFunction(FName FuncName, const UObject& Object, const UClass& StopAtClass)
	{
		const UFunction* Function = Object.GetClass()->FindFunctionByName(FuncName);
		ensure(Function);
		return (Function != nullptr) && (Function->GetOuter() != &StopAtClass);
	}

	FORCEINLINE FString GetNodeName(const UObject& NodeObject)
	{
		return NodeObject.GetClass()->GetName().LeftChop(2);
	}

	//----------------------------------------------------------------------//
	// DEPRECATED
	//----------------------------------------------------------------------//
	UE_DEPRECATED(4.11, "This version of HasBlueprintFunction is deprecated. Please use the one taking reference to UObject and StopAtClass rather than a pointers.")
	AIMODULE_API bool HasBlueprintFunction(FName FuncName, const UObject* Object, const UClass* StopAtClass);
}
