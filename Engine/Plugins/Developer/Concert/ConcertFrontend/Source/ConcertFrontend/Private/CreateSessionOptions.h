// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "PropertyHandle.h"
#include "Types/SlateEnums.h"
#include "UObject/ObjectMacros.h"

#include "CreateSessionOptions.generated.h"

class SEditableTextBox;
class STextBlock;
class STextComboBox;

class FCreateSessionDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailLayout) override;

private:

	void HandleSessionNameChanged(const FText& SessionName);

	void HandleSessionToRestoreSelectionChanged(TSharedPtr<FString> SelectedString, ESelectInfo::Type SelectInfo);

	void HandleSessionToRestoreCheckChanged(ECheckBoxState CheckState);

	bool IsSessionToRestoreEnabled() const;

	void HandleSaveSessionAsChanged(const FText& SessionName);

	void HandleSaveSessionAsCheckChanged(ECheckBoxState CheckState);

	EVisibility HandleSaveSessionAsWarningVisibility() const;

	FText HandleSaveSessionAsWarningGlyph() const;

	FSlateColor HandleSaveSessionAsWarningColor() const;

	FText HandleSaveSessionAsWarningToolTip() const;

	bool IsSaveSessionAsEnabled() const;

	TArray<TSharedPtr<FString>> SessionToRestoreOptions;
	TMap<FString, uint32> SessionsToRestoreSet;

	TSharedPtr<IPropertyHandle> SessionNamePropertyHandle;
	TSharedPtr<IPropertyHandle> SessionToRestorePropertyHandle;
	TSharedPtr<IPropertyHandle> SessionToRestoreEnabledPropertyHandle;
	TSharedPtr<IPropertyHandle> SaveSessionAsEnabledPropertyHandle;
	TSharedPtr<IPropertyHandle> SaveSessionAsPropertyHandle;

	bool bAutoUpdateSessionToRestoreSelection = true; 
	TSharedPtr<STextComboBox> SessionToRestoreComboBox;
	TSharedPtr<SEditableTextBox> SaveSessionAsTextBox;
	TSharedPtr<STextBlock> SaveSessionAsWarningIcon;
};


USTRUCT()
struct FCreateSessionOptions
{
public:

	GENERATED_BODY()

	/**
	 * The server on which the session will be created.
	 */
	UPROPERTY(VisibleAnywhere, Category="Server")
	FString ServerName;

	/**
	 * The name of the session.
	 */
	UPROPERTY(EditAnywhere, Category="Session Settings")
	FString SessionName;


	UPROPERTY(EditAnywhere, Category = "Session Data Management")
	bool bSessionToRestoreEnabled = false;

	/**
	 * Select a saved session to restore its content.
	 */
	UPROPERTY(EditAnywhere, Category="Session Data Management", meta = (EditCondition = "bSessionToRestoreEnabled"))
	FString SessionToRestore;


	UPROPERTY(EditAnywhere, Category = "Session Data Management")
	bool bSaveSessionAsEnabled = false;

	/**
	 * Enter a name for the save and the session will be saved before it's deleted/closed.
	 */
	UPROPERTY(EditAnywhere, Category = "Session Data Management", meta = (EditCondition = "bSaveSessionAsEnabled"))
	FString SaveSessionAs;

	// Contains the list of session data that the user can select via the details panel
	UPROPERTY(EditAnywhere, Category = "Session Data Management")
	TArray<FString> SessionToRestoreOptions;
};
