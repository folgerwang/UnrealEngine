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
struct FConcertClientInfo;

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
	friend class SConcertWorkspaceLockStateIndicator;
	friend class SConcertWorkspaceLockStateTooltip;
	friend class SConcertWorkspaceModifiedByOtherIndicator;
	friend class SConcertWorkspaceModifiedByOtherTooltip;

	/** Check out and optionally submit files to source control. */
	bool SubmitChangelist(const FConcertPersistCommand& PersistCommand, FText& OperationMessage);

	/** Get a text description for the specified client that can be displayed in UI. */
	FText GetUserDescriptionText(const FGuid& ClientId) const;
	FText GetUserDescriptionText(const FConcertClientInfo& ClientInfo) const;

	/** Get the local workspace's lock id. */
	FGuid GetWorkspaceLockId() const;

	/** Get the id of the client who owns the lock on a given resource. */
	FGuid GetResourceLockId(const FName InResourceName) const;
	
	/**
	 * @return whether a list of resources can be locked.
	 */
	bool CanLockResources(TArray<FName> InResourceNames) const;

	/**
	 * @return whether a list of resources can be unlocked.
	 */
	bool CanUnlockResources(TArray<FName> InResourceNames) const;

	/** Lock a list of resources. */
	void ExecuteLockResources(TArray<FName> InResourceNames);

	/**
	 * Unlock a list of resources.
	 */
	void ExecuteUnlockResources(TArray<FName> InResourceNames);

	/**
	 * View the history of the specified recourses.
	 */
	void ExecuteViewHistory(TArray<FName> InResourceNames);

	/**
	 * Returns true if the specified asset was modified by another user than the one associated to this workspace and optionally return the information about the last client who modified the resource.
	 * @param[in] AssetName The name of the asset to look up.
	 * @param[out] OutOtherClientsWithModifNum If not null, will contain how many other client(s) have modified the specified asset.
	 * @param[out] OutOtherClientsWithModifInfo If not null, will contain the other client who modified the asset, up to OtherClientsWithModifMaxFetchNum.
	 * @param[in] OtherClientsWithModifMaxFetchNum The maximum number of client info to store in OutOtherClientsWithModifInfo if the latter is not null.
	 */
	bool IsAssetModifiedByOtherClients(const FName& AssetName, int* OutOtherClientsWithModifNum = nullptr, TArray<FConcertClientInfo>* OutOtherClientsWithModifInfo = nullptr, int OtherClientsWithModifMaxFetchNum = 0) const;

	/** Delegate to extend the content browser asset context menu. */
	TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);

	/** Called to generate concert asset context menu. */
	void GenerateConcertAssetContextMenu(FMenuBuilder& MenuBuilder, TArray<FName> AssetObjectPaths);

	/** Delegate to extend the source control menu. */
	TSharedRef<class FExtender> OnExtendLevelEditorSourceControlMenu(const TSharedRef<class FUICommandList>);

	/** Delegate to generate an extra lock state indicator on content browser assets. */
	TSharedRef<SWidget> OnGenerateAssetViewLockStateIcons(const FAssetData& AssetData);

	/** Delegate to generate extra lock state tooltip on content browser assets. */
	TSharedRef<SWidget> OnGenerateAssetViewLockStateTooltip(const FAssetData& AssetData);

	/** Delegate to generate an extra "modified by other" icon on content browser assets. */
	TSharedRef<SWidget> OnGenerateAssetViewModifiedByOtherIcon(const FAssetData& AssetData);

	/** Delegate to generate the "modified by..." tooltip  on content browser assets. */
	TSharedRef<SWidget> OnGenerateAssetViewModifiedByOtherTooltip(const FAssetData& AssetData);

	/** Create an asset history tab filtered with a resource name. */
	TSharedRef<SDockTab> CreateHistoryTab(const FName& ResourceName) const;

	/** Workspace this is a view of. */
	TWeakPtr<IConcertClientWorkspace> ClientWorkspace;

	/** Delegate handle for context menu extension. */
	FDelegateHandle ContentBrowserAssetExtenderDelegateHandle;

	/** Delegate handle for asset lock state indicator icon extension. */
	FDelegateHandle ContentBrowserAssetLockStateIconDelegateHandle;

	/** Delegate handle for asset lock state indicator tooltip extension. */
	FDelegateHandle ContentBrowserAssetLockStateTooltipDelegateHandle;

	/** Delegate handle for asset modified by another client icon extension. */
	FDelegateHandle ContentBrowserAssetModifiedByOtherIconDelegateHandle;

	/** Delegate handle for asset modified by another client tooltip extension. */
	FDelegateHandle ContentBrowserAssetModifiedByOtherTooltipDelegateHandle;

	/** Delegate handle for source control menu extension. */
	FDelegateHandle SourceControlExtensionDelegateHandle;

	TSharedPtr<FTabManager::FLayout> AssetHistoryLayout;
};