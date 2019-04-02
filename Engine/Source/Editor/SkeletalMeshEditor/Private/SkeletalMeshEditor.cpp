// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshEditor.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorReimportHandler.h"
#include "AssetData.h"
#include "EdGraph/EdGraphSchema.h"
#include "Editor/EditorEngine.h"
#include "EngineGlobals.h"
#include "ISkeletalMeshEditorModule.h"
#include "IPersonaToolkit.h"
#include "PersonaModule.h"
#include "SkeletalMeshEditorMode.h"
#include "IPersonaPreviewScene.h"
#include "SkeletalMeshEditorCommands.h"
#include "IDetailsView.h"
#include "ISkeletonTree.h"
#include "ISkeletonEditorModule.h"
#include "IAssetFamily.h"
#include "PersonaCommonCommands.h"
#include "EngineUtils.h"
#include "Rendering/SkeletalMeshModel.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "Assets/ClothingAsset.h"
#include "SCreateClothingSettingsPanel.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "Preferences/PersonaOptions.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorViewportClient.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Algo/Transform.h"
#include "ISkeletonTreeItem.h"
#include "FbxMeshUtils.h"
#include "LODUtilities.h"

#include "LODUtilities.h"
#include "ScopedTransaction.h"
#include "ComponentReregisterContext.h"
#include "EditorFramework/AssetImportData.h"
#include "Factories/FbxSkeletalMeshImportData.h"

const FName SkeletalMeshEditorAppIdentifier = FName(TEXT("SkeletalMeshEditorApp"));

const FName SkeletalMeshEditorModes::SkeletalMeshEditorMode(TEXT("SkeletalMeshEditorMode"));

const FName SkeletalMeshEditorTabs::DetailsTab(TEXT("DetailsTab"));
const FName SkeletalMeshEditorTabs::SkeletonTreeTab(TEXT("SkeletonTreeView"));
const FName SkeletalMeshEditorTabs::AssetDetailsTab(TEXT("AnimAssetPropertiesTab"));
const FName SkeletalMeshEditorTabs::ViewportTab(TEXT("Viewport"));
const FName SkeletalMeshEditorTabs::AdvancedPreviewTab(TEXT("AdvancedPreviewTab"));
const FName SkeletalMeshEditorTabs::MorphTargetsTab("MorphTargetsTab");
const FName SkeletalMeshEditorTabs::AnimationMappingTab("AnimationMappingWindow");

DEFINE_LOG_CATEGORY(LogSkeletalMeshEditor);

#define LOCTEXT_NAMESPACE "SkeletalMeshEditor"

FSkeletalMeshEditor::FSkeletalMeshEditor()
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}
}

FSkeletalMeshEditor::~FSkeletalMeshEditor()
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->UnregisterForUndo(this);
	}
}

void FSkeletalMeshEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_SkeletalMeshEditor", "Skeletal Mesh Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FSkeletalMeshEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FSkeletalMeshEditor::InitSkeletalMeshEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, USkeletalMesh* InSkeletalMesh)
{
	SkeletalMesh = InSkeletalMesh;

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(InSkeletalMesh);

	PersonaToolkit->GetPreviewScene()->SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode::ReferencePose);

	TSharedRef<IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(InSkeletalMesh);
	AssetFamily->RecordAssetOpened(FAssetData(InSkeletalMesh));

	TSharedPtr<IPersonaPreviewScene> PreviewScene = PersonaToolkit->GetPreviewScene();

	FSkeletonTreeArgs SkeletonTreeArgs;
	SkeletonTreeArgs.OnSelectionChanged = FOnSkeletonTreeSelectionChanged::CreateSP(this, &FSkeletalMeshEditor::HandleSelectionChanged);
	SkeletonTreeArgs.PreviewScene = PreviewScene;
	SkeletonTreeArgs.ContextName = GetToolkitFName();

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::GetModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	SkeletonTree = SkeletonEditorModule.CreateSkeletonTree(PersonaToolkit->GetSkeleton(), SkeletonTreeArgs);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, SkeletalMeshEditorAppIdentifier, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InSkeletalMesh);

	BindCommands();

	AddApplicationMode(
		SkeletalMeshEditorModes::SkeletalMeshEditorMode,
		MakeShareable(new FSkeletalMeshEditorMode(SharedThis(this), SkeletonTree.ToSharedRef())));

	SetCurrentMode(SkeletalMeshEditorModes::SkeletalMeshEditorMode);

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// Set up mesh click selection
	PreviewScene->RegisterOnMeshClick(FOnMeshClick::CreateSP(this, &FSkeletalMeshEditor::HandleMeshClick));
	PreviewScene->SetAllowMeshHitProxies(GetDefault<UPersonaOptions>()->bAllowMeshSectionSelection);
}

FName FSkeletalMeshEditor::GetToolkitFName() const
{
	return FName("SkeletalMeshEditor");
}

FText FSkeletalMeshEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "SkeletalMeshEditor");
}

FString FSkeletalMeshEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "SkeletalMeshEditor ").ToString();
}

