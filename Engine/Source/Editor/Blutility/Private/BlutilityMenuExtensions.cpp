// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BlutilityMenuExtensions.h"
#include "AssetRegistryModule.h"
#include "EditorUtilityBlueprint.h"
#include "Misc/PackageName.h"
#include "Toolkits/AssetEditorManager.h"
#include "BlueprintEditorModule.h"
#include "GlobalEditorUtilityBase.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "PropertyEditorModule.h"
#include "IStructureDetailsView.h"
#include "EditorStyleSet.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "UObject/PropertyPortFlags.h"
#include "Widgets/Layout/SScrollBox.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "BlutilityMenuExtensions"

/** Dialog widget used to display function properties */
class SFunctionParamDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SFunctionParamDialog) {}

	/** Text to display on the "OK" button */
	SLATE_ARGUMENT(FText, OkButtonText)

	/** Tooltip text for the "OK" button */
	SLATE_ARGUMENT(FText, OkButtonTooltipText)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow, TSharedRef<FStructOnScope> InStructOnScope)
	{
		bOKPressed = false;

		// Initialize details view
		FDetailsViewArgs DetailsViewArgs;
		{
			DetailsViewArgs.bAllowSearch = false;
			DetailsViewArgs.bHideSelectionTip = true;
			DetailsViewArgs.bLockable = false;
			DetailsViewArgs.bSearchInitialKeyFocus = true;
			DetailsViewArgs.bUpdatesFromSelection = false;
			DetailsViewArgs.bShowOptions = false;
			DetailsViewArgs.bShowModifiedPropertiesOption = false;
			DetailsViewArgs.bShowActorLabel = false;
			DetailsViewArgs.bForceHiddenPropertyVisibility = true;
			DetailsViewArgs.bShowScrollBar = false;
		}
	
		FStructureDetailsViewArgs StructureViewArgs;
		{
			StructureViewArgs.bShowObjects = true;
			StructureViewArgs.bShowAssets = true;
			StructureViewArgs.bShowClasses = true;
			StructureViewArgs.bShowInterfaces = true;
		}

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		TSharedRef<IStructureDetailsView> StructureDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, InStructOnScope);

		StructureDetailsView->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& InPropertyAndParent)
		{
			return InPropertyAndParent.Property.HasAnyPropertyFlags(CPF_Parm);
		}));

		StructureDetailsView->GetDetailsView()->ForceRefresh();

		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+SScrollBox::Slot()
				[
					StructureDetailsView->GetWidget().ToSharedRef()
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(6, 2))
						.OnClicked_Lambda([this, InParentWindow, InArgs]()
						{
							if(InParentWindow.IsValid())
							{
								InParentWindow.Pin()->RequestDestroyWindow();
							}
							bOKPressed = true;
							return FReply::Handled(); 
						})
						.ToolTipText(InArgs._OkButtonTooltipText)
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
							.Text(InArgs._OkButtonText)
						]
					]
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(6, 2))
						.OnClicked_Lambda([InParentWindow]()
						{ 
							if(InParentWindow.IsValid())
							{
								InParentWindow.Pin()->RequestDestroyWindow();
							}
							return FReply::Handled(); 
						})
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
							.Text(LOCTEXT("Cancel", "Cancel"))
						]
					]
				]
			]
		];
	}

	bool bOKPressed;
};

void FBlutilityMenuExtensions::GetBlutilityClasses(TArray<FAssetData>& OutAssets, const FName& InClassName)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Get class names
	TArray<FName> BaseNames;
	BaseNames.Add(InClassName);
	TSet<FName> Excluded;
	TSet<FName> DerivedNames;
	AssetRegistry.GetDerivedClassNames(BaseNames, Excluded, DerivedNames);

	// Now get all UEditorUtilityBlueprint assets
	FARFilter Filter;
	Filter.ClassNames.Add(UEditorUtilityBlueprint::StaticClass()->GetFName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	// Check each asset to see if it matches our type
	for (const FAssetData& Asset : AssetList)
	{
		FAssetDataTagMapSharedView::FFindTagResult Result = Asset.TagsAndValues.FindTag(FBlueprintTags::GeneratedClassPath);
		if (Result.IsSet())
		{
			const FString ClassObjectPath = FPackageName::ExportTextPathToObjectPath(Result.GetValue());
			const FString ClassName = FPackageName::ObjectPathToObjectName(ClassObjectPath);

			if (DerivedNames.Contains(*ClassName))
			{
				OutAssets.Add(Asset);
			}
		}
	}
}

