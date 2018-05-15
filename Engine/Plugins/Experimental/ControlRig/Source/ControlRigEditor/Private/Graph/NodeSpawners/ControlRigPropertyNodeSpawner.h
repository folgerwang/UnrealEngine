// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSignature.h"
#include "BlueprintFieldNodeSpawner.h"
#include "ControlRigPropertyNodeSpawner.generated.h"

class UControlRigGraphNode;

UCLASS(Transient)
class UControlRigPropertyNodeSpawner : public UBlueprintFieldNodeSpawner
{
	GENERATED_BODY()

public:
	/**
	 * Creates a new UControlRigPropertyNodeSpawner, charged with spawning 
	 * a member-variable node (for a variable that has an associated UProperty) 
	 * 
	 * @param  NodeClass	The node type that you want the spawner to spawn.
	 * @param  VarProperty	The property that represents the member-variable you want nodes spawned for.
	 * @param  VarContext	The graph that the local variable belongs to.
	 * @param  Outer		Optional outer for the new spawner (if left null, the transient package will be used).
	 * @return A newly allocated instance of this class.
	 */
	static UControlRigPropertyNodeSpawner* CreateFromProperty(TSubclassOf<UControlRigGraphNode> NodeClass, UProperty const* VarProperty, UEdGraph* VarContext = nullptr, UObject* Outer = nullptr);

	// UBlueprintNodeSpawner interface
	virtual void Prime() override;
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	virtual bool IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const override;
	// End UBlueprintNodeSpawner interface

	/**
	 * Accessor to the variable's property. Will be null if this is for a local 
	 * variable (as they don't have UProperties associated with them).
	 * 
	 * @return Null if this wraps a local variable (or if the variable property is stale), otherwise the property this was initialized with. 
	 */
	UProperty const* GetProperty() const;

	/**
	 * Utility function for easily accessing the variable's type (needs to pull
	 * the information differently if it is a local variable as opposed to a
	 * member variable with a UProperty).
	 * 
	 * @return A struct detailing the wrapped variable's type.
	 */
	FEdGraphPinType GetVarType() const;

private:
	/**
	 * Utility function for easily accessing the variable's name (needs to pull
	 * the information differently if it is a local variable as opposed to a
	 * member variable with a UProperty).
	 * 
	 * @return A friendly, user presentable, name for the variable that this wraps. 
	 */
	FText GetVariableName() const;
};
