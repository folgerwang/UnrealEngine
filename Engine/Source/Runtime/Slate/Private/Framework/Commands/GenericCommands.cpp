// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "GenericCommands"

void FGenericCommands::RegisterCommands()
{
	UI_COMMAND( Cut, "Cut", "Cut selection", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::X ) )
	UI_COMMAND( Copy, "Copy", "Copy selection", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::C ) )
	UI_COMMAND( Paste, "Paste", "Paste clipboard contents", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::V ) )
#if PLATFORM_MAC
	UI_COMMAND( Duplicate, "Duplicate", "Duplicate selection", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Command, EKeys::W ) )
#else
	UI_COMMAND( Duplicate, "Duplicate", "Duplicate selection", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::W ) )
#endif
	UI_COMMAND( Undo, "Undo", "Undo last action", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::Z ) )
	UI_COMMAND( Redo, "Redo", "Redo last action", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::Y ) )

	UI_COMMAND(Delete, "Delete", "Delete current selection", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete), FInputChord(EKeys::BackSpace))
	
	UI_COMMAND( Rename, "Rename", "Rename current selection", EUserInterfaceActionType::Button, FInputChord( EKeys::F2 ) )
	UI_COMMAND( SelectAll, "Select All", "Select everything in the current scope", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::A ) )
	
#if PLATFORM_MAC
	UI_COMMAND( Preferences, "Preferences...", "Preferences", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::Comma ) )
	UI_COMMAND( Help, "Help", "Help", EUserInterfaceActionType::Button, FInputChord( EModifierKey::None, EKeys::F1 ) )
	UI_COMMAND( Hide, "Hide", "Hide", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::H ) )
	UI_COMMAND( HideOthers, "HideOthers", "Hide Others", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control | EModifierKey::Alt, EKeys::H ) )
	UI_COMMAND( Minimize, "Minimize", "Minimize", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::M ) )
	UI_COMMAND( Close, "Close", "Close", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::W ) )
	UI_COMMAND( Quit, "Quit", "Quit", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::Q ) )		 
#endif
}

#undef LOCTEXT_NAMESPACE
