// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

enum class EEditableMeshElementType;

class IMeshEditorMode 
{
public:
	/** Returns the current element selection mode in operation */
	virtual EEditableMeshElementType GetMeshElementSelectionMode() const = 0;

	/** Sets the element selection mode to use */
	virtual void SetMeshElementSelectionMode(EEditableMeshElementType ElementType) = 0;

	/** Set up hooks for external menu systems like VR Editor */
	virtual void RegisterWithExternalMenuSystem() = 0;

	/** Unregister from any external menu systems */
	virtual void UnregisterWithExternalMenuSystem() = 0;
};