FLinearColor FSkeletalMeshEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void FSkeletalMeshEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SkeletalMesh);
}

void FSkeletalMeshEditor::BindCommands()
{
	FSkeletalMeshEditorCommands::Register();

	ToolkitCommands->MapAction(FSkeletalMeshEditorCommands::Get().ReimportMesh,
		FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::HandleReimportMesh, (int32)INDEX_NONE));

	ToolkitCommands->MapAction(FSkeletalMeshEditorCommands::Get().ReimportAllMesh,
		FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::HandleReimportAllMesh, (int32)INDEX_NONE));

	ToolkitCommands->MapAction(FSkeletalMeshEditorCommands::Get().MeshSectionSelection,
		FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::ToggleMeshSectionSelection),
		FCanExecuteAction(), 
		FIsActionChecked::CreateSP(this, &FSkeletalMeshEditor::IsMeshSectionSelectionChecked));

	ToolkitCommands->MapAction(FPersonaCommonCommands::Get().TogglePlay,
		FExecuteAction::CreateRaw(&GetPersonaToolkit()->GetPreviewScene().Get(), &IPersonaPreviewScene::TogglePlayback));
}

void FSkeletalMeshEditor::ExtendToolbar()
{
	// If the ToolbarExtender is valid, remove it before rebuilding it
	if (ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	auto ConstructReimportContextMenu = [this]()
	{
		bool bShowSubMenu = SkeletalMesh != nullptr && SkeletalMesh->AssetImportData != nullptr && SkeletalMesh->AssetImportData->GetSourceFileCount() > 1;
		FMenuBuilder MenuBuilder(true, nullptr);
		
		if (!bShowSubMenu)
		{
			//Reimport
			MenuBuilder.AddMenuEntry(FSkeletalMeshEditorCommands::Get().ReimportMesh->GetLabel(),
				FSkeletalMeshEditorCommands::Get().ReimportMesh->GetDescription(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::HandleReimportMesh, 0)));
			
			MenuBuilder.AddMenuEntry(FSkeletalMeshEditorCommands::Get().ReimportMeshWithNewFile->GetLabel(),
				FSkeletalMeshEditorCommands::Get().ReimportMeshWithNewFile->GetDescription(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::HandleReimportMeshWithNewFile, 0)));

			//Reimport ALL
			MenuBuilder.AddMenuEntry(FSkeletalMeshEditorCommands::Get().ReimportAllMesh->GetLabel(),
				FSkeletalMeshEditorCommands::Get().ReimportAllMesh->GetDescription(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::HandleReimportAllMesh, 0)));

			MenuBuilder.AddMenuEntry(FSkeletalMeshEditorCommands::Get().ReimportAllMeshWithNewFile->GetLabel(),
				FSkeletalMeshEditorCommands::Get().ReimportAllMeshWithNewFile->GetDescription(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::HandleReimportAllMeshWithNewFile, 0)));

			FText ReimportMultiSources = LOCTEXT("ReimportMultiSources", "Reimport Content");
			FText ReimportMultiSourcesTooltip = LOCTEXT("ReimportMultiSourcesTooltip", "Reimport Geometry or Skinning Weights content, this will create multi import source file.");

			auto CreateMultiContentSubMenu = [this](FMenuBuilder& SubMenuBuilder)
			{
				auto UpdateImportDataContentType = [this](int32 SourceFileIndex)
				{
					UFbxSkeletalMeshImportData* SkeletalMeshImportData = Cast<UFbxSkeletalMeshImportData>(SkeletalMesh->AssetImportData);
					if (SkeletalMeshImportData)
					{
						SkeletalMeshImportData->ImportContentType = SourceFileIndex == 0 ? EFBXImportContentType::FBXICT_All : SourceFileIndex == 1 ? EFBXImportContentType::FBXICT_Geometry : EFBXImportContentType::FBXICT_SkinningWeights;
						HandleReimportMeshWithNewFile(SourceFileIndex);
					}
				};

				FText ReimportGeometryContentLabel = LOCTEXT("ReimportGeometryContentLabel", "Geometry");
				FText ReimportGeometryContentLabelTooltip = LOCTEXT("ReimportGeometryContentLabelTooltipTooltip", "Reimport Geometry Only");
				SubMenuBuilder.AddMenuEntry(
					ReimportGeometryContentLabel,
					ReimportGeometryContentLabelTooltip,
					FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
					FUIAction(
						FExecuteAction::CreateLambda(UpdateImportDataContentType, 1),
						FCanExecuteAction()
					)
				);
				FText ReimportSkinningAndWeightsContentLabel = LOCTEXT("ReimportSkinningAndWeightsContentLabel", "Skinning And Weights");
				FText ReimportSkinningAndWeightsContentLabelTooltip = LOCTEXT("ReimportSkinningAndWeightsContentLabelTooltipTooltip", "Reimport Skinning And Weights Only");
				SubMenuBuilder.AddMenuEntry(
					ReimportSkinningAndWeightsContentLabel,
					ReimportSkinningAndWeightsContentLabelTooltip,
					FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
					FUIAction(
						FExecuteAction::CreateLambda(UpdateImportDataContentType, 2),
						FCanExecuteAction()
					)
				);
			};

			MenuBuilder.AddSubMenu(
				ReimportMultiSources,
				ReimportMultiSourcesTooltip,
				FNewMenuDelegate::CreateLambda(CreateMultiContentSubMenu));
		}
		else
		{
			auto CreateSubMenu = [this](FMenuBuilder& SubMenuBuilder, bool bReimportAll, bool bWithNewFile)
			{
				//Get the data, we cannot use the closure since the lambda will be call when the function scope will be gone
				TArray<FString> SourceFilePaths;
				SkeletalMesh->AssetImportData->ExtractFilenames(SourceFilePaths);
				TArray<FString> SourceFileLabels;
				SkeletalMesh->AssetImportData->ExtractDisplayLabels(SourceFileLabels);

				if (SourceFileLabels.Num() > 0 && SourceFileLabels.Num() == SourceFilePaths.Num())
				{
					auto UpdateImportDataContentType = [this](int32 SourceFileIndex, bool bReimportAll, bool bWithNewFile)
					{
						UFbxSkeletalMeshImportData* SkeletalMeshImportData = Cast<UFbxSkeletalMeshImportData>(SkeletalMesh->AssetImportData);
						if (SkeletalMeshImportData)
						{
							SkeletalMeshImportData->ImportContentType = SourceFileIndex == 0 ? EFBXImportContentType::FBXICT_All : SourceFileIndex == 1 ? EFBXImportContentType::FBXICT_Geometry : EFBXImportContentType::FBXICT_SkinningWeights;
							if (bReimportAll)
							{
								if (bWithNewFile)
								{
									HandleReimportAllMeshWithNewFile(SourceFileIndex);
								}
								else
								{
									HandleReimportAllMesh(SourceFileIndex);
								}
							}
							else
							{
								if (bWithNewFile)
								{
									HandleReimportMeshWithNewFile(SourceFileIndex);
								}
								else
								{
									HandleReimportMesh(SourceFileIndex);
								}
							}
						}
					};

					for (int32 SourceFileIndex = 0; SourceFileIndex < SourceFileLabels.Num(); ++SourceFileIndex)
					{
						FText ReimportLabel = FText::Format(LOCTEXT("ReimportNoLabel", "SourceFile {0}"), SourceFileIndex);
						FText ReimportLabelTooltip = FText::Format(LOCTEXT("ReimportNoLabelTooltip", "Reimport File: {0}"), FText::FromString(SourceFilePaths[SourceFileIndex]));
						if (SourceFileLabels[SourceFileIndex].Len() > 0)
						{
							ReimportLabel = FText::Format(LOCTEXT("ReimportLabel", "{0}"), FText::FromString(SourceFileLabels[SourceFileIndex]));
							ReimportLabelTooltip = FText::Format(LOCTEXT("ReimportLabelTooltip", "Reimport {0} File: {1}"), FText::FromString(SourceFileLabels[SourceFileIndex]), FText::FromString(SourceFilePaths[SourceFileIndex]));
						}
						SubMenuBuilder.AddMenuEntry(
							ReimportLabel,
							ReimportLabelTooltip,
							FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
							FUIAction(
								FExecuteAction::CreateLambda(UpdateImportDataContentType, SourceFileIndex, bReimportAll, bWithNewFile),
								FCanExecuteAction()
							)
						);
					}
				}
			};

			//Create 4 submenu: Reimport, ReimportWithNewFile, ReimportAll and ReimportAllWithNewFile
			MenuBuilder.AddSubMenu(
				FSkeletalMeshEditorCommands::Get().ReimportMesh->GetLabel(),
				FSkeletalMeshEditorCommands::Get().ReimportMesh->GetDescription(),
				FNewMenuDelegate::CreateLambda(CreateSubMenu, false, false));
			
			MenuBuilder.AddSubMenu(
				FSkeletalMeshEditorCommands::Get().ReimportMeshWithNewFile->GetLabel(),
				FSkeletalMeshEditorCommands::Get().ReimportMeshWithNewFile->GetDescription(),
				FNewMenuDelegate::CreateLambda(CreateSubMenu, false, true));

			MenuBuilder.AddSubMenu(
				FSkeletalMeshEditorCommands::Get().ReimportAllMesh->GetLabel(),
				FSkeletalMeshEditorCommands::Get().ReimportAllMesh->GetDescription(),
				FNewMenuDelegate::CreateLambda(CreateSubMenu, true, false));

			MenuBuilder.AddSubMenu(
				FSkeletalMeshEditorCommands::Get().ReimportAllMeshWithNewFile->GetLabel(),
				FSkeletalMeshEditorCommands::Get().ReimportAllMeshWithNewFile->GetDescription(),
				FNewMenuDelegate::CreateLambda(CreateSubMenu, true, true));
		}

		return MenuBuilder.MakeWidget();
	};

	// extend extra menu/toolbars
	auto FillToolbar = [this, ConstructReimportContextMenu](FToolBarBuilder& ToolbarBuilder)
	{
		FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
		FPersonaModule::FCommonToolbarExtensionArgs Args;
		Args.bPreviewMesh = false;
		PersonaModule.AddCommonToolbarExtensions(ToolbarBuilder, PersonaToolkit.ToSharedRef(), Args);

		ToolbarBuilder.BeginSection("Mesh");
		{
			ToolbarBuilder.AddToolBarButton(FSkeletalMeshEditorCommands::Get().ReimportMesh);
			ToolbarBuilder.AddComboButton(
				FUIAction(),
				FOnGetContent::CreateLambda(ConstructReimportContextMenu),
				TAttribute<FText>(),
				TAttribute<FText>()
			);
		}
		ToolbarBuilder.EndSection();

		ToolbarBuilder.BeginSection("Selection");
		{
			ToolbarBuilder.AddToolBarButton(FSkeletalMeshEditorCommands::Get().MeshSectionSelection);
		}
		ToolbarBuilder.EndSection();
	};

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda(FillToolbar)
		);

	AddToolbarExtender(ToolbarExtender);

	ISkeletalMeshEditorModule& SkeletalMeshEditorModule = FModuleManager::GetModuleChecked<ISkeletalMeshEditorModule>("SkeletalMeshEditor");
	AddToolbarExtender(SkeletalMeshEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	TArray<ISkeletalMeshEditorModule::FSkeletalMeshEditorToolbarExtender> ToolbarExtenderDelegates = SkeletalMeshEditorModule.GetAllSkeletalMeshEditorToolbarExtenders();

	for (auto& ToolbarExtenderDelegate : ToolbarExtenderDelegates)
	{
		if (ToolbarExtenderDelegate.IsBound())
		{
			AddToolbarExtender(ToolbarExtenderDelegate.Execute(GetToolkitCommands(), SharedThis(this)));
		}
	}

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ParentToolbarBuilder)
	{
		FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
		TSharedRef<class IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(SkeletalMesh);
		AddToolbarWidget(PersonaModule.CreateAssetFamilyShortcutWidget(SharedThis(this), AssetFamily));
	}
	));
}