void FBlutilityMenuExtensions::CreateBlutilityActionsMenu(FMenuBuilder& MenuBuilder, TArray<UGlobalEditorUtilityBase*> Utils)
{
	const static FName NAME_CallInEditor(TEXT("CallInEditor"));

	// Helper struct to track the util to call a function on
	struct FFunctionAndUtil
	{
		FFunctionAndUtil(UFunction* InFunction, UGlobalEditorUtilityBase* InUtil) 
			: Function(InFunction)
			, Util(InUtil) {}

		bool operator==(const FFunctionAndUtil& InFunction) const
		{
			return InFunction.Function == Function;
		}

		UFunction* Function;
		UGlobalEditorUtilityBase* Util;
	};
	TArray<FFunctionAndUtil> FunctionsToList;
	TSet<UClass*> ProcessedClasses;

	// Find the exposed functions available in each class, making sure to not list shared functions from a parent class more than once
	for(UGlobalEditorUtilityBase* Util : Utils)
	{
		UClass* Class = Util->GetClass();

		if (ProcessedClasses.Contains(Class))
		{
			continue;
		}

		for (UClass* ParentClass = Class; ParentClass != UObject::StaticClass(); ParentClass = ParentClass->GetSuperClass())
		{
			ProcessedClasses.Add(ParentClass);
		}

		for (TFieldIterator<UFunction> FunctionIt(Class); FunctionIt; ++FunctionIt)
		{
			if (UFunction* Func = *FunctionIt)
			{
				if (Func->HasMetaData(NAME_CallInEditor) && Func->GetReturnProperty() == nullptr)
				{
					FunctionsToList.AddUnique(FFunctionAndUtil(Func, Util));
				}
			}
		}
	}

	// Sort the functions by name
	FunctionsToList.Sort([](const FFunctionAndUtil& A, const FFunctionAndUtil& B) { return A.Function->GetName() < B.Function->GetName(); });

	// Add a menu item for each function
	if (FunctionsToList.Num() > 0)
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("ScriptedActorActions", "Scripted Actions"), 
			LOCTEXT("ScriptedActorActionsTooltip", "Scripted actions available for the selected actors"),
			FNewMenuDelegate::CreateLambda([FunctionsToList](FMenuBuilder& InMenuBuilder)
			{
				for (const FFunctionAndUtil& FunctionAndUtil : FunctionsToList)
				{
					const FText TooltipText = FText::Format(LOCTEXT("AssetUtilTooltipFormat", "{0}\n(Shift-click to edit script)"), FunctionAndUtil.Function->GetToolTipText());

					InMenuBuilder.AddMenuEntry(
						FunctionAndUtil.Function->GetDisplayNameText(), 
						TooltipText,
						FSlateIcon("EditorStyle", "GraphEditor.Event_16x"),
						FExecuteAction::CreateLambda([FunctionAndUtil] 
						{
							if(FSlateApplication::Get().GetModifierKeys().IsShiftDown())
							{
								// Edit the script if we have shift held down
								if(UBlueprint* Blueprint = Cast<UBlueprint>(FunctionAndUtil.Util->GetClass()->ClassGeneratedBy))
								{
									if(IAssetEditorInstance* AssetEditor = FAssetEditorManager::Get().FindEditorForAsset(Blueprint, true))
									{
										check(AssetEditor->GetEditorName() == TEXT("BlueprintEditor"));
										IBlueprintEditor* BlueprintEditor = static_cast<IBlueprintEditor*>(AssetEditor);
										BlueprintEditor->JumpToHyperlink(FunctionAndUtil.Function, false);
									}
									else
									{
										FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
										TSharedRef<IBlueprintEditor> BlueprintEditor = BlueprintEditorModule.CreateBlueprintEditor(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), Blueprint, false);
										BlueprintEditor->JumpToHyperlink(FunctionAndUtil.Function, false);
									}
								}
							}
							else
							{
								// We dont run this on the CDO, as bad things could occur!
								UObject* TempObject = NewObject<UObject>(GetTransientPackage(), FunctionAndUtil.Util->GetClass());
								TempObject->AddToRoot(); // Some Blutility actions might run GC so the TempObject needs to be rooted to avoid getting destroyed

								if(FunctionAndUtil.Function->NumParms > 0)
								{
									// Create a parameter struct and fill in defaults
									TSharedRef<FStructOnScope> FuncParams = MakeShared<FStructOnScope>(FunctionAndUtil.Function);
									for (TFieldIterator<UProperty> It(FunctionAndUtil.Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
									{
										FString Defaults;
										if(UEdGraphSchema_K2::FindFunctionParameterDefaultValue(FunctionAndUtil.Function, *It, Defaults))
										{
											It->ImportText(*Defaults, It->ContainerPtrToValuePtr<uint8>(FuncParams->GetStructMemory()), PPF_None, nullptr);
										}
									}

									// pop up a dialog to input params to the function
									TSharedRef<SWindow> Window = SNew(SWindow)
										.Title(FunctionAndUtil.Function->GetDisplayNameText())
										.ClientSize(FVector2D(400, 200))
										.SupportsMinimize(false)
										.SupportsMaximize(false);

									TSharedPtr<SFunctionParamDialog> Dialog;
									Window->SetContent(
										SAssignNew(Dialog, SFunctionParamDialog, Window, FuncParams)
										.OkButtonText(LOCTEXT("OKButton", "OK"))
										.OkButtonTooltipText(FunctionAndUtil.Function->GetToolTipText()));

									GEditor->EditorAddModalWindow(Window);

									if(Dialog->bOKPressed)
									{
										FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "BlutilityAction", "Blutility Action") );
										FEditorScriptExecutionGuard ScriptGuard;
										TempObject->ProcessEvent(FunctionAndUtil.Function, FuncParams->GetStructMemory());
									}
								}
								else
								{
									FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "BlutilityAction", "Blutility Action") );
									FEditorScriptExecutionGuard ScriptGuard;
									TempObject->ProcessEvent(FunctionAndUtil.Function, nullptr);
								}

								TempObject->RemoveFromRoot();
							}
						}));
				}
			}),
			false,
			FSlateIcon("EditorStyle", "GraphEditor.Event_16x"));
	}
}

#undef LOCTEXT_NAMESPACE 