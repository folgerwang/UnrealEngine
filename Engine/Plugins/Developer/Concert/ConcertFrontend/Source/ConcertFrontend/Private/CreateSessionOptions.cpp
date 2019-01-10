// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CreateSessionOptions.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EditorStyleSet.h"
#include "EditorFontGlyphs.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "CreateSessionDetails"

TSharedRef<IDetailCustomization> FCreateSessionDetails::MakeInstance()
{
	return MakeShared<FCreateSessionDetails>();
}

void FCreateSessionDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	// Server Name
	{
		TSharedRef<IPropertyHandle> ServerNamePropertyHandle = InDetailLayout.GetProperty("ServerName");
		ServerNamePropertyHandle->MarkHiddenByCustomization();

		FString ServerName;
		ServerNamePropertyHandle->GetValue(ServerName);


		// Always display the server first
		IDetailCategoryBuilder& ServerCategoryBuilder = InDetailLayout.EditCategory("Server", FText::GetEmpty(), ECategoryPriority::Important);
		ServerCategoryBuilder.RestoreExpansionState(false);
		ServerCategoryBuilder.AddCustomRow(ServerNamePropertyHandle->GetPropertyDisplayName())
			.NameContent()
			[
				ServerNamePropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MaxDesiredWidth(600.f)
			.VAlign(VAlign_Center)
			[
					SNew(STextBlock)
					.Text(FText::FromString(ServerName))
					.Font(InDetailLayout.GetDetailFont())
					.ToolTipText(ServerNamePropertyHandle->GetToolTipText())
			];
	}

	// Session Name
	{
		SessionNamePropertyHandle = InDetailLayout.GetProperty("SessionName");
		SessionNamePropertyHandle->MarkHiddenByCustomization();
	
		FString SessionName;
		SessionNamePropertyHandle->GetValue(SessionName);

		// Always display the session settings in second
		IDetailCategoryBuilder& SessionSettingsCategoryBuilder = InDetailLayout.EditCategory("Session Settings", FText::GetEmpty(), ECategoryPriority::Important);
		SessionSettingsCategoryBuilder.RestoreExpansionState(false);
		SessionSettingsCategoryBuilder.AddCustomRow(SessionNamePropertyHandle->GetPropertyDisplayName())
			.NameContent()
			[
				SessionNamePropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.VAlign(VAlign_Center)
			.MaxDesiredWidth(600.f)
			[
				SNew(SEditableTextBox)
				.SelectAllTextOnCommit(true)
				.SelectAllTextWhenFocused(true)
				.ClearKeyboardFocusOnCommit(false)
				.ToolTipText(SessionNamePropertyHandle->GetToolTipText())
				.HintText(LOCTEXT("HintSessionName", "Enter a name"))
				.OnTextChanged(this, &FCreateSessionDetails::HandleSessionNameChanged)
			];
		
	}


	IDetailCategoryBuilder& SessionDataManagementCategory = InDetailLayout.EditCategory("Session Data Management");
	SessionDataManagementCategory.InitiallyCollapsed(true);

	// Session to restore
	{
		SessionToRestorePropertyHandle = InDetailLayout.GetProperty("SessionToRestore");
		SessionToRestorePropertyHandle->MarkHiddenByCustomization();

		SessionToRestoreEnabledPropertyHandle = InDetailLayout.GetProperty("bSessionToRestoreEnabled");
		SessionToRestoreEnabledPropertyHandle->MarkHiddenByCustomization();

		// Add options for the load of a save 
		TSharedRef<IPropertyHandle> SessionToRestoreOptionsPropertyHandle = InDetailLayout.GetProperty("SessionToRestoreOptions");
		SessionToRestoreOptionsPropertyHandle->MarkHiddenByCustomization();

		TSharedPtr<IPropertyHandleArray> SessionToRestoreOptionsPropertyArray = SessionToRestoreOptionsPropertyHandle->AsArray();
		uint32 Size;
		SessionToRestoreOptionsPropertyArray->GetNumElements(Size);

		if (Size > 0)
		{
			SessionsToRestoreSet.Empty(Size);

			for (uint32 i = 0; i < Size; i++)
			{
				FString* Value = new FString();
				SessionToRestoreOptionsPropertyArray->GetElement(i)->GetValue(*Value);
				SessionToRestoreOptions.Add(MakeShareable(Value));
				SessionsToRestoreSet.Emplace(*Value, i);
			}

			 SAssignNew(SessionToRestoreComboBox, STextComboBox)
				.OptionsSource(&SessionToRestoreOptions)
				.OnSelectionChanged(this, &FCreateSessionDetails::HandleSessionToRestoreSelectionChanged)
				.IsEnabled(this, &FCreateSessionDetails::IsSessionToRestoreEnabled);

			TSharedRef<SWidget> SessionToRestoreLabel = SessionToRestorePropertyHandle->CreatePropertyNameWidget();
			SessionToRestoreLabel->SetEnabled(TAttribute<bool>(this, &FCreateSessionDetails::IsSessionToRestoreEnabled));

			SessionDataManagementCategory.AddCustomRow(LOCTEXT("LoadSaveFilter", "Load Save"))
				.NameContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &FCreateSessionDetails::HandleSessionToRestoreCheckChanged)
						.ToolTipText(SessionToRestorePropertyHandle->GetToolTipText())
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					[
						SessionToRestoreLabel
					]
				]
				.ValueContent()
				[
					SessionToRestoreComboBox.ToSharedRef()
				];
		}
	}
	
	// Save Session As
	{
		SaveSessionAsPropertyHandle = InDetailLayout.GetProperty("SaveSessionAs");
		SaveSessionAsPropertyHandle->MarkHiddenByCustomization();

		SaveSessionAsEnabledPropertyHandle = InDetailLayout.GetProperty("bSaveSessionAsEnabled");
		SaveSessionAsEnabledPropertyHandle->MarkHiddenByCustomization();

		TSharedRef<SWidget> SaveSessionLabel = SaveSessionAsPropertyHandle->CreatePropertyNameWidget();
		SaveSessionLabel->SetEnabled(TAttribute<bool>(this, &FCreateSessionDetails::IsSaveSessionAsEnabled));

		SessionDataManagementCategory.AddCustomRow(LOCTEXT("SaveSessionAsFilter", "Save Session As"))
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &FCreateSessionDetails::HandleSaveSessionAsCheckChanged)
					.ToolTipText(SaveSessionAsPropertyHandle->GetToolTipText())
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				[
					SaveSessionLabel
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(2.f, 0.f)
				[
					SAssignNew(SaveSessionAsWarningIcon, STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle(TEXT("FontAwesome.10")))
					.Justification(ETextJustify::Center)
					.Visibility(this, &FCreateSessionDetails::HandleSaveSessionAsWarningVisibility)
					.Text(this, &FCreateSessionDetails::HandleSaveSessionAsWarningGlyph)
					.ColorAndOpacity(this, &FCreateSessionDetails::HandleSaveSessionAsWarningColor)
					.ToolTipText(this, &FCreateSessionDetails::HandleSaveSessionAsWarningToolTip)
				]
			]
			.ValueContent()
			[
				SAssignNew(SaveSessionAsTextBox,SEditableTextBox)
				.SelectAllTextOnCommit(true)
				.SelectAllTextWhenFocused(true)
				.ClearKeyboardFocusOnCommit(false)
				.ToolTipText(SaveSessionAsPropertyHandle->GetToolTipText())
				.HintText(LOCTEXT("HintSaveSessionAsName", "Enter a save name"))
				.IsEnabled(this, &FCreateSessionDetails::IsSaveSessionAsEnabled)
				.OnTextChanged(this, &FCreateSessionDetails::HandleSaveSessionAsChanged)
			];
	}
}