void FSkeletalMeshEditor::FillMeshClickMenu(FMenuBuilder& MenuBuilder, HActor* HitProxy, const FViewportClick& Click)
{
	UDebugSkelMeshComponent* MeshComp = GetPersonaToolkit()->GetPreviewMeshComponent();

	// Must have hit something, but if the preview is invalid, bail
	if(!MeshComp)
	{
		return;
	}

	const int32 LodIndex = MeshComp->PredictedLODLevel;
	const int32 SectionIndex = HitProxy->SectionIndex;

	TSharedRef<SWidget> InfoWidget = SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(2.5f, 5.0f, 2.5f, 0.0f))
		[
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			//.Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
				.Text(FText::Format(LOCTEXT("MeshClickMenu_SectionInfo", "LOD{0} - Section {1}"), LodIndex, SectionIndex))
				]
			]
		];


	MenuBuilder.AddWidget(InfoWidget, FText::GetEmpty(), true, false);

	MenuBuilder.BeginSection(TEXT("MeshClickMenu_Asset"), LOCTEXT("MeshClickMenu_Section_Asset", "Asset"));
	{
		FUIAction Action;
		Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &FSkeletalMeshEditor::CanApplyClothing, LodIndex, SectionIndex);

		MenuBuilder.AddSubMenu(
			LOCTEXT("MeshClickMenu_AssetApplyMenu", "Apply Clothing Data..."),
			LOCTEXT("MeshClickMenu_AssetApplyMenu_ToolTip", "Select clothing data to apply to the selected section."),
			FNewMenuDelegate::CreateSP(this, &FSkeletalMeshEditor::FillApplyClothingAssetMenu, LodIndex, SectionIndex),
			Action,
			TEXT(""),
			EUserInterfaceActionType::Button
			);

		Action.ExecuteAction = FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::OnRemoveClothingAssetMenuItemClicked, LodIndex, SectionIndex);
		Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &FSkeletalMeshEditor::CanRemoveClothing, LodIndex, SectionIndex);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("MeshClickMenu_RemoveClothing", "Remove Clothing Data"),
			LOCTEXT("MeshClickMenu_RemoveClothing_ToolTip", "Remove the currently assigned clothing data."),
			FSlateIcon(),
			Action
			);
			
		Action.ExecuteAction = FExecuteAction();
		Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &FSkeletalMeshEditor::CanCreateClothing, LodIndex, SectionIndex);

		MenuBuilder.AddSubMenu(
			LOCTEXT("MeshClickMenu_CreateClothing_Label", "Create Clothing Data from Section"),
			LOCTEXT("MeshClickMenu_CreateClothing_ToolTip", "Create a new clothing data using the selected section as a simulation mesh"),
			FNewMenuDelegate::CreateSP(this, &FSkeletalMeshEditor::FillCreateClothingMenu, LodIndex, SectionIndex),
			Action,
			TEXT(""),
			EUserInterfaceActionType::Button
			);

		Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &FSkeletalMeshEditor::CanCreateClothingLod, LodIndex, SectionIndex);

		MenuBuilder.AddSubMenu(
			LOCTEXT("MeshClickMenu_CreateClothingNewLod_Label", "Create Clothing LOD from Section"),
			LOCTEXT("MeshClickMenu_CreateClothingNewLod_ToolTip", "Create a clothing simulation mesh from the selected section and add it as a LOD to existing clothing data."),
			FNewMenuDelegate::CreateSP(this, &FSkeletalMeshEditor::FillCreateClothingLodMenu, LodIndex, SectionIndex),
			Action,
			TEXT(""),
			EUserInterfaceActionType::Button
		);


		if (SkeletalMesh != nullptr && SkeletalMesh->GetImportedModel()->LODModels.IsValidIndex(LodIndex))
		{
			const FSkeletalMeshLODInfo* SkeletalMeshLODInfo = SkeletalMesh->GetLODInfo(LodIndex);
			if (SkeletalMeshLODInfo != nullptr)
			{
				FUIAction ActionRemoveSection;
				ActionRemoveSection.ExecuteAction = FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::OnRemoveSectionFromLodAndBelowMenuItemClicked, LodIndex, SectionIndex);

				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("MeshClickMenu_RemoveSectionFromLodAndBelow", "Generate section {1} up to LOD {0}"), LodIndex, SectionIndex),
					FText::Format(LOCTEXT("MeshClickMenu_RemoveSectionFromLodAndBelow_Tooltip", "Generated LODs will use section {1} up to LOD {0}, and ignore it for lower quality LODs"), LodIndex, SectionIndex),
					FSlateIcon(),
					ActionRemoveSection
				);
			}
		}
	}
	MenuBuilder.EndSection();
}

