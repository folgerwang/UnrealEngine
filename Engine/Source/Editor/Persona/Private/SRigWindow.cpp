// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "SRigWindow.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "ReferenceSkeleton.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SButton.h"
#include "AssetNotifications.h"
#include "Animation/Rig.h"
#include "BoneSelectionWidget.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "SRigPicker.h"
#include "BoneMappingHelper.h"
#include "SSkeletonWidget.h"
#include "IEditableSkeleton.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Framework/Application/SlateApplication.h"

class FPersona;

#define LOCTEXT_NAMESPACE "SRigWindow"

DECLARE_DELEGATE_TwoParams(FOnBoneMappingChanged, FName /** NodeName */, FName /** BoneName **/);
DECLARE_DELEGATE_RetVal_OneParam(FName, FOnGetBoneMapping, FName /** Node Name **/);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// SRigWindow

void SRigWindow::Construct(const FArguments& InArgs, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo)
{
	EditableSkeletonPtr = InEditableSkeleton;
	PreviewScenePtr = InPreviewScene;
	bDisplayAdvanced = false;

	InEditableSkeleton->RefreshRigConfig();

	ChildSlot
	[
		SNew( SVerticalBox )

		// first add rig asset picker
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2,2)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RigNameLabel", "Select Rig "))
				.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.BoldFont"))
			]

			+SHorizontalBox::Slot()
			[
				SAssignNew( AssetComboButton, SComboButton )
				//.ToolTipText( this, &SPropertyEditorAsset::OnGetToolTip )
				.ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
				.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent( this, &SRigWindow::MakeRigPickerWithMenu )
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					// Show the name of the asset or actor
					SNew(STextBlock)
					.TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
					.Font( FEditorStyle::GetFontStyle( "PropertyWindow.NormalFont" ) )
					.Text(this,&SRigWindow::GetAssetName)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(2, 5)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.Padding(2, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRigWindow::OnAutoMapping))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("AutoMapping_Title", "AutoMap"))
				.ToolTipText(LOCTEXT("AutoMapping_Tooltip", "Automatically map the best matching bones"))
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.Padding(2, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRigWindow::OnClearMapping))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("ClearMapping_Title", "Clear"))
				.ToolTipText(LOCTEXT("ClearMapping_Tooltip", "Clear currently mapping bones"))
			]

// 			+ SHorizontalBox::Slot()
// 			.HAlign(HAlign_Right)
// 			.Padding(5, 0)
// 			[
// 				SNew(SButton)
// 				.OnClicked(FOnClicked::CreateSP(this, &SRigWindow::OnToggleView))
// 				.HAlign(HAlign_Center)
// 				.VAlign(VAlign_Center)
// 				.Text(LOCTEXT("HierarchyView_Title", "Show Hierarchy"))
// 				.ToolTipText(LOCTEXT("HierarchyView_Tooltip", "Show Hierarchy View"))
// 			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.Padding(2, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRigWindow::OnSaveMapping))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("SaveMapping_Title", "Save"))
				.ToolTipText(LOCTEXT("SaveMapping_Tooltip", "Save currently mapping bones"))
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.Padding(2, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRigWindow::OnLoadMapping))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("LoadMapping_Title", "Load"))
				.ToolTipText(LOCTEXT("LoadMapping_Tooltip", "Load mapping from saved asset."))
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.Padding(2, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRigWindow::OnToggleAdvanced))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(this, &SRigWindow::GetAdvancedButtonText)
				.ToolTipText(LOCTEXT("ToggleAdvanced_Tooltip", "Toggle Base/Advanced configuration"))
			]
		]

		// now show bone mapping
		+ SVerticalBox::Slot()
		.FillHeight(1)
		.Padding(0,2)
		[
			SAssignNew(BoneMappingWidget, SBoneMappingBase, InOnPostUndo)
			.OnBoneMappingChanged(this, &SRigWindow::OnBoneMappingChanged)
			.OnGetBoneMapping(this, &SRigWindow::GetBoneMapping)
			.OnCreateBoneMapping(this, &SRigWindow::CreateBoneMappingList)
			.OnGetReferenceSkeleton(this, &SRigWindow::GetReferenceSkeleton)
		]
	];
}

void SRigWindow::CreateBoneMappingList( const FString& SearchText, TArray< TSharedPtr<FDisplayedBoneMappingInfo> >& BoneMappingList)
{
	BoneMappingList.Empty();

	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();
	const URig* Rig = Skeleton.GetRig();

	if ( Rig )
	{
		bool bDoFiltering = !SearchText.IsEmpty();
		const TArray<FNode>& Nodes = Rig->GetNodes();

		for ( const auto Node : Nodes )
		{
			const FName& Name = Node.Name;
			const FString& DisplayName = Node.DisplayName;
			const FName& BoneName = Skeleton.GetRigBoneMapping(Name);

			if (Node.bAdvanced == bDisplayAdvanced)
			{
				if(bDoFiltering)
				{
					// make sure it doens't fit any of them
					if(!Name.ToString().Contains(SearchText) && !DisplayName.Contains(SearchText) && !BoneName.ToString().Contains(SearchText))
					{
						continue; // Skip items that don't match our filter
					}
				}

				TSharedRef<FDisplayedBoneMappingInfo> Info = FDisplayedBoneMappingInfo::Make(Name, DisplayName);

				BoneMappingList.Add(Info);
			}
		}
	}
}


