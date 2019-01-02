// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DatasmithSceneActorDetailsPanel.h"

#include "DatasmithContentEditorModule.h"
#include "DatasmithSceneActor.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "UObject/UnrealType.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"

FDatasmithSceneActorDetailsPanel::FDatasmithSceneActorDetailsPanel()
	: bReimportDeletedActors(false)
{
}

TSharedRef<IDetailCustomization> FDatasmithSceneActorDetailsPanel::MakeInstance()
{
	return MakeShared< FDatasmithSceneActorDetailsPanel >();
}

void FDatasmithSceneActorDetailsPanel::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();

	TSharedRef<SWrapBox> WrapBox = SNew(SWrapBox).UseAllottedWidth(true);

	FString CategoryName = TEXT("Datasmith");
	IDetailCategoryBuilder& ActionsCategory = DetailLayoutBuilder.EditCategory(*CategoryName);

	// Add the scene row first
	ActionsCategory.AddProperty( DetailLayoutBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( ADatasmithSceneActor, Scene ) ) );

	// Add the update actors button
	const FText ButtonCaption = FText::FromString( TEXT("Update actors from Scene") );
	const FText CheckBoxCaption = FText::FromString( TEXT("Respawn deleted actors") );

	auto IsChecked = [ this ]() -> ECheckBoxState { return bReimportDeletedActors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; };
	auto CheckedStateChanged = [ this ]( ECheckBoxState NewState ) { bReimportDeletedActors = ( NewState == ECheckBoxState::Checked ); };

	WrapBox->AddSlot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(ButtonCaption)
				.OnClicked(	FOnClicked::CreateSP(this, &FDatasmithSceneActorDetailsPanel::OnExecuteAction) )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(SCheckBox)
				.ToolTipText(CheckBoxCaption)
				.IsChecked_Lambda( IsChecked )
				.OnCheckStateChanged_Lambda( CheckedStateChanged )
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text( CheckBoxCaption )
			]
		];

	ActionsCategory.AddCustomRow(FText::GetEmpty())
		.ValueContent()
		[
			WrapBox
		];
}

FReply FDatasmithSceneActorDetailsPanel::OnExecuteAction()
{
	IDatasmithContentEditorModule& DatasmithContentEditorModule = FModuleManager::GetModuleChecked< IDatasmithContentEditorModule >( TEXT("DatasmithContentEditor") );

	for ( const TWeakObjectPtr< UObject >& SelectedObject : SelectedObjectsList )
	{
		ADatasmithSceneActor* SceneActor = Cast< ADatasmithSceneActor >( SelectedObject.Get() );
		DatasmithContentEditorModule.GetSpawnDatasmithSceneActorsHandler().ExecuteIfBound( SceneActor, bReimportDeletedActors );
	}

	return FReply::Handled();
}