void FSkeletalMeshEditor::OnRemoveSectionFromLodAndBelowMenuItemClicked(int32 LodIndex, int32 SectionIndex)
{
	if (SkeletalMesh == nullptr || !SkeletalMesh->GetImportedModel()->LODModels.IsValidIndex(LodIndex) || !SkeletalMesh->GetImportedModel()->LODModels[LodIndex].Sections.IsValidIndex(SectionIndex))
	{
		return;
	}
	const FSkeletalMeshLODInfo* SkeletalMeshLODInfo = SkeletalMesh->GetLODInfo(LodIndex);
	if (SkeletalMeshLODInfo == nullptr)
	{
		return;
	}
	FScopedTransaction Transaction(LOCTEXT("ChangeGenerateUpTo", "Set Generate Up To"));
	SkeletalMesh->Modify();

	SkeletalMesh->GetImportedModel()->LODModels[LodIndex].Sections[SectionIndex].GenerateUpToLodIndex = LodIndex;
	FSkeletalMeshUpdateContext UpdateContext;
	UpdateContext.SkeletalMesh = SkeletalMesh;
	UpdateContext.AssociatedComponents.Push(GetPersonaToolkit()->GetPreviewMeshComponent());
	//Generate only the LODs that can be affected by the changes
	TArray<int32> BaseLodIndexes;
	BaseLodIndexes.Add(LodIndex);
	for (int32 GenerateLodIndex = LodIndex + 1; GenerateLodIndex < SkeletalMesh->GetImportedModel()->LODModels.Num(); ++GenerateLodIndex)
	{
		const FSkeletalMeshLODInfo* CurrentSkeletalMeshLODInfo = SkeletalMesh->GetLODInfo(GenerateLodIndex);
		if (CurrentSkeletalMeshLODInfo != nullptr && CurrentSkeletalMeshLODInfo->bHasBeenSimplified && BaseLodIndexes.Contains(CurrentSkeletalMeshLODInfo->ReductionSettings.BaseLOD))
		{
			FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, GenerateLodIndex);
			BaseLodIndexes.Add(GenerateLodIndex);
		}
	}
	SkeletalMesh->PostEditChange();
	GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
}

