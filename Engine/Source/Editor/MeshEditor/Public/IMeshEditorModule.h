// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "Editor.h"
#include "IMeshEditorMode.h"
#include "EditorModeManager.h"


/**
 * The public interface to this module
 */
class IMeshEditorModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IMeshEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IMeshEditorModule >( "MeshEditor" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "MeshEditor" );
	}

	/** Stores the FEditorMode ID of the associated editor mode */
	static inline FEditorModeID GetEditorModeID()
	{
		static FEditorModeID MeshEditorFeatureName = FName(TEXT("MeshEditor"));
		return MeshEditorFeatureName;
	}

	/** Returns the Level Editor's Mesh Editor Mode as an interface for external systems */
	virtual IMeshEditorMode* GetLevelEditorMeshEditorMode() = 0;

};

