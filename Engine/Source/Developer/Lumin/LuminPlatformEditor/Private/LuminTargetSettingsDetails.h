// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PropertyHandle.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "Engine/EngineTypes.h"
#include "Framework/SlateDelegates.h"
#include "Delegates/Delegate.h"
#include "Widgets/Input/SButton.h"

#include "TargetPlatformAudioCustomization.h"

/**
* Detail customization for PS4 target settings panel
*/
class FLuminTargetSettingsDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	// These are hard-wired defaults for the engine tree default icons.
	const FString DefaultIconModelPath;
	const FString DefaultIconPortalPath;

	const FString GameLuminPath;
	const FString GameProjectSetupPath;

	IDetailLayoutBuilder* SavedLayoutBuilder;

	FLuminTargetSettingsDetails();

	FUNC_DECLARE_DELEGATE(FOnPickPath, FReply, const FString&);
	FUNC_DECLARE_DELEGATE(FOnChoosePath, FReply, TAttribute<FString>, const FOnPickPath&, TSharedPtr<SButton>);
	void BuildPathPicker(IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& Category,
		TAttribute<FString> DirPath, const FText& Label, const FText& Tooltip,
		const FOnChoosePath& OnChoose, const FOnPickPath& OnPick);
	FReply OnPickDirectory(TAttribute<FString> DirPath,
		const FOnPickPath& OnPick, TSharedPtr<SButton> PickButton);
	FReply OnPickFile(TAttribute<FString> FilePath,
		const FOnPickPath& OnPick, TSharedPtr<SButton> PickButton,
		const FString & Title, const FString & Filter);

	// Setup files.

	TAttribute<bool> SetupForPlatformAttribute;

	TSharedPtr<IPropertyHandle> IconModelPathProp;
	TSharedPtr<IPropertyHandle> IconPortalPathProp;
	TSharedPtr<IPropertyHandle> CertificateProp;

	TAttribute<FString> IconModelPathAttribute;
	TAttribute<FString> IconPortalPathAttribute;
	TAttribute<FString> CertificateAttribute;

	FString IconModelPathGetter();
	FString IconPortalPathGetter();
	FString CertificateGetter();

	void BuildAppTileSection(IDetailLayoutBuilder& DetailBuilder);
	void CopySetupFilesIntoProject();
	FReply OpenBuildFolder();
	FReply OnPickIconModelPath(const FString& DirPath);
	FReply OnPickIconPortalPath(const FString& DirPath);
	bool CopyDir(FString SourceDir, FString TargetDir);
	FReply OnPickCertificate(const FString& CertificatePath);

	// Audio section.

	FAudioPluginWidgetManager AudioPluginManager;

	void BuildAudioSection(IDetailLayoutBuilder& DetailBuilder);
};