void FSkeletalMeshEditor::FillApplyClothingAssetMenu(FMenuBuilder& MenuBuilder, int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	// Nothing to fill
	if(!Mesh)
	{
		return;
	}

	MenuBuilder.BeginSection(TEXT("ApplyClothingMenu"), LOCTEXT("ApplyClothingMenuHeader", "Available Assets"));
	{
		for(UClothingAssetBase* BaseAsset : Mesh->MeshClothingAssets)
		{
			UClothingAsset* ClothAsset = CastChecked<UClothingAsset>(BaseAsset);

			FUIAction Action;
			Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &FSkeletalMeshEditor::CanApplyClothing, InLodIndex, InSectionIndex);

			const int32 NumClothLods = ClothAsset->LodData.Num();
			for(int32 ClothLodIndex = 0; ClothLodIndex < NumClothLods; ++ClothLodIndex)
			{
				Action.ExecuteAction = FExecuteAction::CreateSP(this, &FSkeletalMeshEditor::OnApplyClothingAssetClicked, BaseAsset, InLodIndex, InSectionIndex, ClothLodIndex);

				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("ApplyClothingMenuItem", "{0} - LOD{1}"), FText::FromString(ClothAsset->GetName()), FText::AsNumber(ClothLodIndex)),
					LOCTEXT("ApplyClothingMenuItem_ToolTip", "Apply this clothing asset to the selected mesh LOD and section"),
					FSlateIcon(),
					Action
					);
			}
		}
	}
	MenuBuilder.EndSection();
}

