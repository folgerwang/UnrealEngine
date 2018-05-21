// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class UStruct;
class UBlueprint;
struct FEdGraphPinType;
class UControlRigGraphNode;
class UEdGraph;
class UEdGraphPin;

struct FControlRigBlueprintUtils
{

/** 
 * Gets a new name for a unit member struct
 * @param	InBlueprint			The blueprint we want to create a new member in
 * @param	InStructTemplate	The struct template we want to use
 * @return the name of the new member
 */
static FName GetNewUnitMemberName(UBlueprint* InBlueprint, UStruct* InStructTemplate);

/** 
 * Adds a new unit member struct. 
 * @param	InBlueprint			The blueprint we want to create a new member in
 * @param	InStructTemplate	The struct template we want to use
 * @return the name of the new member, or NAME_None if the member was not created
 */
static FName AddUnitMember(UBlueprint* InBlueprint, UStruct* InStructTemplate);

/** 
 * Gets a new name for a member
 * @param	InBlueprint			The blueprint we want to create a new member in
 * @param	InVariableDesc		A description of the variable type, used to create a base variable name
 * @return the name of the new member
 */
static FName GetNewPropertyMemberName(UBlueprint* InBlueprint, const FString& InVariableDesc);

/** 
 * Adds a new property member. 
 * @param	InBlueprint			The blueprint we want to create a new member in
 * @param	InPinType			The type of the property we want to create
 * @param	InVariableDesc		A description of the variable type, used to create a base variable name
 * @return the name of the new member, or NAME_None if the member was not created
 */
static FName AddPropertyMember(UBlueprint* InBlueprint, const FEdGraphPinType& InPinType, const FString& InVariableDesc);

/**
 * Instantiate a node in the specified graph for the supplied property
 * @param	InGraph				The graph to create the node in
 * @param	InPropertyName		The property the node represents
 * @param	InLocation			Optional location to create the node at
 * @return the new graph node
 */
static UControlRigGraphNode* InstantiateGraphNodeForProperty(UEdGraph* InGraph, const FName& InPropertyName, const FVector2D& InLocation = FVector2D::ZeroVector);

/**
 * Check whether we can instantiate a node in the specified graph for the specified property
 * We don't allow properties to be instantiated more than once
 * @param	InGraph				The graph to create the node in
 * @param	InPropertyName		The property the node represents
 * @return true if the node can be instantiated
 */
static bool CanInstantiateGraphNodeForProperty(UEdGraph* InGraph, const FName& InPropertyName);

/** Call a function for each valid rig unit struct */
static void ForAllRigUnits(TFunction<void(UStruct*)> InFunction);

/** Handle blueprint node reconstruction */
static void HandleReconstructAllNodes(UBlueprint* InBlueprint);

/** Handle blueprint node refresh */
static void HandleRefreshAllNodes(UBlueprint* InBlueprint);

/** Handle variables getting renamed */
static void HandleRenameVariableReferencesEvent(UBlueprint* InBlueprint, UClass* InVariableClass, const FName& InOldVarName, const FName& InNewVarName);

/** remove the variable if not used by anybody else but ToBeDeleted*/
static void RemoveMemberVariableIfNotUsed(UBlueprint* Blueprint, const FName VarName, UControlRigGraphNode* ToBeDeleted);
};