void SRigWindow::OnAssetSelected(UObject* Object)
{
	AssetComboButton->SetIsOpen(false);

	EditableSkeletonPtr.Pin()->SetRigConfig(Cast<URig>(Object));

	BoneMappingWidget.Get()->RefreshBoneMappingList();

	FAssetNotifications::SkeletonNeedsToBeSaved(&EditableSkeletonPtr.Pin()->GetSkeleton());
}

/** Returns true if the asset shouldn't show  */
bool SRigWindow::ShouldFilterAsset(const struct FAssetData& AssetData)
{
	return (AssetData.GetAsset() == GetRigObject());
}

URig* SRigWindow::GetRigObject() const
{
	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();
	return Skeleton.GetRig();
}

void SRigWindow::OnBoneMappingChanged(FName NodeName, FName BoneName)
{
	EditableSkeletonPtr.Pin()->SetRigBoneMapping(NodeName, BoneName);
}

FName SRigWindow::GetBoneMapping(FName NodeName)
{
	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();
	return Skeleton.GetRigBoneMapping(NodeName);
}

FReply SRigWindow::OnToggleAdvanced()
{
	bDisplayAdvanced = !bDisplayAdvanced;

	BoneMappingWidget.Get()->RefreshBoneMappingList();

	return FReply::Handled();
}

FText SRigWindow::GetAdvancedButtonText() const
{
	if (bDisplayAdvanced)
	{
		return LOCTEXT("ShowBase", "Show Base");
	}

	return LOCTEXT("ShowAdvanced", "Show Advanced");
}

TSharedRef<SWidget> SRigWindow::MakeRigPickerWithMenu()
{
	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();

	// rig asset picker
	return	
		SNew(SRigPicker)
		.InitialObject(Skeleton.GetRig())
		.OnShouldFilterAsset(this, &SRigWindow::ShouldFilterAsset)
		.OnSetReference(this, &SRigWindow::OnAssetSelected)
		.OnClose(this, &SRigWindow::CloseComboButton );
}

void SRigWindow::CloseComboButton()
{
	AssetComboButton->SetIsOpen(false);
}

FText SRigWindow::GetAssetName() const
{
	URig* Rig = GetRigObject();
	if (Rig)
	{
		return FText::FromString(Rig->GetName());
	}

	return LOCTEXT("None", "None");
}

const struct FReferenceSkeleton& SRigWindow::GetReferenceSkeleton() const
{
	// have to change this to preview mesh because that's what the retarget base pose will be
	UDebugSkelMeshComponent* PreviewMeshComp = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
	USkeletalMesh* PreviewMesh = (PreviewMeshComp) ? PreviewMeshComp->SkeletalMesh : nullptr;
	// it's because retarget base pose leaves in mesh, so if you give ref skeleton of skeleton, you might have joint that your mesh doesn't have
	return (PreviewMesh)? PreviewMesh->RefSkeleton : EditableSkeletonPtr.Pin()->GetSkeleton().GetReferenceSkeleton();
}

bool SRigWindow::OnTargetSkeletonSelected(USkeleton* SelectedSkeleton, URig*  Rig) const
{
	if (SelectedSkeleton)
	{
		// make sure the skeleton contains all the rig node names
		const FReferenceSkeleton& RefSkeleton = SelectedSkeleton->GetReferenceSkeleton();

		if (RefSkeleton.GetNum() > 0)
		{
			const TArray<FNode> RigNodes = Rig->GetNodes();
			int32 BoneMatched = 0;

			for (const auto& RigNode : RigNodes)
			{
				if (RefSkeleton.FindBoneIndex(RigNode.Name) != INDEX_NONE)
				{
					++BoneMatched;
				}
			}

			float BoneMatchedPercentage = (float)(BoneMatched) / RefSkeleton.GetNum();
			if (BoneMatchedPercentage > 0.5f)
			{
				Rig->SetSourceReferenceSkeleton(RefSkeleton);

				return true;
			}
		}
	}

	return false;
}

bool SRigWindow::SelectSourceReferenceSkeleton(URig* Rig) const
{
	TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
		.Title(LOCTEXT("SelectSourceSkeletonForRig", "Select Source Skeleton for the Rig"))
		.ClientSize(FVector2D(500, 600));

	TSharedRef<SSkeletonSelectorWindow> SkeletonSelectorWindow = SNew(SSkeletonSelectorWindow).WidgetWindow(WidgetWindow);

	WidgetWindow->SetContent(SkeletonSelectorWindow);

	GEditor->EditorAddModalWindow(WidgetWindow);
	USkeleton* RigSkeleton = SkeletonSelectorWindow->GetSelectedSkeleton();
	if (RigSkeleton)
	{
		return OnTargetSkeletonSelected(RigSkeleton, Rig);
	}

	return false;
}