void FSkeletalMeshEditor::FillCreateClothingMenu(FMenuBuilder& MenuBuilder, int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(!Mesh)
	{
		return;
	}

	TSharedRef<SWidget> Widget = SNew(SCreateClothingSettingsPanel)
		.Mesh(Mesh)
		.MeshName(Mesh->GetName())
		.LodIndex(InLodIndex)
		.SectionIndex(InSectionIndex)
		.OnCreateRequested(this, &FSkeletalMeshEditor::OnCreateClothingAssetMenuItemClicked)
		.bIsSubImport(false);

	MenuBuilder.AddWidget(Widget, FText::GetEmpty(), true, false);
}

void FSkeletalMeshEditor::FillCreateClothingLodMenu(FMenuBuilder& MenuBuilder, int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(!Mesh)
	{
		return;
	}

	TSharedRef<SWidget> Widget = SNew(SCreateClothingSettingsPanel)
		.Mesh(Mesh)
		.MeshName(Mesh->GetName())
		.LodIndex(InLodIndex)
		.SectionIndex(InSectionIndex)
		.OnCreateRequested(this, &FSkeletalMeshEditor::OnCreateClothingAssetMenuItemClicked)
		.bIsSubImport(true);

		MenuBuilder.AddWidget(Widget, FText::GetEmpty(), true, false);
}

void FSkeletalMeshEditor::OnRemoveClothingAssetMenuItemClicked(int32 InLodIndex, int32 InSectionIndex)
{
	RemoveClothing(InLodIndex, InSectionIndex);
}

void FSkeletalMeshEditor::OnCreateClothingAssetMenuItemClicked(FSkeletalMeshClothBuildParams& Params)
{
	// Close the menu we created
	FSlateApplication::Get().DismissAllMenus();

	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(Mesh)
	{
		// Handle the creation through the clothing asset factory
		FClothingSystemEditorInterfaceModule& ClothingEditorModule = FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>("ClothingSystemEditorInterface");
		UClothingAssetFactoryBase* AssetFactory = ClothingEditorModule.GetClothingAssetFactory();

		Mesh->Modify();

		// See if we're importing a LOD or new asset
		if(Params.TargetAsset.IsValid())
		{
			UClothingAssetBase* TargetAssetPtr = Params.TargetAsset.Get();
			int32 SectionIndex = -1, AssetLodIndex = -1;
			if (Params.bRemapParameters)
			{
				if (TargetAssetPtr)
				{
					//Cache the section and asset LOD this asset was bound at before unbinding
					FSkeletalMeshLODModel& SkelLod = Mesh->GetImportedModel()->LODModels[Params.TargetLod];
					for (int32 i = 0; i < SkelLod.Sections.Num(); ++i)
					{
						if (SkelLod.Sections[i].ClothingData.AssetGuid == TargetAssetPtr->GetAssetGuid())
						{
							SectionIndex = i;
							AssetLodIndex = SkelLod.Sections[i].ClothingData.AssetLodIndex;
							TargetAssetPtr->UnbindFromSkeletalMesh(Mesh, Params.TargetLod);
							break;
						}
					}
				}
			}

			AssetFactory->ImportLodToClothing(Mesh, Params);

			if (Params.bRemapParameters)
			{
				//If it was bound previously, rebind at same section with same LOD
				if (TargetAssetPtr && SectionIndex > -1)
				{
					ApplyClothing(TargetAssetPtr, Params.TargetLod, SectionIndex, AssetLodIndex);
				}
			}
		}
		else
		{
			UClothingAssetBase* NewClothingAsset = AssetFactory->CreateFromSkeletalMesh(Mesh, Params);

			if(NewClothingAsset)
			{
				Mesh->AddClothingAsset(NewClothingAsset);
			}
		}

		//Make sure no section is isolated or highlighted
		UDebugSkelMeshComponent * MeshComponent = GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent();
		if(MeshComponent)
		{
			MeshComponent->SetSelectedEditorSection(INDEX_NONE);
			MeshComponent->SetSelectedEditorMaterial(INDEX_NONE);
			MeshComponent->SetMaterialPreview(INDEX_NONE);
			MeshComponent->SetSectionPreview(INDEX_NONE);
		}
	}
}

void FSkeletalMeshEditor::OnApplyClothingAssetClicked(UClothingAssetBase* InAssetToApply, int32 InMeshLodIndex, int32 InMeshSectionIndex, int32 InClothLodIndex)
{
	ApplyClothing(InAssetToApply, InMeshLodIndex, InMeshSectionIndex, InClothLodIndex);
}

