// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


namespace UndoHelpers
{
	/** @return Returns true if the undo system is available right now.  When in Simulate Mode, we can't store undo states or use undo/redo features */
	extern MESHEDITOR_API bool IsUndoSystemAvailable();

	/** Saves undo state, if possible (e.g., not in Simulate mode.) */
	extern MESHEDITOR_API void StoreUndo( UObject* Object, TUniquePtr<FChange> UndoChange );
}