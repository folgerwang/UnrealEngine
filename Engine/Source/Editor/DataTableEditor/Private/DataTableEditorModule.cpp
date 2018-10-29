// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DataTableEditorModule.h"
#include "Modules/ModuleManager.h"
#include "IDataTableEditor.h"
#include "DataTableEditor.h"
#include "CompositeDataTableEditor.h"
#include "Engine/CompositeDataTable.h"

IMPLEMENT_MODULE( FDataTableEditorModule, DataTableEditor );


const FName FDataTableEditorModule::DataTableEditorAppIdentifier( TEXT( "DataTableEditorApp" ) );

void FDataTableEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
}


void FDataTableEditorModule::ShutdownModule()
{
	MenuExtensibilityManager.Reset();
}


TSharedRef<IDataTableEditor> FDataTableEditorModule::CreateDataTableEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UDataTable* Table)
{
	if (Cast<UCompositeDataTable>(Table) != nullptr)
	{
		return CreateCompositeDataTableEditor(Mode, InitToolkitHost, Table);
	}

	return CreateStandardDataTableEditor(Mode, InitToolkitHost, Table);
}

TSharedRef<IDataTableEditor> FDataTableEditorModule::CreateStandardDataTableEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UDataTable* Table)
{
	TSharedRef< FDataTableEditor > NewDataTableEditor( new FDataTableEditor() );
	NewDataTableEditor->InitDataTableEditor( Mode, InitToolkitHost, Table );
	return NewDataTableEditor;
}

TSharedRef<IDataTableEditor> FDataTableEditorModule::CreateCompositeDataTableEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UDataTable* Table)
{
	TSharedRef< FCompositeDataTableEditor > NewDataTableEditor(new FCompositeDataTableEditor());
	NewDataTableEditor->InitDataTableEditor(Mode, InitToolkitHost, Table);
	return NewDataTableEditor;
}