bool FSkeletalMeshEditor::CanApplyClothing(int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(Mesh->MeshClothingAssets.Num() > 0)
	{
	FSkeletalMeshModel* MeshResource = Mesh->GetImportedModel();

	if(MeshResource->LODModels.IsValidIndex(InLodIndex))
	{
		FSkeletalMeshLODModel& LodModel = MeshResource->LODModels[InLodIndex];

		if(LodModel.Sections.IsValidIndex(InSectionIndex))
		{
			return !LodModel.Sections[InSectionIndex].HasClothingData();
		}
	}
	}

	return false;
}

bool FSkeletalMeshEditor::CanRemoveClothing(int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	FSkeletalMeshModel* MeshResource = Mesh->GetImportedModel();

	if(MeshResource->LODModels.IsValidIndex(InLodIndex))
	{
		FSkeletalMeshLODModel& LodModel = MeshResource->LODModels[InLodIndex];

		if(LodModel.Sections.IsValidIndex(InSectionIndex))
		{
			return LodModel.Sections[InSectionIndex].HasClothingData();
		}
	}

	return false;
}

bool FSkeletalMeshEditor::CanCreateClothing(int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	FSkeletalMeshModel* MeshResource = Mesh->GetImportedModel();

	if(MeshResource->LODModels.IsValidIndex(InLodIndex))
	{
		FSkeletalMeshLODModel& LodModel = MeshResource->LODModels[InLodIndex];

		if(LodModel.Sections.IsValidIndex(InSectionIndex))
		{
			FSkelMeshSection& Section = LodModel.Sections[InSectionIndex];

			return !Section.HasClothingData();
		}
	}

	return false;
}

bool FSkeletalMeshEditor::CanCreateClothingLod(int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	return Mesh && Mesh->MeshClothingAssets.Num() > 0 && CanApplyClothing(InLodIndex, InSectionIndex);
}

void FSkeletalMeshEditor::ApplyClothing(UClothingAssetBase* InAsset, int32 InLodIndex, int32 InSectionIndex, int32 InClothingLod)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(UClothingAsset* ClothingAsset = Cast<UClothingAsset>(InAsset))
	{
		ClothingAsset->BindToSkeletalMesh(Mesh, InLodIndex, InSectionIndex, InClothingLod);
	}
	else
	{
		RemoveClothing(InLodIndex, InSectionIndex);
	}
}

void FSkeletalMeshEditor::RemoveClothing(int32 InLodIndex, int32 InSectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetPreviewMesh();

	if(Mesh)
	{
		if(UClothingAssetBase* CurrentAsset = Mesh->GetSectionClothingAsset(InLodIndex, InSectionIndex))
		{
			CurrentAsset->UnbindFromSkeletalMesh(Mesh, InLodIndex);
		}
	}
}

void FSkeletalMeshEditor::ExtendMenu()
{
	MenuExtender = MakeShareable(new FExtender);

	AddMenuExtender(MenuExtender);

	ISkeletalMeshEditorModule& SkeletalMeshEditorModule = FModuleManager::GetModuleChecked<ISkeletalMeshEditorModule>("SkeletalMeshEditor");
	AddMenuExtender(SkeletalMeshEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FSkeletalMeshEditor::HandleObjectsSelected(const TArray<UObject*>& InObjects)
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObjects(InObjects);
	}
}

void FSkeletalMeshEditor::HandleObjectSelected(UObject* InObject)
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(InObject);
	}
}

void FSkeletalMeshEditor::HandleSelectionChanged(const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type InSelectInfo)
{
	if (DetailsView.IsValid())
	{
		TArray<UObject*> Objects;
		Algo::TransformIf(InSelectedItems, Objects, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject() != nullptr; }, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject(); });
		DetailsView->SetObjects(Objects);
	}
}

void FSkeletalMeshEditor::PostUndo(bool bSuccess)
{
	OnPostUndo.Broadcast();
}

void FSkeletalMeshEditor::PostRedo(bool bSuccess)
{
	OnPostUndo.Broadcast();
}

void FSkeletalMeshEditor::Tick(float DeltaTime)
{
	GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
}

TStatId FSkeletalMeshEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSkeletalMeshEditor, STATGROUP_Tickables);
}

void FSkeletalMeshEditor::HandleDetailsCreated(const TSharedRef<IDetailsView>& InDetailsView)
{
	DetailsView = InDetailsView;
}

void FSkeletalMeshEditor::HandleMeshDetailsCreated(const TSharedRef<IDetailsView>& InDetailsView)
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
	PersonaModule.CustomizeMeshDetails(InDetailsView, GetPersonaToolkit());
}

UObject* FSkeletalMeshEditor::HandleGetAsset()
{
	return GetEditingObject();
}

bool FSkeletalMeshEditor::HandleReimportMeshInternal(int32 SourceFileIndex /*= INDEX_NONE*/, bool bWithNewFile /*= false*/)
{
	// Reimport the asset
	return FReimportManager::Instance()->Reimport(SkeletalMesh, true, true, TEXT(""), nullptr, SourceFileIndex, bWithNewFile);
}