FReply SRigWindow::OnAutoMapping()
{
	URig* Rig = GetRigObject();
	if (Rig)
	{
		if (!Rig->IsSourceReferenceSkeletonAvailable())
		{
			//ask if they want to set up source skeleton
			EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("TheRigNeedsSkeleton", 
				"In order to attempt to auto-map bones, the rig should have the source skeleton. However, the current rig is missing the source skeleton. Would you like to choose one? It's best to select the skeleton this rig is from."));

			if (Response == EAppReturnType::No)
			{
				return FReply::Handled();
			}

			if (!SelectSourceReferenceSkeleton(Rig))
			{
				return FReply::Handled();
			}
		}

		FReferenceSkeleton RigReferenceSkeleton = Rig->GetSourceReferenceSkeleton();
		const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();
		FBoneMappingHelper Helper(RigReferenceSkeleton, Skeleton.GetReferenceSkeleton());
		TMap<FName, FName> BestMatches;
		Helper.TryMatch(BestMatches);

		EditableSkeletonPtr.Pin()->SetRigBoneMappings(BestMatches);
		// refresh the list
		BoneMappingWidget->RefreshBoneMappingList();
	}

	return FReply::Handled();
}

FReply SRigWindow::OnClearMapping()
{
	URig* Rig = GetRigObject();
	if (Rig)
	{
		const TArray<FNode>& Nodes = Rig->GetNodes();
		TMap<FName, FName> Mappings;
		for (const auto& Node : Nodes)
		{
			Mappings.Add(Node.Name, NAME_None);
		}

		EditableSkeletonPtr.Pin()->SetRigBoneMappings(Mappings);

		// refresh the list
		BoneMappingWidget->RefreshBoneMappingList();
	}
	return FReply::Handled();
}

// save mapping function
FReply SRigWindow::OnSaveMapping()
{
	URig* Rig = GetRigObject();
	if (Rig)
	{
		const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();
		const FString DefaultPackageName = Skeleton.GetPathName();
		const FString DefaultPath = FPackageName::GetLongPackagePath(DefaultPackageName);
		const FString DefaultName = TEXT("BoneMapping");

		// Initialize SaveAssetDialog config
		FSaveAssetDialogConfig SaveAssetDialogConfig;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveMappingToAsset", "Save Mapping");
		SaveAssetDialogConfig.DefaultPath = DefaultPath;
		SaveAssetDialogConfig.DefaultAssetName = DefaultName;
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.AssetClassNames.Add(UNodeMappingContainer::StaticClass()->GetFName());

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
		if (!SaveObjectPath.IsEmpty())
		{
			const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
			const FString SavePackagePath = FPaths::GetPath(SavePackageName);
			const FString SaveAssetName = FPaths::GetBaseFilename(SavePackageName);
			
			// create package and create object
			UPackage* Package = CreatePackage(nullptr, *SavePackageName);
			UNodeMappingContainer* MapperClass = NewObject<UNodeMappingContainer>(Package, *SaveAssetName, RF_Public | RF_Standalone);
			USkeletalMeshComponent* PreviewMeshComp = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
			USkeletalMesh* PreviewMesh = PreviewMeshComp->SkeletalMesh;
			if (MapperClass && PreviewMesh)
			{
				// update mapping information on the class
				MapperClass->SetSourceAsset(Rig);
				MapperClass->SetTargetAsset(PreviewMesh);

				const TArray<FNode>& Nodes = Rig->GetNodes();
				for (const auto& Node : Nodes)
				{
					FName MappingName = Skeleton.GetRigBoneMapping(Node.Name);
					if (Node.Name != NAME_None && MappingName != NAME_None)
					{
						MapperClass->AddMapping(Node.Name, MappingName);
					}
				}

				// save mapper class
				FString const PackageName = Package->GetName();
				FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

				UPackage::SavePackage(Package, NULL, RF_Standalone, *PackageFileName, GError, nullptr, false, true, SAVE_NoError);
			}
		}
	}
	return FReply::Handled(); 
}

FReply SRigWindow::OnLoadMapping()
{
	// show list of skeletalmeshes that they can choose from
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(UNodeMappingContainer::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SRigWindow::SetSelectedMappingAsset);
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Tile;

	TSharedRef<SWidget> Widget = SNew(SBox)
		.WidthOverride(384)
		.HeightOverride(768)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.25f, 0.25f, 0.25f, 1.f))
			.Padding(2)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(8)
				[
					ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
				]
			]
		];

	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		Widget,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TopMenu)
	);

	return FReply::Handled();
}

FReply SRigWindow::OnToggleView()
{
	return FReply::Handled();
}

void SRigWindow::SetSelectedMappingAsset(const FAssetData& InAssetData)
{
	UNodeMappingContainer* Container = Cast<UNodeMappingContainer>(InAssetData.GetAsset());
	if (Container)
	{
		const TMap<FName, FName> SourceToTarget = Container->GetNodeMappingTable();
		EditableSkeletonPtr.Pin()->SetRigBoneMappings(SourceToTarget);
	}

	FSlateApplication::Get().DismissAllMenus();
}

#undef LOCTEXT_NAMESPACE