void FCreateSessionDetails::HandleSessionNameChanged(const FText& SessionName)
{
	FString OldSessionName;
	SessionNamePropertyHandle->GetValue(OldSessionName);

	const FString Session = SessionName.ToString();
	SessionNamePropertyHandle->SetValue(Session, EPropertyValueSetFlags::NotTransactable);

	// Update the save session as
	{
		bool bSaveSessionAsEnabled = false;
		SaveSessionAsEnabledPropertyHandle->GetValue(bSaveSessionAsEnabled);
		FString SaveSessionAs;
		SaveSessionAsPropertyHandle->GetValue(SaveSessionAs);
		if (!bSaveSessionAsEnabled || SaveSessionAs == OldSessionName)
		{
			SaveSessionAsTextBox->SetText(SessionName);
		}
	}

	// Update the session to restore
	if (SessionToRestoreComboBox.IsValid())
	{
		if (bAutoUpdateSessionToRestoreSelection)
		{
			if (SessionsToRestoreSet.Contains(Session))
			{
				SessionToRestoreComboBox->SetSelectedItem(SessionToRestoreOptions[SessionsToRestoreSet[Session]]);
			}
			else
			{
				SessionToRestoreComboBox->ClearSelection();
			}
		}
		else
		{
			TSharedPtr<FString> SelectedSessionToRestore =  SessionToRestoreComboBox->GetSelectedItem();
			if (SelectedSessionToRestore.IsValid())
			{
				bAutoUpdateSessionToRestoreSelection = *SelectedSessionToRestore.Get() == Session;
			}
		}
	}
}

