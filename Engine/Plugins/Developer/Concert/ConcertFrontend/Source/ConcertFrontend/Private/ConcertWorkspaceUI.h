// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"

struct FAssetData;
class SWidget;
class FExtender;
class FMenuBuilder;
struct FConcertPersistCommand;
class IConcertClientWorkspace;
class SDockTab;

/**
 * Concert Client Workspace UI
 */
class FConcertWorkspaceUI : public TSharedFromThis<FConcertWorkspaceUI>
{
public:
	FConcertWorkspaceUI();
	~FConcertWorkspaceUI();

	/** Install UI extensions for the workspace UI. */
	void InstallWorkspaceExtensions(TWeakPtr<IConcertClientWorkspace> InClientWorkspace);

	/** Uninstall UI extensions for the workspace UI. */
	void UninstallWorspaceExtensions();

	/** Prompt User on which workspace file changes should be persisted and prepared for source control submission. */
	void PromptPersistSessionChanges();

private:
	friend class SConcertWorkspaceStateIndicators;
	friend class SConcertWorkspaceStateTooltip;

	bool SubmitChangelist(const FConcertPersistCommand& PersistCommand, FText& OperationMessage);

	FText GetSessionClientUser(const FGuid& ClientId) const;

	FGuid GetWorkspaceLockId() const;
	FGuid GetResourceLockId(const FName InResourceName) const;

	bool CanLockResources(TArray<FName> InResourceNames) const;
	bool CanUnlockResources(TArray<FName> InResourceNames) const;

	void ExecuteLockResources(TArray<FName> InResourceNames);
	void ExecuteUnlockResources(TArray<FName> InResourceNames);
	void ExecuteViewHistory(TArray<FName> InResourceNames);

	/** Delegate to extend the content browser asset context menu. */
	TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);

	/** Called to generate concert asset context menu. */
	void GenerateConcertAssetContextMenu(FMenuBuilder& MenuBuilder, TArray<FName> AssetObjectPaths);

	/** Delegate to extend the source control menu. */
	TSharedRef<class FExtender> OnExtendLevelEditorSourceControlMenu(const TSharedRef<class FUICommandList>);

	/** Delegate to generate extra state indicator on content browser assets. */
	TSharedRef<SWidget> OnGenerateAssetViewExtraStateIcon(const FAssetData& AssetData);

	/** Delegate to generate extra state tooltip on content browser assets. */
	TSharedRef<SWidget> OnGenerateAssetViewExtraStateTooltip(const FAssetData& AssetData);

	/** Create an asset history tab filtered with a resource name. */
	TSharedRef<SDockTab> CreateHistoryTab(const FName& ResourceName) const;

	/** Workspace this is a view of. */
	TWeakPtr<IConcertClientWorkspace> ClientWorkspace;

	/** Delegate handle for context menu extension. */
	FDelegateHandle ContentBrowserAssetExtenderDelegateHandle;

	/** Delegate handle for asset state indicator icon extension. */
	FDelegateHandle ContentBrowserAssetStateIconDelegateHandle;

	/** Delegate handle for asset state indicator tooltip extension. */
	FDelegateHandle ContentBrowserAssetStateTooltipDelegateHandle;

	/** Delegate handle for source control menu extension. */
	FDelegateHandle SourceControlExtensionDelegateHandle;

	TSharedPtr<FTabManager::FLayout> AssetHistoryLayout;
};