void FSkeletalMeshEditor::HandleReimportMesh(int32 SourceFileIndex /*= INDEX_NONE*/)
{
	HandleReimportMeshInternal(SourceFileIndex, false);
}

void FSkeletalMeshEditor::HandleReimportMeshWithNewFile(int32 SourceFileIndex /*= INDEX_NONE*/)
{
	HandleReimportMeshInternal(SourceFileIndex, true);
}

void ReimportAllCustomLODs(USkeletalMesh* SkeletalMesh, UDebugSkelMeshComponent* PreviewMeshComponent, bool bWithNewFile)
{
	//Find the dependencies of the generated LOD
	TArray<bool> Dependencies;
	Dependencies.AddZeroed(SkeletalMesh->GetLODNum());
	//Avoid making LOD 0 to true in the dependencies since everything that should be regenerate base on LOD 0 is already regenerate at this point.
	//But we need to regenerate every generated LOD base on any re-import custom LOD
	//Reimport all custom LODs
	for (int32 LodIndex = 1; LodIndex < SkeletalMesh->GetLODNum(); ++LodIndex)
	{
		//Do not reimport LOD that was re-import with the base mesh
		if (SkeletalMesh->GetLODInfo(LodIndex)->bImportWithBaseMesh)
		{
			continue;
		}
		if (SkeletalMesh->GetLODInfo(LodIndex)->bHasBeenSimplified == false)
		{
			FString SourceFilenameBackup = SkeletalMesh->GetLODInfo(LodIndex)->SourceImportFilename;
			if (bWithNewFile)
			{
				SkeletalMesh->GetLODInfo(LodIndex)->SourceImportFilename.Empty();
			}

			if (!FbxMeshUtils::ImportMeshLODDialog(SkeletalMesh, LodIndex))
			{
				if (bWithNewFile)
				{
					SkeletalMesh->GetLODInfo(LodIndex)->SourceImportFilename = SourceFilenameBackup;
				}
			}
			else
			{
				Dependencies[LodIndex] = true;
			}
		}
		else if(Dependencies[SkeletalMesh->GetLODInfo(LodIndex)->ReductionSettings.BaseLOD])
		{
			//Regenerate the LOD
			FSkeletalMeshUpdateContext UpdateContext;
			UpdateContext.SkeletalMesh = SkeletalMesh;
			UpdateContext.AssociatedComponents.Push(PreviewMeshComponent);
			FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, LodIndex);
			Dependencies[LodIndex] = true;
		}
	}
}

void FSkeletalMeshEditor::HandleReimportAllMesh(int32 SourceFileIndex /*= INDEX_NONE*/)
{
	// Reimport the asset
	if (SkeletalMesh)
	{
		//Reimport base LOD
		if (HandleReimportMeshInternal(SourceFileIndex, false))
		{
			//Reimport all custom LODs
			ReimportAllCustomLODs(SkeletalMesh, GetPersonaToolkit()->GetPreviewMeshComponent(), false);
		}
	}
}

void FSkeletalMeshEditor::HandleReimportAllMeshWithNewFile(int32 SourceFileIndex /*= INDEX_NONE*/)
{
	// Reimport the asset
	if (SkeletalMesh)
	{
		TArray<UObject*> ImportObjs;
		ImportObjs.Add(SkeletalMesh);
		if (HandleReimportMeshInternal(SourceFileIndex, true))
		{
			//Reimport all custom LODs
			ReimportAllCustomLODs(SkeletalMesh, GetPersonaToolkit()->GetPreviewMeshComponent(), true);
		}
	}
}


void FSkeletalMeshEditor::ToggleMeshSectionSelection()
{
	TSharedRef<IPersonaPreviewScene> PreviewScene = GetPersonaToolkit()->GetPreviewScene();
	PreviewScene->DeselectAll();
	bool bState = !PreviewScene->AllowMeshHitProxies();
	GetMutableDefault<UPersonaOptions>()->bAllowMeshSectionSelection = bState;
	PreviewScene->SetAllowMeshHitProxies(bState);
}

bool FSkeletalMeshEditor::IsMeshSectionSelectionChecked() const
{
	return GetPersonaToolkit()->GetPreviewScene()->AllowMeshHitProxies();
}

void FSkeletalMeshEditor::HandleMeshClick(HActor* HitProxy, const FViewportClick& Click)
{
	USkeletalMeshComponent* Component = GetPersonaToolkit()->GetPreviewMeshComponent();
	if (Component)
	{
		Component->SetSelectedEditorSection(HitProxy->SectionIndex);
		Component->PushSelectionToProxy();
	}

	if(Click.GetKey() == EKeys::RightMouseButton)
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		FillMeshClickMenu(MenuBuilder, HitProxy, Click);

		FSlateApplication::Get().PushMenu(
			FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef(),
			FWidgetPath(),
			MenuBuilder.MakeWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
			);
	}
}

#undef LOCTEXT_NAMESPACE
