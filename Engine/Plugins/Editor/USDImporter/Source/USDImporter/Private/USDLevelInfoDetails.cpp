// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "USDLevelInfoDetails.h"
#include "USDLevelInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Engine/World.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "IPythonScriptPlugin.h"

#define LOCTEXT_NAMESPACE "FUSDLevelInfoDetails"



TSharedRef<IDetailCustomization> FUSDLevelInfoDetails::MakeInstance()
{
	return MakeShareable(new FUSDLevelInfoDetails);
}

void FUSDLevelInfoDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout.GetSelectedObjects();

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if ( CurrentObject.IsValid() )
		{
			AUSDLevelInfo* CurrentUSDLevelInfo = Cast<AUSDLevelInfo>(CurrentObject.Get());
			if (CurrentUSDLevelInfo != NULL)
			{
				USDLevelInfo = CurrentUSDLevelInfo;
				break;
			}
		}
	}

	DetailLayout.EditCategory( "USD" )
	.AddCustomRow( NSLOCTEXT("FUSDLevelInfoDetails", "SaveUSD", "Save USD") )
		.NameContent()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( NSLOCTEXT("FUSDLevelInfoDetails", "SaveUSD", "Save USD") )
		]
		.ValueContent()
		.MaxDesiredWidth(125.f)
		.MinDesiredWidth(125.f)
		[
			SNew(SButton)
			.ContentPadding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked( this, &FUSDLevelInfoDetails::OnSaveUSD )
			[
				SNew( STextBlock )
				.Font( IDetailLayoutBuilder::GetDetailFont() )
				.Text( NSLOCTEXT("FUSDLevelInfoDetails", "SaveUSD", "Save USD") )
			]
		];
}

FReply FUSDLevelInfoDetails::OnSaveUSD()
{
	if (!USDLevelInfo.IsValid())
	{
		return FReply::Handled();
	}
	
	if (IPythonScriptPlugin::Get()->IsPythonAvailable())
	{
		IPythonScriptPlugin::Get()->ExecPythonCommand(TEXT("import usd_unreal.export_level; usd_unreal.export_level.export_current_level(None)"));
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