void FCreateSessionDetails::HandleSessionToRestoreSelectionChanged(TSharedPtr<FString> SelectedString, ESelectInfo::Type SelectInfo)
{
	SessionToRestorePropertyHandle->SetValue(SelectedString.IsValid() ? *SelectedString.Get() : FString(), EPropertyValueSetFlags::NotTransactable);

	if (SelectInfo != ESelectInfo::Direct)
	{
		FString SessionName;
		SessionNamePropertyHandle->GetValue(SessionName);
		bAutoUpdateSessionToRestoreSelection = SessionName == (SelectedString.IsValid() ? *SelectedString.Get() : FString());
	}
}

void FCreateSessionDetails::HandleSessionToRestoreCheckChanged(ECheckBoxState CheckState)
{
	if (CheckState == ECheckBoxState::Checked)
	{
		SessionToRestoreEnabledPropertyHandle->SetValue(true, EPropertyValueSetFlags::NotTransactable);
	}
	else if (CheckState == ECheckBoxState::Unchecked)
	{
		bAutoUpdateSessionToRestoreSelection = true;
		SessionToRestoreEnabledPropertyHandle->SetValue(false, EPropertyValueSetFlags::NotTransactable);

		if (SessionToRestoreComboBox.IsValid())
		{
			FString SessionName;
			SessionNamePropertyHandle->GetValue(SessionName);

			if (SessionsToRestoreSet.Contains(SessionName))
			{
				SessionToRestoreComboBox->SetSelectedItem(SessionToRestoreOptions[SessionsToRestoreSet[SessionName]]);
			}
			else
			{
				SessionToRestoreComboBox->ClearSelection();
			}
		}
	}
}

bool FCreateSessionDetails::IsSessionToRestoreEnabled() const
{
	bool bEnabled;
	SessionToRestoreEnabledPropertyHandle->GetValue(bEnabled);
	return bEnabled;
}


void FCreateSessionDetails::HandleSaveSessionAsChanged(const FText& SessionName)
{
	SaveSessionAsPropertyHandle->SetValue(SessionName.ToString(), EPropertyValueSetFlags::NotTransactable);
}

void FCreateSessionDetails::HandleSaveSessionAsCheckChanged(ECheckBoxState CheckState)
{
	if (CheckState == ECheckBoxState::Checked)
	{
		SaveSessionAsEnabledPropertyHandle->SetValue(true, EPropertyValueSetFlags::NotTransactable);

	}
	else if (CheckState == ECheckBoxState::Unchecked)
	{
		SaveSessionAsEnabledPropertyHandle->SetValue(false, EPropertyValueSetFlags::NotTransactable);
		FString SessionName;
		SessionNamePropertyHandle->GetValue(SessionName);
		SaveSessionAsTextBox->SetText(FText::FromString(SessionName));
	}
}

bool FCreateSessionDetails::IsSaveSessionAsEnabled() const
{
	bool bEnabled;
	SaveSessionAsEnabledPropertyHandle->GetValue(bEnabled);
	return bEnabled;
}

EVisibility FCreateSessionDetails::HandleSaveSessionAsWarningVisibility() const
{
	FString SaveSessionAs;
	SaveSessionAsPropertyHandle->GetValue(SaveSessionAs);
	if (!IsSaveSessionAsEnabled() || (!SessionsToRestoreSet.Contains(SaveSessionAs) && !SaveSessionAs.IsEmpty()))
	{
		return EVisibility::Collapsed;
	}
	
	return EVisibility::Visible;
}

FText FCreateSessionDetails::HandleSaveSessionAsWarningGlyph() const
{
	FString SaveSessionAs;
	SaveSessionAsPropertyHandle->GetValue(SaveSessionAs);
	if (SaveSessionAs.IsEmpty())
	{
		return FEditorFontGlyphs::Exclamation_Circle;
	}
	
	return FEditorFontGlyphs::Exclamation_Triangle;
}

FSlateColor FCreateSessionDetails::HandleSaveSessionAsWarningColor() const
{
	FString SaveSessionAs;
	SaveSessionAsPropertyHandle->GetValue(SaveSessionAs);
	if (SaveSessionAs.IsEmpty())
	{
		return FEditorStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("FlatButton.Danger")).Normal.TintColor;
	}

	return  FEditorStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("FlatButton.Warning")).Normal.TintColor;
}

FText FCreateSessionDetails::HandleSaveSessionAsWarningToolTip() const
{
	FString SaveSessionAs;
	SaveSessionAsPropertyHandle->GetValue(SaveSessionAs);
	if (SaveSessionAs.IsEmpty())
	{
		return LOCTEXT("SaveSessionAsWontSave", "The session won't be saved.");
	}

	return LOCTEXT("SaveSessionAsWillRemplaceASave", "The previous save will be replaced.");
}

#undef  LOCTEXT_NAMESPACE
