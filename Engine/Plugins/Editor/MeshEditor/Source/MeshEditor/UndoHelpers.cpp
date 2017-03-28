// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UndoHelpers.h"

namespace UndoHelpers
{
	bool IsUndoSystemAvailable()
	{
		return GUndo != nullptr;
	}


	void StoreUndo( UObject* Object, TUniquePtr<FChange> UndoChange )
	{
		// Did you forget to use an FScopedTransaction?  If GUndo was null, then most likely we forgot to wrap this call within an editor transaction.
		// The only exception is in Simulate mode, where Undo is not allowed.
		check( IsUndoSystemAvailable() || GEditor == nullptr || GEditor->bIsSimulatingInEditor );

		if( IsUndoSystemAvailable() )
		{
			GUndo->StoreUndo( Object, MoveTemp( UndoChange ) );
		}
	}
}


