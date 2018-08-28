// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PersonaMeshDetails.h"
#include "Widgets/Input/SCheckBox.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IDetailGroup.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "Components/SkinnedMeshComponent.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "IPersonaPreviewScene.h"
#include "Animation/AnimBlueprint.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorDirectories.h"
#include "UnrealEdGlobals.h"
#include "IDetailsView.h"
#include "PropertyCustomizationHelpers.h"
#include "DesktopPlatformModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Rendering/SkeletalMeshModel.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "EditorFramework/AssetImportData.h"

#if WITH_APEX_CLOTHING
	#include "ApexClothingUtils.h"
	#include "ApexClothingOptionWindow.h"
#endif // #if WITH_APEX_CLOTHING

#include "Assets/ClothingAsset.h"

#include "LODUtilities.h"
#include "MeshUtilities.h"
#include "FbxMeshUtils.h"

#include "Widgets/Input/STextComboBox.h"

#include "Engine/SkeletalMeshLODSettings.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "IPersonaToolkit.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "IPersonaPreviewScene.h"
#include "IDocumentation.h"
#include "JsonObjectConverter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "ComponentReregisterContext.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "ClothingAssetFactoryInterface.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "SKismetInspector.h"
#include "PropertyEditorDelegates.h"
#include "IEditableSkeleton.h"
#include "IMeshReductionManagerModule.h"

#define LOCTEXT_NAMESPACE "PersonaMeshDetails"

/*
* Custom data key
*/
enum SK_CustomDataKey
{
	CustomDataKey_LODVisibilityState = 0, //This is the key to know if a LOD is shown in custom mode. Do CustomDataKey_LODVisibilityState + LodIndex for a specific LOD
	CustomDataKey_LODEditMode = 100 //This is the key to know the state of the custom lod edit mode.
};

namespace PersonaMeshDetailsConstants
{
	/** Number to extend the num lods slider beyond the currently existing number of lods in the mesh */
	const static int32 LodSliderExtension = 5;
}

/** Returns true if automatic mesh reduction is available. */
static bool IsAutoMeshReductionAvailable()
{
	static bool bAutoMeshReductionAvailable = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface").GetSkeletalMeshReductionInterface() != NULL;
	return bAutoMeshReductionAvailable;
}


// Container widget for LOD buttons
class SSkeletalLODActions : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSkeletalLODActions)
		: _LODIndex(INDEX_NONE)
		, _PersonaToolkit(nullptr)
	{}
	SLATE_ARGUMENT(int32, LODIndex)
	SLATE_ARGUMENT(TWeakPtr<IPersonaToolkit>, PersonaToolkit)
	SLATE_EVENT(FOnClicked, OnApplyLODChangeClicked)
	SLATE_EVENT(FOnClicked, OnRemoveLODClicked)
	SLATE_EVENT(FOnClicked, OnReimportClicked)
	SLATE_EVENT(FOnClicked, OnReimportNewFileClicked)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	EActiveTimerReturnType RefreshExistFlag(double InCurrentTime, float InDeltaSeconds)
	{
		bDoesSourceFileExist_Cached = false;

		TSharedPtr<IPersonaToolkit> SharedToolkit = PersonaToolkit.Pin();
		if(SharedToolkit.IsValid())
		{
			USkeletalMesh* SkelMesh = SharedToolkit->GetMesh();

			if(!SkelMesh)
			{
				return EActiveTimerReturnType::Continue;
			}

			if(SkelMesh->IsValidLODIndex(LODIndex))
			{
				FSkeletalMeshLODInfo& LODInfo = *(SkelMesh->GetLODInfo(LODIndex));

				bDoesSourceFileExist_Cached = !LODInfo.SourceImportFilename.IsEmpty() && FPaths::FileExists(UAssetImportData::ResolveImportFilename(LODInfo.SourceImportFilename, nullptr));
			}
		}
		return EActiveTimerReturnType::Continue;
	}

	FText GetReimportButtonToolTipText() const
	{
		TSharedPtr<IPersonaToolkit> SharedToolkit = PersonaToolkit.Pin();

		if(!CanReimportFromSource() || !SharedToolkit.IsValid())
		{
			return LOCTEXT("ReimportButton_NewFile_NoSource_ToolTip", "No source file available for reimport");
		}

		USkeletalMesh* SkelMesh = SharedToolkit->GetMesh();
		check(SkelMesh);
		if (!SkelMesh->IsValidLODIndex(LODIndex))
		{
			// Should be true for the button to exist except if we delete a LOD
			return LOCTEXT("ReimportButton_NewFile_LODNotValid_ToolTip", "Cannot reimport, LOD was delete");
		}

		FSkeletalMeshLODInfo& LODInfo = *(SkelMesh->GetLODInfo(LODIndex));

		FString Filename = FPaths::GetCleanFilename(LODInfo.SourceImportFilename);

		return FText::Format(LOCTEXT("ReimportButton_NewFile_ToolTip", "Reimport LOD{0} using the current source file ({1})"), FText::AsNumber(LODIndex), FText::FromString(Filename));
	}

	FText GetReimportButtonNewFileToolTipText() const
	{
		return FText::Format(LOCTEXT("ReimportButton_ToolTip", "Choose a new file to reimport over this LOD (LOD{0})"), FText::AsNumber(LODIndex));
	}

	bool CanReimportFromSource() const
	{
		return bDoesSourceFileExist_Cached;
	}

	// Incoming arg data
	int32 LODIndex;
	TWeakPtr<IPersonaToolkit> PersonaToolkit;
	FOnClicked OnApplyLODChangeClicked;
	FOnClicked OnRemoveLODClicked;
	FOnClicked OnReimportClicked;
	FOnClicked OnReimportNewFileClicked;

	// Cached exists flag so we don't constantly hit the file system
	bool bDoesSourceFileExist_Cached;
};

void SSkeletalLODActions::Construct(const FArguments& InArgs)
{
	LODIndex = InArgs._LODIndex;
	PersonaToolkit = InArgs._PersonaToolkit;
	OnApplyLODChangeClicked = InArgs._OnApplyLODChangeClicked;
	OnRemoveLODClicked = InArgs._OnRemoveLODClicked;
	OnReimportClicked = InArgs._OnReimportClicked;
	OnReimportNewFileClicked = InArgs._OnReimportNewFileClicked;

	TSharedPtr<SWrapBox> WrapBox;
	this->ChildSlot

		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SAssignNew(WrapBox, SWrapBox)
				.UseAllottedWidth(true)
			]
		];

	if (OnApplyLODChangeClicked.IsBound())
	{
		WrapBox->AddSlot()
		.Padding(FMargin(0, 0, 2, 4))
		[
			SNew(SBox)
			.WidthOverride(120.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked(OnApplyLODChangeClicked)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("RegenerateLOD", "Regenerate LOD"))
				]
			]
		];
	}

	if (OnRemoveLODClicked.IsBound())
	{
		WrapBox->AddSlot()
		.Padding(FMargin(0, 0, 2, 4))
		[
			SNew(SBox)
			.WidthOverride(120.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked(OnRemoveLODClicked)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("RemoveLOD", "Remove this LOD"))
				]
			]
		];
	}

	if (OnReimportClicked.IsBound())
	{
		WrapBox->AddSlot()
		.Padding(FMargin(0, 0, 2, 4))
		[
			SNew(SBox)
			.WidthOverride(120.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(this, &SSkeletalLODActions::GetReimportButtonToolTipText)
				.IsEnabled(this, &SSkeletalLODActions::CanReimportFromSource)
				.OnClicked(OnReimportClicked)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("ReimportLOD", "Reimport"))
				]
			]
		];
	}

	if (OnReimportNewFileClicked.IsBound())
	{
		WrapBox->AddSlot()
		.Padding(FMargin(0, 0, 2, 4))
		[
			SNew(SBox)
			.WidthOverride(120.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(this, &SSkeletalLODActions::GetReimportButtonNewFileToolTipText)
				.OnClicked(OnReimportNewFileClicked)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("ReimportLOD_NewFile", "Reimport (New File)"))
				]
			]
		];
	}

	// Register timer to refresh out exists flag periodically, with a bit added per LOD so we're not doing everything on the same frame
	const float LODTimeOffset = 1.0f / 30.0f;
	RegisterActiveTimer(1.0f + LODTimeOffset * LODIndex, FWidgetActiveTimerDelegate::CreateSP(this, &SSkeletalLODActions::RefreshExistFlag));
}

/**
* FPersonaMeshDetails
*/
FPersonaMeshDetails::~FPersonaMeshDetails()
{
	if (HasValidPersonaToolkit())
	{
		TSharedRef<IPersonaPreviewScene> PreviewScene = GetPersonaToolkit()->GetPreviewScene();
		PreviewScene->UnregisterOnPreviewMeshChanged(this);
	}
}

TSharedRef<IDetailCustomization> FPersonaMeshDetails::MakeInstance(TWeakPtr<class IPersonaToolkit> InPersonaToolkit)
{
	return MakeShareable( new FPersonaMeshDetails(InPersonaToolkit.Pin().ToSharedRef()) );
}

void FPersonaMeshDetails::OnCopySectionList(int32 LODIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		FSkeletalMeshModel* ImportedResource = Mesh->GetImportedModel();

		if (ImportedResource != nullptr && ImportedResource->LODModels.IsValidIndex(LODIndex))
		{
			const FSkeletalMeshLODModel& Model = ImportedResource->LODModels[LODIndex];

			TSharedRef<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject());

			for (int32 SectionIdx = 0; SectionIdx < Model.Sections.Num(); ++SectionIdx)
			{
				const FSkelMeshSection& ModelSection = Model.Sections[SectionIdx];

				TSharedPtr<FJsonObject> JSonSection = MakeShareable(new FJsonObject);

				JSonSection->SetNumberField(TEXT("MaterialIndex"), ModelSection.MaterialIndex);
				JSonSection->SetBoolField(TEXT("RecomputeTangent"), ModelSection.bRecomputeTangent);
				JSonSection->SetBoolField(TEXT("CastShadow"), ModelSection.bCastShadow);
				JSonSection->SetNumberField(TEXT("GenerateUpToLodIndex"), ModelSection.GenerateUpToLodIndex);

				RootJsonObject->SetObjectField(FString::Printf(TEXT("Section_%d"), SectionIdx), JSonSection);
			}

			typedef TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriter;
			typedef TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriterFactory;

			FString CopyStr;
			TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(&CopyStr);
			FJsonSerializer::Serialize(RootJsonObject, Writer);

			if (!CopyStr.IsEmpty())
			{
				FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
			}
		}
	}
}

bool FPersonaMeshDetails::OnCanCopySectionList(int32 LODIndex) const
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		FSkeletalMeshModel* ImportedResource = Mesh->GetImportedModel();

		if (ImportedResource != nullptr && ImportedResource->LODModels.IsValidIndex(LODIndex))
		{
			return ImportedResource->LODModels[LODIndex].Sections.Num() > 0;
		}
	}

	return false;
}

void FPersonaMeshDetails::OnPasteSectionList(int32 LODIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		FString PastedText;
		FPlatformApplicationMisc::ClipboardPaste(PastedText);

		TSharedPtr<FJsonObject> RootJsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PastedText);
		FJsonSerializer::Deserialize(Reader, RootJsonObject);

		if (RootJsonObject.IsValid())
		{
			FSkeletalMeshModel* ImportedResource = Mesh->GetImportedModel();

			if (ImportedResource != nullptr && ImportedResource->LODModels.IsValidIndex(LODIndex))
			{
				FScopedTransaction Transaction(LOCTEXT("PersonaChangedPasteSectionList", "Persona editor: Pasted section list"));
				Mesh->Modify();

				FSkeletalMeshLODModel& Model = ImportedResource->LODModels[LODIndex];

				for (int32 SectionIdx = 0; SectionIdx < Model.Sections.Num(); ++SectionIdx)
				{
					FSkelMeshSection& ModelSection = Model.Sections[SectionIdx];

					const TSharedPtr<FJsonObject>* JSonSection = nullptr;
					if (RootJsonObject->TryGetObjectField(FString::Printf(TEXT("Section_%d"), SectionIdx), JSonSection))
					{
						int32 Value;
						if ((*JSonSection)->TryGetNumberField(TEXT("MaterialIndex"), Value))
						{
							ModelSection.MaterialIndex = (uint16)Value;
						}

						(*JSonSection)->TryGetBoolField(TEXT("RecomputeTangent"), ModelSection.bRecomputeTangent);
						(*JSonSection)->TryGetBoolField(TEXT("CastShadow"), ModelSection.bCastShadow);
						if ((*JSonSection)->TryGetNumberField(TEXT("GenerateUpToLodIndex"), Value))
						{
							ModelSection.GenerateUpToLodIndex = (int8)Value;
						}
					}
				}

				Mesh->PostEditChange();
			}
		}
	}
}

void FPersonaMeshDetails::OnCopySectionItem(int32 LODIndex, int32 SectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		FSkeletalMeshModel* ImportedResource = Mesh->GetImportedModel();

		if (ImportedResource != nullptr && ImportedResource->LODModels.IsValidIndex(LODIndex))
		{
			const FSkeletalMeshLODModel& Model = ImportedResource->LODModels[LODIndex];

			TSharedRef<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject());

			if (Model.Sections.IsValidIndex(SectionIndex))
			{
				const FSkelMeshSection& ModelSection = Model.Sections[SectionIndex];

				RootJsonObject->SetNumberField(TEXT("MaterialIndex"), ModelSection.MaterialIndex);
				RootJsonObject->SetBoolField(TEXT("RecomputeTangent"), ModelSection.bRecomputeTangent);
				RootJsonObject->SetBoolField(TEXT("CastShadow"), ModelSection.bCastShadow);
				RootJsonObject->SetNumberField(TEXT("GenerateUpToLodIndex"), ModelSection.GenerateUpToLodIndex);
			}

			typedef TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriter;
			typedef TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriterFactory;

			FString CopyStr;
			TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(&CopyStr);
			FJsonSerializer::Serialize(RootJsonObject, Writer);

			if (!CopyStr.IsEmpty())
			{
				FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
			}
		}
	}
}

bool FPersonaMeshDetails::OnCanCopySectionItem(int32 LODIndex, int32 SectionIndex) const
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		FSkeletalMeshModel* ImportedResource = Mesh->GetImportedModel();

		if (ImportedResource != nullptr && ImportedResource->LODModels.IsValidIndex(LODIndex))
		{
			return ImportedResource->LODModels[LODIndex].Sections.IsValidIndex(SectionIndex);
		}
	}

	return false;
}

void FPersonaMeshDetails::OnPasteSectionItem(int32 LODIndex, int32 SectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		FString PastedText;
		FPlatformApplicationMisc::ClipboardPaste(PastedText);

		TSharedPtr<FJsonObject> RootJsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PastedText);
		FJsonSerializer::Deserialize(Reader, RootJsonObject);

		if (RootJsonObject.IsValid())
		{
			FSkeletalMeshModel* ImportedResource = Mesh->GetImportedModel();

			if (ImportedResource != nullptr && ImportedResource->LODModels.IsValidIndex(LODIndex))
			{
				FSkeletalMeshLODModel& Model = ImportedResource->LODModels[LODIndex];

				FScopedTransaction Transaction(LOCTEXT("PersonaChangedPasteSectionItem", "Persona editor: Pasted section item"));
				Mesh->Modify();

				if (Model.Sections.IsValidIndex(SectionIndex))
				{
					FSkelMeshSection& ModelSection = Model.Sections[SectionIndex];

					int32 Value;
					if (RootJsonObject->TryGetNumberField(TEXT("MaterialIndex"), Value))
					{
						ModelSection.MaterialIndex = (uint16)Value;
					}

					RootJsonObject->TryGetBoolField(TEXT("RecomputeTangent"), ModelSection.bRecomputeTangent);
					RootJsonObject->TryGetBoolField(TEXT("CastShadow"), ModelSection.bCastShadow);
					if (RootJsonObject->TryGetNumberField(TEXT("GenerateUpToLodIndex"), Value))
					{
						ModelSection.GenerateUpToLodIndex = (int8)Value;
					}
				}

				Mesh->PostEditChange();
			}
		}
	}
}

void FPersonaMeshDetails::OnCopyMaterialList()
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		UProperty* Property = USkeletalMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(USkeletalMesh, Materials));
		auto JsonValue = FJsonObjectConverter::UPropertyToJsonValue(Property, &Mesh->Materials, 0, 0);

		typedef TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriter;
		typedef TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriterFactory;

		FString CopyStr;
		TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(&CopyStr);
		FJsonSerializer::Serialize(JsonValue.ToSharedRef(), TEXT(""), Writer);

		if (!CopyStr.IsEmpty())
		{
			FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
		}
	}
}

bool FPersonaMeshDetails::OnCanCopyMaterialList() const
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		return Mesh->Materials.Num() > 0;
	}

	return false;
}

void FPersonaMeshDetails::OnPasteMaterialList()
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		FString PastedText;
		FPlatformApplicationMisc::ClipboardPaste(PastedText);

		TSharedPtr<FJsonValue> RootJsonValue;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PastedText);
		FJsonSerializer::Deserialize(Reader, RootJsonValue);

		if (RootJsonValue.IsValid())
		{
			UProperty* Property = USkeletalMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(USkeletalMesh, Materials));

			Mesh->PreEditChange(Property);
			FScopedTransaction Transaction(LOCTEXT("PersonaChangedPasteMaterialList", "Persona editor: Pasted material list"));
			Mesh->Modify();
			TArray<FSkeletalMaterial> TempMaterials;
			FJsonObjectConverter::JsonValueToUProperty(RootJsonValue, Property, &TempMaterials, 0, 0);
			//Do not change the number of material in the array
			for (int32 MaterialIndex = 0; MaterialIndex < TempMaterials.Num(); ++MaterialIndex)
			{
				if (Mesh->Materials.IsValidIndex(MaterialIndex))
				{
					Mesh->Materials[MaterialIndex].MaterialInterface = TempMaterials[MaterialIndex].MaterialInterface;
				}
			}
			

			Mesh->PostEditChange();
		}
	}
}

void FPersonaMeshDetails::OnCopyMaterialItem(int32 CurrentSlot)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		TSharedRef<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject());

		if (Mesh->Materials.IsValidIndex(CurrentSlot))
		{
			const FSkeletalMaterial& Material = Mesh->Materials[CurrentSlot];

			FJsonObjectConverter::UStructToJsonObject(FSkeletalMaterial::StaticStruct(), &Material, RootJsonObject, 0, 0);
		}

		typedef TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriter;
		typedef TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>> FStringWriterFactory;

		FString CopyStr;
		TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(&CopyStr);
		FJsonSerializer::Serialize(RootJsonObject, Writer);

		if (!CopyStr.IsEmpty())
		{
			FPlatformApplicationMisc::ClipboardCopy(*CopyStr);
		}
	}
}

bool FPersonaMeshDetails::OnCanCopyMaterialItem(int32 CurrentSlot) const
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		return Mesh->Materials.IsValidIndex(CurrentSlot);
	}

	return false;
}

void FPersonaMeshDetails::OnPasteMaterialItem(int32 CurrentSlot)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh != nullptr)
	{
		FString PastedText;
		FPlatformApplicationMisc::ClipboardPaste(PastedText);

		TSharedPtr<FJsonObject> RootJsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PastedText);
		FJsonSerializer::Deserialize(Reader, RootJsonObject);

		if (RootJsonObject.IsValid())
		{
			Mesh->PreEditChange(USkeletalMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(USkeletalMesh, Materials)));
			FScopedTransaction Transaction(LOCTEXT("PersonaChangedPasteMaterialItem", "Persona editor: Pasted material item"));
			Mesh->Modify();

			if (Mesh->Materials.IsValidIndex(CurrentSlot))
			{
				FSkeletalMaterial TmpSkeletalMaterial;
				FJsonObjectConverter::JsonObjectToUStruct(RootJsonObject.ToSharedRef(), FSkeletalMaterial::StaticStruct(), &TmpSkeletalMaterial, 0, 0);
				Mesh->Materials[CurrentSlot].MaterialInterface = TmpSkeletalMaterial.MaterialInterface;
			}

			Mesh->PostEditChange();
		}
	}
}

void FPersonaMeshDetails::AddLODLevelCategories(IDetailLayoutBuilder& DetailLayout)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	if (SkelMesh)
	{
		const int32 SkelMeshLODCount = SkelMesh->GetLODNum();


#if WITH_APEX_CLOTHING
		ClothComboBoxes.Reset();
#endif

		//Create material list panel to let users control the materials array
		{
			FString MaterialCategoryName = FString(TEXT("Material Slots"));
			IDetailCategoryBuilder& MaterialCategory = DetailLayout.EditCategory(*MaterialCategoryName, FText::GetEmpty(), ECategoryPriority::Important);
			MaterialCategory.AddCustomRow(LOCTEXT("AddLODLevelCategories_MaterialArrayOperationAdd", "Materials Operation Add Material Slot"))
				.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FPersonaMeshDetails::OnCopyMaterialList), FCanExecuteAction::CreateSP(this, &FPersonaMeshDetails::OnCanCopyMaterialList)))
				.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FPersonaMeshDetails::OnPasteMaterialList)))
				.NameContent()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("AddLODLevelCategories_MaterialArrayOperations", "Material Slots"))
				]
				.ValueContent()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text(this, &FPersonaMeshDetails::GetMaterialArrayText)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Padding(2.0f, 1.0f)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
							.Text(LOCTEXT("AddLODLevelCategories_MaterialArrayOpAdd", "Add Material Slot"))
							.ToolTipText(LOCTEXT("AddLODLevelCategories_MaterialArrayOpAdd_Tooltip", "Add Material Slot at the end of the Material slot array. Those Material slots can be used to override a LODs section, (not the base LOD)"))
							.ContentPadding(4.0f)
							.ForegroundColor(FSlateColor::UseForeground())
							.OnClicked(this, &FPersonaMeshDetails::AddMaterialSlot)
							.IsEnabled(true)
							.IsFocusable(false)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("PropertyWindow.Button_AddToArray"))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
					]
				];
			{
				FMaterialListDelegates MaterialListDelegates;

				MaterialListDelegates.OnGetMaterials.BindSP(this, &FPersonaMeshDetails::OnGetMaterialsForArray, 0);
				MaterialListDelegates.OnMaterialChanged.BindSP(this, &FPersonaMeshDetails::OnMaterialArrayChanged, 0);
				MaterialListDelegates.OnGenerateCustomNameWidgets.BindSP(this, &FPersonaMeshDetails::OnGenerateCustomNameWidgetsForMaterialArray);
				MaterialListDelegates.OnGenerateCustomMaterialWidgets.BindSP(this, &FPersonaMeshDetails::OnGenerateCustomMaterialWidgetsForMaterialArray, 0);
				MaterialListDelegates.OnMaterialListDirty.BindSP(this, &FPersonaMeshDetails::OnMaterialListDirty);

				MaterialListDelegates.OnCopyMaterialItem.BindSP(this, &FPersonaMeshDetails::OnCopyMaterialItem);
				MaterialListDelegates.OnCanCopyMaterialItem.BindSP(this, &FPersonaMeshDetails::OnCanCopyMaterialItem);
				MaterialListDelegates.OnPasteMaterialItem.BindSP(this, &FPersonaMeshDetails::OnPasteMaterialItem);

				MaterialCategory.AddCustomBuilder(MakeShareable(new FMaterialList(MaterialCategory.GetParentLayout(), MaterialListDelegates, false, true, true)));
			}
		}

		int32 CurrentLodIndex = 0;
		if (GetPersonaToolkit()->GetPreviewMeshComponent() != nullptr)
		{
			CurrentLodIndex = GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel;
		}

		FString LODControllerCategoryName = FString(TEXT("LODCustomMode"));
		FText LODControllerString = LOCTEXT("LODCustomModeCategoryName", "LOD Picker");

		IDetailCategoryBuilder& LODCustomModeCategory = DetailLayout.EditCategory(*LODControllerCategoryName, LODControllerString, ECategoryPriority::Important);
		LodCustomCategory = &LODCustomModeCategory;

		LODCustomModeCategory.AddCustomRow((LOCTEXT("LODCustomModeSelect", "Select LOD")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LODCustomModeSelectTitle", "LOD"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsEnabled(this, &FPersonaMeshDetails::IsLodComboBoxEnabledForLodPicker)
		]
		.ValueContent()
		[
			OnGenerateLodComboBoxForLodPicker()
		];

		LODCustomModeCategory.AddCustomRow((LOCTEXT("LODCustomModeFirstRowName", "LODCustomMode")))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FPersonaMeshDetails::GetLODCustomModeNameContent, (int32)INDEX_NONE)
			.ToolTipText(LOCTEXT("LODCustomModeFirstRowTooltip", "Custom Mode allow editing multiple LOD in same time."))
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FPersonaMeshDetails::IsLODCustomModeCheck, (int32)INDEX_NONE)
			.OnCheckStateChanged(this, &FPersonaMeshDetails::SetLODCustomModeCheck, (int32)INDEX_NONE)
			.ToolTipText(LOCTEXT("LODCustomModeFirstRowTooltip", "Custom Mode allow editing multiple LOD in same time."))
		];

		LodCategories.Empty(SkelMeshLODCount);
		DetailDisplayLODs.Reset();
		// Create information panel for each LOD level.
		for (int32 LODIndex = 0; LODIndex < SkelMeshLODCount; ++LODIndex)
		{
			TAttribute<bool> EnabledAttrib = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPersonaMeshDetails::IsLODInfoEditingEnabled, LODIndex));
			//Show the viewport LOD at start
			bool IsViewportLOD = (CurrentLodIndex == 0 ? 0 : CurrentLodIndex - 1) == LODIndex;
			DetailDisplayLODs.Add(true); //Enable all LOD in custum mode
			LODCustomModeCategory.AddCustomRow(( LOCTEXT("LODCustomModeRowName", "LODCheckBoxRowName")), true)
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(this, &FPersonaMeshDetails::GetLODCustomModeNameContent, LODIndex)
				.IsEnabled(this, &FPersonaMeshDetails::IsLODCustomModeEnable, LODIndex)
			]
			.ValueContent()
			[
				SNew(SCheckBox)
				.IsChecked(this, &FPersonaMeshDetails::IsLODCustomModeCheck, LODIndex)
				.OnCheckStateChanged(this, &FPersonaMeshDetails::SetLODCustomModeCheck, LODIndex)
				.IsEnabled(this, &FPersonaMeshDetails::IsLODCustomModeEnable, LODIndex)
			];

			TSharedRef<IPropertyHandle> LODInfoProperty = DetailLayout.GetProperty(FName("LODInfo"), USkeletalMesh::StaticClass());
			uint32 NumChildren = 0;
			LODInfoProperty->GetNumChildren(NumChildren);
			check(NumChildren >(uint32)LODIndex);
			TSharedPtr<IPropertyHandle> ChildHandle = LODInfoProperty->GetChildHandle(LODIndex);
			check(ChildHandle.IsValid());

			FString CategoryName = FString(TEXT("LOD"));
			CategoryName.AppendInt(LODIndex);

			FText LODLevelString = FText::FromString(FString(TEXT("LOD ")) + FString::FromInt(LODIndex) );

			IDetailCategoryBuilder& LODCategory = DetailLayout.EditCategory(*CategoryName, LODLevelString, ECategoryPriority::Important);
			LodCategories.Add(&LODCategory);
			TSharedRef<SWidget> LODCategoryWidget =

				SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text_Raw(this, &FPersonaMeshDetails::GetLODImportedText, LODIndex)
					.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				];

			// want to make sure if this data has imported or not
			LODCategory.HeaderContent(LODCategoryWidget);
			{
				FSectionListDelegates SectionListDelegates;

				SectionListDelegates.OnGetSections.BindSP(this, &FPersonaMeshDetails::OnGetSectionsForView, LODIndex);
				SectionListDelegates.OnSectionChanged.BindSP(this, &FPersonaMeshDetails::OnSectionChanged);
				SectionListDelegates.OnGenerateCustomNameWidgets.BindSP(this, &FPersonaMeshDetails::OnGenerateCustomNameWidgetsForSection);
				SectionListDelegates.OnGenerateCustomSectionWidgets.BindSP(this, &FPersonaMeshDetails::OnGenerateCustomSectionWidgetsForSection);

				SectionListDelegates.OnCopySectionList.BindSP(this, &FPersonaMeshDetails::OnCopySectionList, LODIndex);
				SectionListDelegates.OnCanCopySectionList.BindSP(this, &FPersonaMeshDetails::OnCanCopySectionList, LODIndex);
				SectionListDelegates.OnPasteSectionList.BindSP(this, &FPersonaMeshDetails::OnPasteSectionList, LODIndex);
				SectionListDelegates.OnCopySectionItem.BindSP(this, &FPersonaMeshDetails::OnCopySectionItem);
				SectionListDelegates.OnCanCopySectionItem.BindSP(this, &FPersonaMeshDetails::OnCanCopySectionItem);
				SectionListDelegates.OnPasteSectionItem.BindSP(this, &FPersonaMeshDetails::OnPasteSectionItem);
				SectionListDelegates.OnEnableSectionItem.BindSP(this, &FPersonaMeshDetails::OnSectionEnabledChanged);

				FName SkeletalMeshSectionListName = FName(*(FString(TEXT("SkeletalMeshSectionListNameLOD_")) + FString::FromInt(LODIndex)));
				LODCategory.AddCustomBuilder(MakeShareable(new FSectionList(LODCategory.GetParentLayout(), SectionListDelegates, false, 64, LODIndex, SkeletalMeshSectionListName)));

				GetPersonaToolkit()->GetPreviewScene()->RegisterOnSelectedLODChanged(FOnSelectedLODChanged::CreateSP(this, &FPersonaMeshDetails::UpdateLODCategoryVisibility));
			}

			if (LODInfoProperty->IsValidHandle())
			{
				// changing property name to "LOD Info" because it shows only array index
				TSharedPtr<IPropertyHandle> LODInfoChild = LODInfoProperty->GetChildHandle(LODIndex);
				uint32 NumInfoChildren = 0;
				LODInfoChild->GetNumChildren(NumInfoChildren);

				IDetailGroup& LODInfoGroup = LODCategory.AddGroup(TEXT("LOD Info"), LOCTEXT("LODInfoGroupLabel", "LOD Info"));

				// enable/disable handler - because we want to make sure not editable if LOD sharing is on
				TSharedPtr<IPropertyHandle> ScreenSizeHandle = LODInfoChild->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, ScreenSize));
				IDetailPropertyRow& ScreenSizeRow = LODInfoGroup.AddPropertyRow(ScreenSizeHandle->AsShared());
				ScreenSizeRow.IsEnabled(EnabledAttrib);
				DetailLayout.HideProperty(ScreenSizeHandle);

				TSharedPtr<IPropertyHandle> LODHysteresisHandle = LODInfoChild->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, LODHysteresis));
				IDetailPropertyRow& LODHysteresisRow = LODInfoGroup.AddPropertyRow(LODHysteresisHandle->AsShared());
				LODHysteresisRow.IsEnabled(EnabledAttrib);
				DetailLayout.HideProperty(LODHysteresisHandle);

				const TArray<FName> HiddenProperties = { GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, ReductionSettings), GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, BakePose), GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, BonesToRemove),
					GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, ScreenSize), GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, LODHysteresis) };
				for (uint32 ChildIndex = 0; ChildIndex < NumInfoChildren; ++ChildIndex)
				{
					TSharedRef<IPropertyHandle> LODInfoChildHandle = LODInfoChild->GetChildHandle(ChildIndex).ToSharedRef();
					if (!HiddenProperties.Contains(LODInfoChildHandle->GetProperty()->GetFName()))
					{
						LODInfoGroup.AddPropertyRow(LODInfoChildHandle);
					}
				}

				TSharedPtr<IPropertyHandle> BakePoseHandle = ChildHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, BakePose));
				DetailLayout.HideProperty(BakePoseHandle);
				LODInfoGroup.AddWidgetRow()
				.NameContent()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("BakePoseTitle", "Bake Pose"))
				]
				.ValueContent()
				[
					SNew(SObjectPropertyEntryBox)
					.PropertyHandle(BakePoseHandle)
					.AllowedClass(UAnimSequence::StaticClass())
					.OnShouldFilterAsset(this, &FPersonaMeshDetails::FilterOutBakePose, SkelMesh->Skeleton)
				];

				TSharedPtr<IPropertyHandle> RemovedBonesHandle = LODInfoChild->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, BonesToRemove));
				IDetailPropertyRow& RemoveBonesRow = LODInfoGroup.AddPropertyRow(RemovedBonesHandle->AsShared());
				RemoveBonesRow.IsEnabled(EnabledAttrib);
				RemovedBonesHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this, LODIndex, SkelMesh]()
				{
					if (SkelMesh->GetLODInfo(LODIndex)->BonesToRemove.Num() == 0)
					{
						RemoveBones(LODIndex);
					}
				}));

				//@Todo : ideally this should be inside of LODinfo customization, but for now this will allow users to re-apply removed joints after re-import if they want to.
				// this also can be buggy if you have this opened and you removed joints using skeleton tree, in that case, it might not show			
				// add custom button to re-apply bone reduction if they want to
				FDetailWidgetRow& ButtonRow = LODInfoGroup.AddWidgetRow()
				.ValueContent()
				.HAlign(HAlign_Left)			
				[
					SNew(SButton)
					.OnClicked(this, &FPersonaMeshDetails::RemoveBones, LODIndex)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ReapplyRemovedBonesButton", "Reapply removed bones"))
						.Font(DetailLayout.GetDetailFont())
					]
				];

				ButtonRow.Visibility(TAttribute<EVisibility>::Create([SkelMesh, LODIndex]() -> EVisibility { return (SkelMesh->GetLODInfo(LODIndex)->BonesToRemove.Num() > 0) ? EVisibility::Visible : EVisibility::Collapsed;  }));

				// Add reduction settings
				if (LODIndex > 0)
				{
					if (IsAutoMeshReductionAvailable())
					{
					
						TSharedPtr<IPropertyHandle> ReductionHandle = LODInfoChild->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSkeletalMeshLODInfo, ReductionSettings));
						check(ReductionHandle->IsValidHandle());
						IDetailPropertyRow& ReductionHandleRow = LODInfoGroup.AddPropertyRow(ReductionHandle->AsShared());
						ReductionHandleRow.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPersonaMeshDetails::IsLODInfoEditingEnabled, LODIndex)));
						
						LODCategory.AddCustomRow(LOCTEXT("RemoveLODRow", "Remove LOD"))
							.ValueContent()
							.HAlign(HAlign_Fill)
							[
								SNew(SSkeletalLODActions)
								.LODIndex(LODIndex)
								.PersonaToolkit(GetPersonaToolkit())
								.OnApplyLODChangeClicked(this, &FPersonaMeshDetails::RegenerateLOD, LODIndex)
								.OnRemoveLODClicked(this, &FPersonaMeshDetails::RemoveOneLOD, LODIndex)
								.OnReimportClicked(this, &FPersonaMeshDetails::OnReimportLodClicked, &DetailLayout, EReimportButtonType::Reimport, LODIndex)
								.OnReimportNewFileClicked(this, &FPersonaMeshDetails::OnReimportLodClicked, &DetailLayout, EReimportButtonType::ReimportWithNewFile, LODIndex)
							];						
					}
					else
					{
						LODCategory.AddCustomRow(LOCTEXT("RemoveLODRow", "Remove LOD"))
							.ValueContent()
							.HAlign(HAlign_Fill)
							[
								SNew(SSkeletalLODActions)
								.LODIndex(LODIndex)
								.PersonaToolkit(GetPersonaToolkit())
								.OnRemoveLODClicked(this, &FPersonaMeshDetails::RemoveOneLOD, LODIndex)
								.OnReimportClicked(this, &FPersonaMeshDetails::OnReimportLodClicked, &DetailLayout, EReimportButtonType::Reimport, LODIndex)
								.OnReimportNewFileClicked(this, &FPersonaMeshDetails::OnReimportLodClicked, &DetailLayout, EReimportButtonType::ReimportWithNewFile, LODIndex)
							];
					}
				}
			}

			LODCategory.SetCategoryVisibility(IsViewportLOD);
		}

		//Show the LOD custom category 
		if (SkelMeshLODCount > 1)
		{
			LODCustomModeCategory.SetCategoryVisibility(true);
			LODCustomModeCategory.SetShowAdvanced(false);
		}

		//Restore the state of the custom check LOD
		for (int32 DetailLODIndex = 0; DetailLODIndex < SkelMeshLODCount; ++DetailLODIndex)
		{
			int32 LodCheckValue = GetPersonaToolkit()->GetCustomData(CustomDataKey_LODVisibilityState + DetailLODIndex);
			if (LodCheckValue != INDEX_NONE && DetailDisplayLODs.IsValidIndex(DetailLODIndex))
			{
				DetailDisplayLODs[DetailLODIndex] = LodCheckValue > 0;
			}
		}

		//Restore the state of the custom LOD mode if its true (greater then 0)
		bool bCustomLodEditMode = GetPersonaToolkit()->GetCustomData(CustomDataKey_LODEditMode) > 0;
		if (bCustomLodEditMode)
		{
			for (int32 DetailLODIndex = 0; DetailLODIndex < SkelMeshLODCount; ++DetailLODIndex)
			{
				if (!LodCategories.IsValidIndex(DetailLODIndex))
				{
					break;
				}
				LodCategories[DetailLODIndex]->SetCategoryVisibility(DetailDisplayLODs[DetailLODIndex]);
			}
		}

		if (LodCustomCategory != nullptr)
		{
			LodCustomCategory->SetShowAdvanced(bCustomLodEditMode);
		}
	}
}

FText FPersonaMeshDetails::GetLODCustomModeNameContent(int32 LODIndex) const
{
	int32 CurrentLodIndex = 0;
	if (GetPersonaToolkit()->GetPreviewMeshComponent() != nullptr)
	{
		CurrentLodIndex = GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel;
	}
	int32 RealCurrentLODIndex = (CurrentLodIndex == 0 ? 0 : CurrentLodIndex - 1);
	if (LODIndex == INDEX_NONE)
	{
		return LOCTEXT("GetLODCustomModeNameContent_None", "Custom");
	}
	return FText::Format(LOCTEXT("GetLODCustomModeNameContent", "LOD{0}"), LODIndex);
}

ECheckBoxState FPersonaMeshDetails::IsLODCustomModeCheck(int32 LODIndex) const
{
	int32 CurrentLodIndex = 0;
	if (GetPersonaToolkit()->GetPreviewMeshComponent() != nullptr)
	{
		CurrentLodIndex = GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel;
	}
	if (LODIndex == INDEX_NONE)
	{
		return (GetPersonaToolkit()->GetCustomData(CustomDataKey_LODEditMode) > 0) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return DetailDisplayLODs[LODIndex] ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FPersonaMeshDetails::SetLODCustomModeCheck(ECheckBoxState NewState, int32 LODIndex)
{
	int32 CurrentLodIndex = 0;
	if (GetPersonaToolkit()->GetPreviewMeshComponent() != nullptr)
	{
		CurrentLodIndex = GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel;
	}
	if (LODIndex == INDEX_NONE)
	{
		if (NewState == ECheckBoxState::Unchecked)
		{
			GetPersonaToolkit()->SetCustomData(CustomDataKey_LODEditMode, 0);
			SetCurrentLOD(CurrentLodIndex);
			for (int32 DetailLODIndex = 0; DetailLODIndex < LODCount; ++DetailLODIndex)
			{
				if (!LodCategories.IsValidIndex(DetailLODIndex))
				{
					break;
				}
				LodCategories[DetailLODIndex]->SetCategoryVisibility(DetailLODIndex == (CurrentLodIndex == 0 ? 0 : CurrentLodIndex - 1));
			}
		}
		else
		{
			GetPersonaToolkit()->SetCustomData(CustomDataKey_LODEditMode, 1);
			SetCurrentLOD(0);
		}
	}
	else if ((GetPersonaToolkit()->GetCustomData(CustomDataKey_LODEditMode) > 0))
	{
		DetailDisplayLODs[LODIndex] = NewState == ECheckBoxState::Checked;
		GetPersonaToolkit()->SetCustomData(CustomDataKey_LODVisibilityState + LODIndex, DetailDisplayLODs[LODIndex] ? 1 : 0);
	}

	if ((GetPersonaToolkit()->GetCustomData(CustomDataKey_LODEditMode) > 0))
	{
		for (int32 DetailLODIndex = 0; DetailLODIndex < LODCount; ++DetailLODIndex)
		{
			if (!LodCategories.IsValidIndex(DetailLODIndex))
			{
				break;
			}
			LodCategories[DetailLODIndex]->SetCategoryVisibility(DetailDisplayLODs[DetailLODIndex]);
		}
	}

	if (LodCustomCategory != nullptr)
	{
		LodCustomCategory->SetShowAdvanced((GetPersonaToolkit()->GetCustomData(CustomDataKey_LODEditMode) > 0));
	}
}

bool FPersonaMeshDetails::IsLODCustomModeEnable(int32 LODIndex) const
{
	if (LODIndex == INDEX_NONE)
	{
		// Custom checkbox is always enable
		return true;
	}
	return (GetPersonaToolkit()->GetCustomData(CustomDataKey_LODEditMode) > 0);
}

TOptional<int32> FPersonaMeshDetails::GetLodSliderMaxValue() const
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	if(SkelMesh)
	{
		return SkelMesh->GetLODNum() + PersonaMeshDetailsConstants::LodSliderExtension;
	}

	return 0;
}

void FPersonaMeshDetails::CustomizeLODSettingsCategories(IDetailLayoutBuilder& DetailLayout)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	LODCount = SkelMesh->GetLODNum();

	UpdateLODNames();

	IDetailCategoryBuilder& LODSettingsCategory = DetailLayout.EditCategory("LodSettings", LOCTEXT("LodSettingsCategory", "LOD Settings"), ECategoryPriority::TypeSpecific);

	TSharedPtr<SWidget> LodTextPtr;

	LODSettingsCategory.AddCustomRow(LOCTEXT("LODImport", "LOD Import"))
	.NameContent()
	[
		SAssignNew(LodTextPtr, STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("LODImport", "LOD Import"))
	]
	.ValueContent()
	[
		SNew(STextComboBox)
		.ContentPadding(0)
		.OptionsSource(&LODNames)
		.InitiallySelectedItem(LODNames[0])
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.OnSelectionChanged(this, &FPersonaMeshDetails::OnImportLOD, &DetailLayout)
	];

	// Add Number of LODs slider.
	const int32 MinAllowedLOD = 1;
	LODSettingsCategory.AddCustomRow(LOCTEXT("NumberOfLODs", "Number of LODs"))
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([]()->EVisibility { return IsAutoMeshReductionAvailable()? EVisibility::Visible : EVisibility::Hidden; })))
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("NumberOfLODs", "Number of LODs"))
	]
	.ValueContent()
	[
		SNew(SSpinBox<int32>)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Value(this, &FPersonaMeshDetails::GetLODCount)
		.OnValueChanged(this, &FPersonaMeshDetails::OnLODCountChanged)
		.OnValueCommitted(this, &FPersonaMeshDetails::OnLODCountCommitted)
		.MinValue(MinAllowedLOD)
		.MaxValue(this, &FPersonaMeshDetails::GetLodSliderMaxValue)
		.ToolTipText(this, &FPersonaMeshDetails::GetLODCountTooltip)
		.IsEnabled(IsAutoMeshReductionAvailable())
	];

	LODSettingsCategory.AddCustomRow(LOCTEXT("ApplyChanges", "Apply Changes"))
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([]()->EVisibility { return IsAutoMeshReductionAvailable() ? EVisibility::Visible : EVisibility::Hidden; })))
	.ValueContent()
	.HAlign(HAlign_Left)
	[
		SNew(SButton)
		.OnClicked(this, &FPersonaMeshDetails::OnApplyChanges)
		.IsEnabled(this, &FPersonaMeshDetails::IsGenerateAvailable)
		[
			SNew(STextBlock)
			.Text(this, &FPersonaMeshDetails::GetApplyButtonText)
			.Font(DetailLayout.GetDetailFont())
		]
	];

	// add lod setting assets
	TSharedPtr<IPropertyHandle> LODSettingAssetPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalMesh, LODSettings), USkeletalMesh::StaticClass());
	DetailLayout.HideProperty(LODSettingAssetPropertyHandle);
	LODSettingsCategory.AddCustomRow(LODSettingAssetPropertyHandle->GetPropertyDisplayName())
	.NameContent()
	[
		LODSettingAssetPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(150)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMeshLODSettings::StaticClass())
			.PropertyHandle(LODSettingAssetPropertyHandle)
			.ThumbnailPool(DetailLayout.GetThumbnailPool())
			.OnObjectChanged(this, &FPersonaMeshDetails::OnLODSettingsSelected)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("GenerateAsset_Tooltip", "Save current LOD info to new or existing asset and use it"))
			.OnClicked(this, &FPersonaMeshDetails::OnSaveLODSettings)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GenerateAsset_Lable", "Generate Asset..."))
				.Font(DetailLayout.GetDetailFont())
			]
		]
	];

	TSharedPtr<IPropertyHandle> MinLODPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalMesh, MinLod), USkeletalMesh::StaticClass());
	IDetailPropertyRow& MinLODRow = LODSettingsCategory.AddProperty(MinLODPropertyHandle);
	MinLODRow.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPersonaMeshDetails::IsLODInfoEditingEnabled, -1)));
	DetailLayout.HideProperty(MinLODPropertyHandle);
}

// save LOD settings
FReply FPersonaMeshDetails::OnSaveLODSettings()
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	if (SkelMesh)
	{
		const FString DefaultPackageName = SkelMesh->GetPathName();
		const FString DefaultPath = FPackageName::GetLongPackagePath(DefaultPackageName);
		const FString DefaultName = SkelMesh->GetName() + TEXT("_LODSettings");

		// Initialize SaveAssetDialog config
		FSaveAssetDialogConfig SaveAssetDialogConfig;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("CreateLODSettings", "Create LOD Settings from existing settings");
		SaveAssetDialogConfig.DefaultPath = DefaultPath;
		SaveAssetDialogConfig.DefaultAssetName = DefaultName;
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.AssetClassNames.Add(USkeletalMeshLODSettings::StaticClass()->GetFName());

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
		if (!SaveObjectPath.IsEmpty())
		{
			const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
			const FString SavePackagePath = FPaths::GetPath(SavePackageName);
			const FString SaveAssetName = FPaths::GetBaseFilename(SavePackageName);

			// create package and create object
			UPackage* Package = CreatePackage(nullptr, *SavePackageName);
			USkeletalMeshLODSettings* NewLODSettingAsset = NewObject<USkeletalMeshLODSettings>(Package, *SaveAssetName, RF_Public | RF_Standalone);
			if (NewLODSettingAsset && SkelMesh->GetLODNum() > 0)
			{
				// update mapping information on the class
				NewLODSettingAsset->SetLODSettingsFromMesh(SkelMesh);

				// save mapper class
				FString const PackageName = Package->GetName();
				FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

				UPackage::SavePackage(Package, NULL, RF_Standalone, *PackageFileName, GError, nullptr, false, true, SAVE_NoError);

				// set the property back to SkelMesh;
				SkelMesh->LODSettings = NewLODSettingAsset;
			}
		}
	}

	return FReply::Handled();
}

void FPersonaMeshDetails::OnLODSettingsSelected(const FAssetData& AssetData)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	if (SkelMesh)
	{
		USkeletalMeshLODSettings* SelectedSettingsAsset = Cast<USkeletalMeshLODSettings>(AssetData.GetAsset());
		if (SelectedSettingsAsset)
		{
			SelectedSettingsAsset->SetLODSettingsToMesh(SkelMesh);
		}
	}
}

bool FPersonaMeshDetails::IsLODInfoEditingEnabled(int32 LODIndex) const
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	if (SkelMesh)
	{
		if (SkelMesh->LODSettings)
		{
			// if LODIndex == -1, we don't care about lod index
			if (LODIndex == -1)
			{
				return false;
			}

			if (SkelMesh->LODSettings->GetNumberOfSettings() > LODIndex)
			{
				return false;
			}
		}
	}

	return true;
}

void FPersonaMeshDetails::OnImportLOD(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, IDetailLayoutBuilder* DetailLayout)
{
	int32 LODIndex = 0;
	if (LODNames.Find(NewValue, LODIndex) && LODIndex > 0)
	{
		USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
		check(SkelMesh);

		FbxMeshUtils::ImportMeshLODDialog(SkelMesh, LODIndex);

		DetailLayout->ForceRefreshDetails();
	}
}

int32 FPersonaMeshDetails::GetLODCount() const
{
	return LODCount;
}

void FPersonaMeshDetails::OnLODCountChanged(int32 NewValue)
{
	LODCount = FMath::Max<int32>(NewValue, 1);

	UpdateLODNames();
}

void FPersonaMeshDetails::OnLODCountCommitted(int32 InValue, ETextCommit::Type CommitInfo)
{
	OnLODCountChanged(InValue);
}

FReply FPersonaMeshDetails::OnApplyChanges()
{
	ApplyChanges();
	return FReply::Handled();
}

FReply FPersonaMeshDetails::RegenerateLOD(int32 LODIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);

	FSkeletalMeshUpdateContext UpdateContext;
	UpdateContext.SkeletalMesh = SkelMesh;
	UpdateContext.AssociatedComponents.Push(GetPersonaToolkit()->GetPreviewMeshComponent());

	if (SkelMesh->IsValidLODIndex(LODIndex))
	{
		if (SkelMesh->GetLODInfo(LODIndex)->bHasBeenSimplified == false)
		{
			const FText Text = FText::Format(LOCTEXT("Warning_SimplygonApplyingToImportedMesh", "LOD {0} has been imported. Are you sure you'd like to apply mesh reduction? This will destroy imported LOD."), FText::AsNumber(LODIndex));
			EAppReturnType::Type Ret = FMessageDialog::Open(EAppMsgType::YesNo, Text);
			if (Ret == EAppReturnType::No)
			{
				return FReply::Handled();
			}
		}

		FLODUtilities::SimplifySkeletalMeshLOD(UpdateContext, LODIndex);
	}

	return FReply::Handled();
}

FReply FPersonaMeshDetails::RemoveOneLOD(int32 LODIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);
	check(SkelMesh->IsValidLODIndex(LODIndex));

	if (LODIndex > 0)
	{
		FText ConfirmRemoveLODText = FText::Format( LOCTEXT("PersonaRemoveLOD_Confirmation", "Are you sure you want to remove LOD {0} from {1}?"), LODIndex, FText::FromString(SkelMesh->GetName()) );

		// if we have lod settings, and then 
		if (SkelMesh->LODSettings != nullptr)
		{
			// if I have more LODs, and if LODSettings will be copied back over, 
			// all LODs have to be regenerated
			// warn users about it
			if (SkelMesh->IsValidLODIndex(LODIndex + 1) && SkelMesh->LODSettings->GetNumberOfSettings() > LODIndex)
			{
				// now the information will get copied over after removing this LOD
				ConfirmRemoveLODText = FText::Format(LOCTEXT("PersonaRemoveLODOverriding_Confirmation", 
					"You're currently using LOD Setting Asset \'{2}\' that will override the next LODs with current setting. This will require to regenerate the next LODs after removing this LOD. If you do not want this, clear the LOD Setting Asset before removing LODs. \n\n Are you sure you want to remove LOD {0} from {1}?"), LODIndex, FText::FromString(SkelMesh->GetName()), FText::FromString(SkelMesh->LODSettings->GetName()));
			}
		}

		if ( FMessageDialog::Open(EAppMsgType::YesNo, ConfirmRemoveLODText) == EAppReturnType::Yes )
		{
			FText RemoveLODText = FText::Format( LOCTEXT("OnPersonaRemoveLOD", "Persona editor: Remove LOD {0}"), LODIndex );
			FScopedTransaction Transaction( TEXT(""), RemoveLODText, SkelMesh );
			SkelMesh->Modify();
			FSkeletalMeshUpdateContext UpdateContext; 
			UpdateContext.SkeletalMesh = SkelMesh;
			UpdateContext.AssociatedComponents.Push(GetPersonaToolkit()->GetPreviewMeshComponent());

			FLODUtilities::RemoveLOD(UpdateContext, LODIndex);

			if (SkelMesh->LODSettings)
			{
				SkelMesh->LODSettings->SetLODSettingsToMesh(SkelMesh);
			}

			SkelMesh->PostEditChange();

			MeshDetailLayout->ForceRefreshDetails();
		}
	}
	return FReply::Handled();
}

FReply FPersonaMeshDetails::RemoveBones(int32 LODIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);
	check(SkelMesh->IsValidLODIndex(LODIndex));

	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	MeshUtilities.RemoveBonesFromMesh(SkelMesh, LODIndex, NULL);

	MeshDetailLayout->ForceRefreshDetails();
	
	GetPersonaToolkit()->GetEditableSkeleton()->RefreshBoneTree();

	return FReply::Handled();
}

FText FPersonaMeshDetails::GetApplyButtonText() const
{
	if (IsApplyNeeded())	
	{
		return LOCTEXT("ApplyChanges", "Apply Changes");
	}
	else if (IsGenerateAvailable())
	{
		return LOCTEXT("Regenerate", "Regenerate");
	}

	return LOCTEXT("ApplyChanges", "Apply Changes");
}

void FPersonaMeshDetails::ApplyChanges()
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);

	// see if there is 
	bool bRegenerateEvenIfImported = false;
	int32 CurrentNumLODs = SkelMesh->GetLODNum();
	if (CurrentNumLODs == LODCount)
	{
		bool bImportedLODs = false;
		// check if anything is imported and ask if users wants to still regenerate it
		for (int32 LODIdx = 1; LODIdx < LODCount; LODIdx++)
		{
			FSkeletalMeshLODInfo& CurrentLODInfo = *(SkelMesh->GetLODInfo(LODIdx));
			if (CurrentLODInfo.bHasBeenSimplified == false)
			{
				bImportedLODs = true;
			}
		}

		// if LOD is imported, ask users if they want to regenerate or just leave it
		if (bImportedLODs)
		{
			const FText Text(LOCTEXT("Warning_SimplygonApplyingToImportedMesh_All", "Some LODs have been imported. Are you sure you'd like to apply mesh reduction to all LODs? This will destroy imported LODs."));
			EAppReturnType::Type Ret = FMessageDialog::Open(EAppMsgType::YesNo, Text);
			if (Ret == EAppReturnType::Yes)
			{
				bRegenerateEvenIfImported = true;
			}
		}
	}

	FLODUtilities::RegenerateLOD(SkelMesh, LODCount, bRegenerateEvenIfImported);

	MeshDetailLayout->ForceRefreshDetails();
}

void FPersonaMeshDetails::UpdateLODNames()
{
	LODNames.Empty();
	LODNames.Add(MakeShareable(new FString(LOCTEXT("BaseLOD", "Base LOD").ToString())));
	for (int32 LODLevelID = 1; LODLevelID < LODCount; ++LODLevelID)
	{
		LODNames.Add(MakeShareable(new FString(FText::Format(NSLOCTEXT("LODSettingsLayout", "LODLevel_Reimport", "Reimport LOD Level {0}"), FText::AsNumber(LODLevelID)).ToString())));
	}
	LODNames.Add(MakeShareable(new FString(FText::Format(NSLOCTEXT("LODSettingsLayout", "LODLevel_Import", "Import LOD Level {0}"), FText::AsNumber(LODCount)).ToString())));
}

bool FPersonaMeshDetails::IsGenerateAvailable() const
{
	return IsAutoMeshReductionAvailable() && (IsApplyNeeded() || (LODCount > 1));
}
bool FPersonaMeshDetails::IsApplyNeeded() const
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);

	if (SkelMesh->GetLODNum() != LODCount)
	{
		return true;
	}

	return false;
}

FText FPersonaMeshDetails::GetLODCountTooltip() const
{
	if (IsAutoMeshReductionAvailable())
	{
		return LOCTEXT("LODCountTooltip", "The number of LODs for this skeletal mesh. If auto mesh reduction is available, setting this number will determine the number of LOD levels to auto generate.");
	}

	return LOCTEXT("LODCountTooltip_Disabled", "Auto mesh reduction is unavailable! Please provide a mesh reduction interface such as Simplygon to use this feature or manually import LOD levels.");
}

FText FPersonaMeshDetails::GetLODImportedText(int32 LODIndex) const
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh && Mesh->IsValidLODIndex(LODIndex))
	{
		if (Mesh->GetLODInfo(LODIndex)->bHasBeenSimplified)
		{
			return  LOCTEXT("LODMeshReductionText_Label", "[generated]");
		}
	}

	return FText();
}

FText FPersonaMeshDetails::GetMaterialSlotNameText(int32 MaterialIndex) const
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh && Mesh->Materials.IsValidIndex(MaterialIndex))
	{
		return FText::FromName(Mesh->Materials[MaterialIndex].MaterialSlotName);
	}

	return LOCTEXT("SkeletalMeshMaterial_InvalidIndex", "Invalid Material Index");
}

void FPersonaMeshDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();
	check(SelectedObjects.Num()<=1); // The OnGenerateCustomWidgets delegate will not be useful if we try to process more than one object.

	TSharedRef<IPersonaPreviewScene> PreviewScene = GetPersonaToolkit()->GetPreviewScene();

	// Ensure that we only have one callback for this object registered
	PreviewScene->RegisterOnPreviewMeshChanged(FOnPreviewMeshChanged::CreateSP(this, &FPersonaMeshDetails::OnPreviewMeshChanged));

	SkeletalMeshPtr = SelectedObjects.Num() > 0 ? Cast<USkeletalMesh>(SelectedObjects[0].Get()) : nullptr;

	// copy temporarily to refresh Mesh details tab from the LOD settings window
	MeshDetailLayout = &DetailLayout;

	// add multiple LOD levels to LOD category
	AddLODLevelCategories(DetailLayout);

	CustomizeLODSettingsCategories(DetailLayout);

	IDetailCategoryBuilder& ClothingCategory = DetailLayout.EditCategory("Clothing", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	CustomizeClothingProperties(DetailLayout,ClothingCategory);

	// Post process selector
	IDetailCategoryBuilder& SkelMeshCategory = DetailLayout.EditCategory("SkeletalMesh");
	TSharedRef<IPropertyHandle> PostProcessHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalMesh, PostProcessAnimBlueprint), USkeletalMesh::StaticClass());
	PostProcessHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPersonaMeshDetails::OnPostProcessBlueprintChanged, &DetailLayout));
	PostProcessHandle->MarkHiddenByCustomization();

	FDetailWidgetRow& PostProcessRow = SkelMeshCategory.AddCustomRow(LOCTEXT("PostProcessFilterString", "Post Process Blueprint"));
	PostProcessRow.NameContent()
		[
			PostProcessHandle->CreatePropertyNameWidget()
		];

	PostProcessRow.ValueContent()
	[
			SNew(SObjectPropertyEntryBox)
			.ObjectPath(this, &FPersonaMeshDetails::GetCurrentPostProcessBlueprintPath)
			.AllowedClass(UAnimBlueprint::StaticClass())
			.NewAssetFactories(TArray<UFactory*>())
			.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FPersonaMeshDetails::OnShouldFilterPostProcessBlueprint))
			.OnObjectChanged(FOnSetObject::CreateSP(this, &FPersonaMeshDetails::OnSetPostProcessBlueprint, PostProcessHandle))
	];

	// Hide the ability to change the import settings object
	IDetailCategoryBuilder& ImportSettingsCategory = DetailLayout.EditCategory("ImportSettings");
	TSharedRef<IPropertyHandle> AssetImportProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalMesh, AssetImportData), USkeletalMesh::StaticClass());
	IDetailPropertyRow& Row = ImportSettingsCategory.AddProperty(AssetImportProperty);
	Row.CustomWidget(true)
		.NameContent()
		[
			AssetImportProperty->CreatePropertyNameWidget()
		];

	HideUnnecessaryProperties(DetailLayout);
}

void FPersonaMeshDetails::HideUnnecessaryProperties(IDetailLayoutBuilder& DetailLayout)
{
	// LODInfo doesn't need to be showed anymore because it was moved to each LOD category
	TSharedRef<IPropertyHandle> LODInfoProperty = DetailLayout.GetProperty(FName("LODInfo"), USkeletalMesh::StaticClass());
	DetailLayout.HideProperty(LODInfoProperty);
	uint32 NumChildren = 0;
	LODInfoProperty->GetNumChildren(NumChildren);
	// Hide reduction settings property because it is duplicated with Reduction settings layout created by ReductionSettingsWidgets
	for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ChildIdx++)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = LODInfoProperty->GetChildHandle(ChildIdx);
		if (ChildHandle.IsValid())
		{
			TSharedPtr<IPropertyHandle> ReductionHandle = ChildHandle->GetChildHandle(FName("ReductionSettings"));
			DetailLayout.HideProperty(ReductionHandle);
		}
	}

	TSharedRef<IPropertyHandle> MaterialsProperty = DetailLayout.GetProperty(FName("Materials"), USkeletalMesh::StaticClass());
	DetailLayout.HideProperty(MaterialsProperty);

	// hide all properties in Mirroring category to hide Mirroring category itself
	IDetailCategoryBuilder& MirroringCategory = DetailLayout.EditCategory("Mirroring", FText::GetEmpty(), ECategoryPriority::Default);
	TArray<TSharedRef<IPropertyHandle>> MirroringProperties;
	MirroringCategory.GetDefaultProperties(MirroringProperties);
	for (int32 MirrorPropertyIdx = 0; MirrorPropertyIdx < MirroringProperties.Num(); MirrorPropertyIdx++)
	{
		DetailLayout.HideProperty(MirroringProperties[MirrorPropertyIdx]);
	}
}

void FPersonaMeshDetails::OnPostProcessBlueprintChanged(IDetailLayoutBuilder* DetailBuilder)
{
	DetailBuilder->ForceRefreshDetails();
}

FString FPersonaMeshDetails::GetCurrentPostProcessBlueprintPath() const
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	if(UClass* PostProcessClass = *SkelMesh->PostProcessAnimBlueprint)
	{
		return PostProcessClass->GetPathName();
	}

	return FString();
}

bool FPersonaMeshDetails::OnShouldFilterPostProcessBlueprint(const FAssetData& AssetData) const
{
	if(USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh())
	{
		const FString CurrentMeshSkeletonName = FString::Printf(TEXT("%s'%s'"), *SkelMesh->Skeleton->GetClass()->GetName(), *SkelMesh->Skeleton->GetPathName());
		const FString SkeletonName = AssetData.GetTagValueRef<FString>("TargetSkeleton");

		return SkeletonName != CurrentMeshSkeletonName;
	}

	return true;
}

void FPersonaMeshDetails::OnSetPostProcessBlueprint(const FAssetData& AssetData, TSharedRef<IPropertyHandle> BlueprintProperty)
{
	if(UAnimBlueprint* SelectedBlueprint = Cast<UAnimBlueprint>(AssetData.GetAsset()))
	{
		BlueprintProperty->SetValue(SelectedBlueprint->GetAnimBlueprintGeneratedClass());
	}
	else if(!AssetData.IsValid())
	{
		// Asset data is not valid so clear the result
		UObject* Value = nullptr;
		BlueprintProperty->SetValue(Value);
	}
}

FReply FPersonaMeshDetails::OnReimportLodClicked(IDetailLayoutBuilder* DetailLayout, EReimportButtonType InReimportType, int32 InLODIndex)
{
	if(USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh())
	{
		if(!SkelMesh->IsValidLODIndex(InLODIndex))
		{
			return FReply::Unhandled();
		}

		FString SourceFilenameBackup("");
		if(InReimportType == EReimportButtonType::ReimportWithNewFile)
		{
			// Back up current source filename and empty it so the importer asks for a new one.
			SourceFilenameBackup = SkelMesh->GetLODInfo(InLODIndex)->SourceImportFilename;
			SkelMesh->GetLODInfo(InLODIndex)->SourceImportFilename.Empty();
		}

		bool bImportSucceeded = FbxMeshUtils::ImportMeshLODDialog(SkelMesh, InLODIndex);

		if(InReimportType == EReimportButtonType::ReimportWithNewFile && !bImportSucceeded)
		{
			// Copy old source file back, as this one failed
			SkelMesh->GetLODInfo(InLODIndex)->SourceImportFilename = SourceFilenameBackup;
		}

		if(DetailLayout)
		{
			DetailLayout->ForceRefreshDetails();
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void FPersonaMeshDetails::OnGetMaterialsForArray(class IMaterialListBuilder& OutMaterials, int32 LODIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	if (!SkelMesh)
		return;

	for (int32 MaterialIndex = 0; MaterialIndex < SkelMesh->Materials.Num(); ++MaterialIndex)
	{
		OutMaterials.AddMaterial(MaterialIndex, SkelMesh->Materials[MaterialIndex].MaterialInterface, true);
	}
}

void FPersonaMeshDetails::OnMaterialArrayChanged(UMaterialInterface* NewMaterial, UMaterialInterface* PrevMaterial, int32 SlotIndex, bool bReplaceAll, int32 LODIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh)
	{
		// Whether or not we made a transaction and need to end it
		bool bMadeTransaction = false;

		UProperty* MaterialProperty = FindField<UProperty>(USkeletalMesh::StaticClass(), "Materials");
		check(MaterialProperty);
		Mesh->PreEditChange(MaterialProperty);
		check(Mesh->Materials.Num() > SlotIndex)

		if (NewMaterial != PrevMaterial)
		{
			GEditor->BeginTransaction(LOCTEXT("PersonaEditorMaterialChanged", "Persona editor: material changed"));
			bMadeTransaction = true;
			Mesh->Modify();
			Mesh->Materials[SlotIndex].MaterialInterface = NewMaterial;

			//Add a default name to the material slot if this slot was manually add and there is no name yet
			if (NewMaterial != nullptr && (Mesh->Materials[SlotIndex].ImportedMaterialSlotName == NAME_None || Mesh->Materials[SlotIndex].MaterialSlotName == NAME_None))
			{
				if (Mesh->Materials[SlotIndex].MaterialSlotName == NAME_None)
				{
					
					Mesh->Materials[SlotIndex].MaterialSlotName = NewMaterial->GetFName();
				}

				//Ensure the imported material slot name is unique
				if (Mesh->Materials[SlotIndex].ImportedMaterialSlotName == NAME_None)
				{
					auto IsMaterialNameUnique = [&Mesh, SlotIndex](const FName TestName)
					{
						for (int32 MaterialIndex = 0; MaterialIndex < Mesh->Materials.Num(); ++MaterialIndex)
						{
							if (MaterialIndex == SlotIndex)
							{
								continue;
							}
							if (Mesh->Materials[MaterialIndex].ImportedMaterialSlotName == TestName)
							{
								return false;
							}
						}
						return true;
					};
					int32 MatchNameCounter = 0;
					//Make sure the name is unique for imported material slot name
					bool bUniqueName = false;
					FString MaterialSlotName = NewMaterial->GetName();
					while (!bUniqueName)
					{
						bUniqueName = true;
						if (!IsMaterialNameUnique(FName(*MaterialSlotName)))
						{
							bUniqueName = false;
							MatchNameCounter++;
							MaterialSlotName = NewMaterial->GetName() + TEXT("_") + FString::FromInt(MatchNameCounter);
						}
					}
					Mesh->Materials[SlotIndex].ImportedMaterialSlotName = FName(*MaterialSlotName);
				}
			}
		}

		FPropertyChangedEvent PropertyChangedEvent(MaterialProperty);
		Mesh->PostEditChangeProperty(PropertyChangedEvent);

		if (bMadeTransaction)
		{
			// End the transation if we created one
			GEditor->EndTransaction();
			// Redraw viewports to reflect the material changes 
			GUnrealEd->RedrawLevelEditingViewports();
		}
	}
}

FReply FPersonaMeshDetails::AddMaterialSlot()
{
	if (!SkeletalMeshPtr.IsValid())
	{
		return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("PersonaAddMaterialSlotTransaction", "Persona editor: Add material slot"));
	SkeletalMeshPtr->Modify();
	SkeletalMeshPtr->Materials.Add(FSkeletalMaterial());

	SkeletalMeshPtr->PostEditChange();

	return FReply::Handled();
}

FText FPersonaMeshDetails::GetMaterialArrayText() const
{
	FString MaterialArrayText = TEXT(" Material Slots");
	int32 SlotNumber = 0;
	if (SkeletalMeshPtr.IsValid())
	{
		SlotNumber = SkeletalMeshPtr->Materials.Num();
	}
	MaterialArrayText = FString::FromInt(SlotNumber) + MaterialArrayText;
	return FText::FromString(MaterialArrayText);
}

void FPersonaMeshDetails::OnGetSectionsForView(ISectionListBuilder& OutSections, int32 LODIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	FSkeletalMeshModel* ImportedResource = SkelMesh->GetImportedModel();

	if (ImportedResource && ImportedResource->LODModels.IsValidIndex(LODIndex))
	{
		FSkeletalMeshLODModel& Model = ImportedResource->LODModels[LODIndex];

		bool bHasMaterialMap = SkelMesh->IsValidLODIndex(LODIndex) && SkelMesh->GetLODInfo(LODIndex)->LODMaterialMap.Num() > 0;

		if (LODIndex == 0 || !bHasMaterialMap)
		{
			int32 NumSections = Model.Sections.Num();
			for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
			{
				int32 MaterialIndex = Model.Sections[SectionIdx].MaterialIndex;

				if (SkelMesh->Materials.IsValidIndex(MaterialIndex))
				{
					FName CurrentSectionMaterialSlotName = SkelMesh->Materials[MaterialIndex].MaterialSlotName;
					FName CurrentSectionOriginalImportedMaterialName = SkelMesh->Materials[MaterialIndex].ImportedMaterialSlotName;
					TMap<int32, FName> AvailableSectionName;
					int32 CurrentIterMaterialIndex = 0;
					for (const FSkeletalMaterial &SkeletalMaterial : SkelMesh->Materials)
					{
						if (MaterialIndex != CurrentIterMaterialIndex)
							AvailableSectionName.Add(CurrentIterMaterialIndex, SkeletalMaterial.MaterialSlotName);
						CurrentIterMaterialIndex++;
					}
					bool bClothSection = Model.Sections[SectionIdx].HasClothingData();
					OutSections.AddSection(LODIndex, SectionIdx, CurrentSectionMaterialSlotName, MaterialIndex, CurrentSectionOriginalImportedMaterialName, AvailableSectionName, SkelMesh->Materials[MaterialIndex].MaterialInterface, bClothSection);
				}
			}
		}
		else // refers to LODMaterialMap
		{
			TArray<int32>& MaterialMap = SkelMesh->GetLODInfo(LODIndex)->LODMaterialMap;

			for(int32 MapIdx = 0; MapIdx < MaterialMap.Num(); MapIdx++)
			{
				int32 MaterialIndex = MaterialMap[MapIdx];

				if (!SkelMesh->Materials.IsValidIndex(MaterialIndex))
				{
					MaterialMap[MapIdx] = MaterialIndex = SkelMesh->Materials.Add(FSkeletalMaterial());
				}
				FName CurrentSectionMaterialSlotName = SkelMesh->Materials[MaterialIndex].MaterialSlotName;
				FName CurrentSectionOriginalImportedMaterialName = SkelMesh->Materials[MaterialIndex].ImportedMaterialSlotName;
				TMap<int32, FName> AvailableSectionName;
				int32 CurrentIterMaterialIndex = 0;
				for (const FSkeletalMaterial &SkeletalMaterial : SkelMesh->Materials)
				{
					if (MaterialIndex != CurrentIterMaterialIndex)
						AvailableSectionName.Add(CurrentIterMaterialIndex, SkeletalMaterial.MaterialSlotName);
					CurrentIterMaterialIndex++;
				}
				OutSections.AddSection(LODIndex, MapIdx, CurrentSectionMaterialSlotName, MaterialIndex, CurrentSectionOriginalImportedMaterialName, AvailableSectionName, SkelMesh->Materials[MaterialIndex].MaterialInterface, false);
			}
		}

	}
}

FText FPersonaMeshDetails::GetMaterialNameText(int32 MaterialIndex) const
{
	if (SkeletalMeshPtr.IsValid() && SkeletalMeshPtr->Materials.IsValidIndex(MaterialIndex))
	{
		return FText::FromName(SkeletalMeshPtr->Materials[MaterialIndex].MaterialSlotName);
	}
	return FText::FromName(NAME_None);
}

FText FPersonaMeshDetails::GetOriginalImportMaterialNameText(int32 MaterialIndex) const
{
	if (SkeletalMeshPtr.IsValid() && SkeletalMeshPtr->Materials.IsValidIndex(MaterialIndex))
	{
		FString OriginalImportMaterialName;
		SkeletalMeshPtr->Materials[MaterialIndex].ImportedMaterialSlotName.ToString(OriginalImportMaterialName);
		OriginalImportMaterialName = TEXT("Original Imported Material Name: ") + OriginalImportMaterialName;
		return FText::FromString(OriginalImportMaterialName);
			}
	return FText::FromName(NAME_None);
		}

void FPersonaMeshDetails::OnMaterialNameCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 MaterialIndex)
{
	FName InValueName = FName(*(InValue.ToString()));
	if (SkeletalMeshPtr.IsValid() && SkeletalMeshPtr->Materials.IsValidIndex(MaterialIndex) && InValueName != SkeletalMeshPtr->Materials[MaterialIndex].MaterialSlotName)
	{
		FScopedTransaction ScopeTransaction(LOCTEXT("PersonaMaterialSlotNameChanged", "Persona editor: Material slot name change"));

		UProperty* ChangedProperty = FindField<UProperty>(USkeletalMesh::StaticClass(), "Materials");
		check(ChangedProperty);
		SkeletalMeshPtr->PreEditChange(ChangedProperty);

		SkeletalMeshPtr->Materials[MaterialIndex].MaterialSlotName = InValueName;
		
		FPropertyChangedEvent PropertyUpdateStruct(ChangedProperty);
		SkeletalMeshPtr->PostEditChangeProperty(PropertyUpdateStruct);
	}
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGenerateCustomNameWidgetsForMaterialArray(UMaterialInterface* Material, int32 MaterialIndex)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FPersonaMeshDetails::IsMaterialSelected, MaterialIndex)
			.OnCheckStateChanged(this, &FPersonaMeshDetails::OnMaterialSelectedChanged, MaterialIndex)
			.ToolTipText(LOCTEXT("Highlight_CustomMaterialName_ToolTip", "Highlights this material in the viewport"))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
				.Text(LOCTEXT("Highlight", "Highlight"))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2, 0, 0)
		[
			SNew(SCheckBox)
			.IsChecked(this, &FPersonaMeshDetails::IsIsolateMaterialEnabled, MaterialIndex)
			.OnCheckStateChanged(this, &FPersonaMeshDetails::OnMaterialIsolatedChanged, MaterialIndex)
			.ToolTipText(LOCTEXT("Isolate_CustomMaterialName_ToolTip", "Isolates this material in the viewport"))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
				.Text(LOCTEXT("Isolate", "Isolate"))
			]
		];
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGenerateCustomMaterialWidgetsForMaterialArray(UMaterialInterface* Material, int32 MaterialIndex, int32 LODIndex)
{
	bool bMaterialIsUsed = false;
	if(SkeletalMeshPtr.IsValid() && MaterialUsedMap.Contains(MaterialIndex))
	{
		bMaterialIsUsed = MaterialUsedMap.Find(MaterialIndex)->Num() > 0;
	}

	return
		SNew(SMaterialSlotWidget, MaterialIndex, bMaterialIsUsed)
		.MaterialName(this, &FPersonaMeshDetails::GetMaterialNameText, MaterialIndex)
		.OnMaterialNameCommitted(this, &FPersonaMeshDetails::OnMaterialNameCommitted, MaterialIndex)
		.CanDeleteMaterialSlot(this, &FPersonaMeshDetails::CanDeleteMaterialSlot, MaterialIndex)
		.OnDeleteMaterialSlot(this, &FPersonaMeshDetails::OnDeleteMaterialSlot, MaterialIndex)
		.ToolTipText(this, &FPersonaMeshDetails::GetOriginalImportMaterialNameText, MaterialIndex);
}

FText FPersonaMeshDetails::GetFirstMaterialSlotUsedBySection(int32 MaterialIndex) const
{
	if (SkeletalMeshPtr.IsValid() && MaterialUsedMap.Contains(MaterialIndex))
	{
		const TArray<FSectionLocalizer> *SectionLocalizers = MaterialUsedMap.Find(MaterialIndex);
		if (SectionLocalizers->Num() > 0)
		{
			FString ArrayItemName = FString::FromInt(SectionLocalizers->Num()) + TEXT(" Sections");
			return FText::FromString(ArrayItemName);
		}
	}
	return FText();
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGetMaterialSlotUsedByMenuContent(int32 MaterialIndex)
{
	FMenuBuilder MenuBuilder(true, NULL);

	TArray<FSectionLocalizer> *SectionLocalizers;
	if (SkeletalMeshPtr.IsValid() && MaterialUsedMap.Contains(MaterialIndex))
{
		SectionLocalizers = MaterialUsedMap.Find(MaterialIndex);
		FUIAction Action;
		FText EmptyTooltip;
		// Add a menu item for each texture.  Clicking on the texture will display it in the content browser
		for (const FSectionLocalizer& SectionUsingMaterial : (*SectionLocalizers))
		{
			FString ArrayItemName = TEXT("Lod ") + FString::FromInt(SectionUsingMaterial.LODIndex) + TEXT("  Index ") + FString::FromInt(SectionUsingMaterial.SectionIndex);
			MenuBuilder.AddMenuEntry(FText::FromString(ArrayItemName), EmptyTooltip, FSlateIcon(), Action);
		}
	}
	

	return MenuBuilder.MakeWidget();
}

bool FPersonaMeshDetails::CanDeleteMaterialSlot(int32 MaterialIndex) const
	{
	if (!SkeletalMeshPtr.IsValid())
	{
		return false;
	}

	return (MaterialIndex + 1) == SkeletalMeshPtr->Materials.Num();
	}
	
void FPersonaMeshDetails::OnDeleteMaterialSlot(int32 MaterialIndex)
{
	if (!SkeletalMeshPtr.IsValid() || !CanDeleteMaterialSlot(MaterialIndex))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("PersonaOnDeleteMaterialSlotTransaction", "Persona editor: Delete material slot"));
	SkeletalMeshPtr->Modify();
	SkeletalMeshPtr->Materials.RemoveAt(MaterialIndex);

	SkeletalMeshPtr->PostEditChange();
}

bool FPersonaMeshDetails::OnMaterialListDirty()
{
	bool ForceMaterialListRefresh = false;
	TMap<int32, TArray<FSectionLocalizer>> TempMaterialUsedMap;
	if (SkeletalMeshPtr.IsValid())
	{
		for (int32 MaterialIndex = 0; MaterialIndex < SkeletalMeshPtr->Materials.Num(); ++MaterialIndex)
		{
			TArray<FSectionLocalizer> SectionLocalizers;
			FSkeletalMeshModel* ImportedResource = SkeletalMeshPtr->GetImportedModel();
			check(ImportedResource);
			for (int32 LODIndex = 0; LODIndex < ImportedResource->LODModels.Num(); ++LODIndex)
			{
				FSkeletalMeshLODInfo& Info = *(SkeletalMeshPtr->GetLODInfo(LODIndex));
				if (LODIndex == 0 || SkeletalMeshPtr->GetLODInfo(LODIndex)->LODMaterialMap.Num() == 0)
				{
					for (int32 SectionIndex = 0; SectionIndex < ImportedResource->LODModels[LODIndex].Sections.Num(); ++SectionIndex)
					{
						if (GetMaterialIndex(LODIndex, SectionIndex) == MaterialIndex)
						{
							SectionLocalizers.Add(FSectionLocalizer(LODIndex, SectionIndex));
						}
					}
				}
				else
				{
					for (int32 SectionIndex = 0; SectionIndex < SkeletalMeshPtr->GetLODInfo(LODIndex)->LODMaterialMap.Num(); ++SectionIndex)
					{
						if (GetMaterialIndex(LODIndex, SectionIndex) == MaterialIndex)
						{
							SectionLocalizers.Add(FSectionLocalizer(LODIndex, SectionIndex));
						}
					}
				}
			}
			TempMaterialUsedMap.Add(MaterialIndex, SectionLocalizers);
		}
	}
	if (TempMaterialUsedMap.Num() != MaterialUsedMap.Num())
	{
		ForceMaterialListRefresh = true;
	}
	else if (!ForceMaterialListRefresh)
	{
		for (auto KvpOld : MaterialUsedMap)
		{
			if (!TempMaterialUsedMap.Contains(KvpOld.Key))
			{
				ForceMaterialListRefresh = true;
				break;
			}
			const TArray<FSectionLocalizer> &TempSectionLocalizers = (*(TempMaterialUsedMap.Find(KvpOld.Key)));
			const TArray<FSectionLocalizer> &OldSectionLocalizers = KvpOld.Value;
			if (TempSectionLocalizers.Num() != OldSectionLocalizers.Num())
			{
				ForceMaterialListRefresh = true;
				break;
			}
			for (int32 SectionLocalizerIndex = 0; SectionLocalizerIndex < OldSectionLocalizers.Num(); ++SectionLocalizerIndex)
			{
				if (OldSectionLocalizers[SectionLocalizerIndex] != TempSectionLocalizers[SectionLocalizerIndex])
				{
					ForceMaterialListRefresh = true;
					break;
				}
			}
			if (ForceMaterialListRefresh)
	{
				break;
			}
		}
	}
	MaterialUsedMap = TempMaterialUsedMap;

	return ForceMaterialListRefresh;
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGenerateCustomNameWidgetsForSection(int32 LodIndex, int32 SectionIndex)
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			.Visibility(this, &FPersonaMeshDetails::ShowEnabledSectionDetail, LodIndex, SectionIndex)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SCheckBox)
				.IsChecked(this, &FPersonaMeshDetails::IsSectionSelected, SectionIndex)
				.OnCheckStateChanged(this, &FPersonaMeshDetails::OnSectionSelectedChanged, SectionIndex)
				.ToolTipText(LOCTEXT("Highlight_ToolTip", "Highlights this section in the viewport"))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
					.Text(LOCTEXT("Highlight", "Highlight"))
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2, 0, 0)
			[
				SNew(SCheckBox)
				.IsChecked(this, &FPersonaMeshDetails::IsIsolateSectionEnabled, SectionIndex)
				.OnCheckStateChanged(this, &FPersonaMeshDetails::OnSectionIsolatedChanged, SectionIndex)
				.ToolTipText(LOCTEXT("Isolate_ToolTip", "Isolates this section in the viewport"))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
					.Text(LOCTEXT("Isolate", "Isolate"))
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2, 0, 0)
			[
				SNew(SBox)
				.Visibility(LodIndex == 0 ? EVisibility::All : EVisibility::Collapsed)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.FillWidth(1.0f)
					[
						SNew(SCheckBox)
						.IsChecked(this, &FPersonaMeshDetails::IsGenerateUpToSectionEnabled, LodIndex, SectionIndex)
						.OnCheckStateChanged(this, &FPersonaMeshDetails::OnSectionGenerateUpToChanged, LodIndex, SectionIndex)
						.ToolTipText(FText::Format(LOCTEXT("GenerateUpTo_ToolTip", "Generated LODs will use section {0} up to the specified value, and ignore it for lower quality LODs"), SectionIndex))
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
							.Text(LOCTEXT("GenerateUpTo", "Generate Up To"))
						]
					]
					+SHorizontalBox::Slot()
					.Padding(5, 2, 5, 0)
					.AutoWidth()
					[
						SNew(SNumericEntryBox<int8>)
						.Visibility(this, &FPersonaMeshDetails::ShowSectionGenerateUpToSlider, LodIndex, SectionIndex)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.MinDesiredValueWidth(40.0f)
						.MinValue(LodIndex)
						//.MaxValue(1)
						.MinSliderValue(LodIndex)
						.MaxSliderValue(FMath::Max(8, LODCount))
						.AllowSpin(true)
						.Value(this, &FPersonaMeshDetails::GetSectionGenerateUpToValue, LodIndex, SectionIndex)
						.OnValueChanged(this, &FPersonaMeshDetails::SetSectionGenerateUpToValue, LodIndex, SectionIndex)
						.OnValueCommitted(this, &FPersonaMeshDetails::SetSectionGenerateUpToValueCommitted, LodIndex, SectionIndex)
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Visibility(this, &FPersonaMeshDetails::ShowDisabledSectionDetail, LodIndex, SectionIndex)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
			.Text(LOCTEXT("SectionDisabled", "Disabled"))
			.ToolTipText(LOCTEXT("SectionDisable_ToolTip", "The section will not be rendered."))
		];
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGenerateCustomSectionWidgetsForSection(int32 LODIndex, int32 SectionIndex)
{
	extern ENGINE_API bool IsGPUSkinCacheAvailable();

	TSharedRef<SVerticalBox> SectionWidget = SNew(SVerticalBox);

#if WITH_APEX_CLOTHING

	UpdateClothingEntries();

	ClothComboBoxes.AddDefaulted();

	SectionWidget->AddSlot()
	.AutoHeight()
	.Padding(0, 2, 0, 0)
	.HAlign(HAlign_Fill)
	[
	SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			.MinDesiredWidth(65.0f)
		[
			SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("Clothing", "Clothing"))
		]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(5, 2, 0, 0)
		[
			SAssignNew(ClothComboBoxes.Last(), SClothComboBox)
			.OnGenerateWidget(this, &FPersonaMeshDetails::OnGenerateWidgetForClothingEntry)
			.OnSelectionChanged(this, &FPersonaMeshDetails::OnClothingSelectionChanged, ClothComboBoxes.Num() - 1, LODIndex, SectionIndex)
			.OnComboBoxOpening(this, &FPersonaMeshDetails::OnClothingComboBoxOpening)
			.OptionsSource(&NewClothingAssetEntries)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(this, &FPersonaMeshDetails::OnGetClothingComboText, LODIndex, SectionIndex)
			]
		]
	];

#endif// #if WITH_APEX_CLOTHING
	SectionWidget->AddSlot()
	.AutoHeight()
	.Padding(0, 2, 0, 0)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0, 2, 0)
		[
			SNew(SCheckBox)
			.IsChecked(this, &FPersonaMeshDetails::IsSectionShadowCastingEnabled, LODIndex, SectionIndex)
			.OnCheckStateChanged(this, &FPersonaMeshDetails::OnSectionShadowCastingChanged, LODIndex, SectionIndex)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("Cast Shadows", "Cast Shadows"))
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0, 2, 0)
		[
			SNew(SCheckBox)
			.IsEnabled(IsGPUSkinCacheAvailable())
			.IsChecked(this, &FPersonaMeshDetails::IsSectionRecomputeTangentEnabled, LODIndex, SectionIndex)
			.OnCheckStateChanged(this, &FPersonaMeshDetails::OnSectionRecomputeTangentChanged, LODIndex, SectionIndex)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("RecomputeTangent_Title", "Recompute Tangent"))
				.ToolTipText(LOCTEXT("RecomputeTangent_Tooltip", "This feature only works if you enable (Support Skincache Shaders) in the Project Settings. Please note that skin cache is an experimental feature and only works if you have compute shaders."))
			]
		]
	];
	return SectionWidget;
}

bool FPersonaMeshDetails::IsSectionEnabled(int32 LodIndex, int32 SectionIndex) const
{
	if(SkeletalMeshPtr.IsValid())
	{
		FSkeletalMeshModel* SourceModel = SkeletalMeshPtr->GetImportedModel();

		if(SourceModel->LODModels.IsValidIndex(LodIndex))
		{
			FSkeletalMeshLODModel& LodModel = SourceModel->LODModels[LodIndex];

			if(LodModel.Sections.IsValidIndex(SectionIndex))
			{
				return !LodModel.Sections[SectionIndex].bDisabled;
			}
		}
	}

	return false;
}

EVisibility FPersonaMeshDetails::ShowEnabledSectionDetail(int32 LodIndex, int32 SectionIndex) const
{
	return IsSectionEnabled(LodIndex, SectionIndex) ? EVisibility::All : EVisibility::Collapsed;
}

EVisibility FPersonaMeshDetails::ShowDisabledSectionDetail(int32 LodIndex, int32 SectionIndex) const
{
	return IsSectionEnabled(LodIndex, SectionIndex) ? EVisibility::Collapsed : EVisibility::All;
}

void FPersonaMeshDetails::OnSectionEnabledChanged(int32 LodIndex, int32 SectionIndex, bool bEnable)
{
	if(SkeletalMeshPtr.IsValid())
	{
		FSkeletalMeshModel* SourceModel = SkeletalMeshPtr->GetImportedModel();

		if(SourceModel->LODModels.IsValidIndex(LodIndex))
		{
			FSkeletalMeshLODModel& LodModel = SourceModel->LODModels[LodIndex];

			if(LodModel.Sections.IsValidIndex(SectionIndex))
			{
				FSkelMeshSection& Section = LodModel.Sections[SectionIndex];

				if(Section.bDisabled != !bEnable)
				{
					FScopedTransaction Transaction(LOCTEXT("ChangeSectionEnabled", "Set section disabled flag."));

					SkeletalMeshPtr->Modify();
					SkeletalMeshPtr->PreEditChange(nullptr);

					Section.bDisabled = !bEnable;

					// Disable highlight and isolate flags
					UDebugSkelMeshComponent * MeshComponent = GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent();
					if(MeshComponent)
					{
						MeshComponent->SetSelectedEditorSection(INDEX_NONE);
						MeshComponent->SetSelectedEditorMaterial(INDEX_NONE);
						MeshComponent->SetMaterialPreview(INDEX_NONE);
						MeshComponent->SetSectionPreview(INDEX_NONE);
					}

					// Invalidate render data
					SkeletalMeshPtr->PostEditChange();
				}
			}
		}
	}
}

TOptional<int8> FPersonaMeshDetails::GetSectionGenerateUpToValue(int32 LodIndex, int32 SectionIndex) const
{
	if (!SkeletalMeshPtr.IsValid() ||
		!SkeletalMeshPtr->GetImportedModel()->LODModels.IsValidIndex(LodIndex) ||
		!SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections.IsValidIndex(SectionIndex) )
	{
		return TOptional<int8>(-1);
	}
	int8 SpecifiedLodIndex = SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections[SectionIndex].GenerateUpToLodIndex;
	check(SpecifiedLodIndex == -1 || SpecifiedLodIndex >= LodIndex);
	return TOptional<int8>(SpecifiedLodIndex);
}

void FPersonaMeshDetails::SetSectionGenerateUpToValue(int8 Value, int32 LodIndex, int32 SectionIndex)
{
	if (!SkeletalMeshPtr.IsValid() ||
		!SkeletalMeshPtr->GetImportedModel()->LODModels.IsValidIndex(LodIndex) ||
		!SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections.IsValidIndex(SectionIndex))
	{
		return;
	}
	int64 ValueKey = ((int64)LodIndex << 32) | (int64)SectionIndex;
	if (!OldGenerateUpToSliderValues.Contains(ValueKey))
	{
		OldGenerateUpToSliderValues.Add(ValueKey, SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections[SectionIndex].GenerateUpToLodIndex);
	}
	SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections[SectionIndex].GenerateUpToLodIndex = Value;
}

void FPersonaMeshDetails::SetSectionGenerateUpToValueCommitted(int8 Value, ETextCommit::Type CommitInfo, int32 LodIndex, int32 SectionIndex)
{
	int64 ValueKey = ((int64)LodIndex << 32) | (int64)SectionIndex;
	int8 OldValue;
	bool bHasOldValue = OldGenerateUpToSliderValues.RemoveAndCopyValue(ValueKey, OldValue);
	if (!SkeletalMeshPtr.IsValid() ||
		!SkeletalMeshPtr->GetImportedModel()->LODModels.IsValidIndex(LodIndex) ||
		!SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections.IsValidIndex(SectionIndex))
	{
		return;
	}
	
	if (bHasOldValue)
	{
		//Put back the original value before registering the undo transaction
		SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections[SectionIndex].GenerateUpToLodIndex = OldValue;
	}

	if (CommitInfo == ETextCommit::OnCleared)
	{
		//If the user cancel is change early exit while the value is the same as the original
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("ChangeGenerateUpTo", "Set Generate Up To"));

	SkeletalMeshPtr->Modify();
	SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections[SectionIndex].GenerateUpToLodIndex = Value;
	SkeletalMeshPtr->PostEditChange();
	GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
}

EVisibility FPersonaMeshDetails::ShowSectionGenerateUpToSlider(int32 LodIndex, int32 SectionIndex) const
{
	if (!SkeletalMeshPtr.IsValid() ||
		!SkeletalMeshPtr->GetImportedModel()->LODModels.IsValidIndex(LodIndex) ||
		!SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections.IsValidIndex(SectionIndex))
	{
		return EVisibility::Collapsed;
	}
	return SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections[SectionIndex].GenerateUpToLodIndex == -1 ? EVisibility::Collapsed : EVisibility::All;
}

ECheckBoxState FPersonaMeshDetails::IsGenerateUpToSectionEnabled(int32 LodIndex, int32 SectionIndex) const
{
	if (!SkeletalMeshPtr.IsValid() ||
		!SkeletalMeshPtr->GetImportedModel()->LODModels.IsValidIndex(LodIndex) ||
		!SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections.IsValidIndex(SectionIndex))
	{
		return ECheckBoxState::Unchecked;
	}
	return SkeletalMeshPtr->GetImportedModel()->LODModels[LodIndex].Sections[SectionIndex].GenerateUpToLodIndex != -1 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FPersonaMeshDetails::OnSectionGenerateUpToChanged(ECheckBoxState NewState, int32 LodIndex, int32 SectionIndex)
{
	SetSectionGenerateUpToValueCommitted(NewState == ECheckBoxState::Checked ? LodIndex : -1, ETextCommit::Type::Default , LodIndex, SectionIndex);
}

void FPersonaMeshDetails::SetCurrentLOD(int32 NewLodIndex)
{
	if (GetPersonaToolkit()->GetPreviewMeshComponent() == nullptr)
	{
		return;
	}
	int32 CurrentDisplayLOD = GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel;
	int32 RealCurrentDisplayLOD = CurrentDisplayLOD == 0 ? 0 : CurrentDisplayLOD - 1;
	int32 RealNewLOD = NewLodIndex == 0 ? 0 : NewLodIndex - 1;
	if (CurrentDisplayLOD == NewLodIndex || !LodCategories.IsValidIndex(RealCurrentDisplayLOD) || !LodCategories.IsValidIndex(RealNewLOD))
	{
		return;
	}
	GetPersonaToolkit()->GetPreviewMeshComponent()->SetForcedLOD(NewLodIndex);
	
	//Reset the preview section since we do not edit the same LOD
	GetPersonaToolkit()->GetPreviewMeshComponent()->SetSectionPreview(INDEX_NONE);
	GetPersonaToolkit()->GetPreviewMeshComponent()->SetSelectedEditorSection(INDEX_NONE);

	GetPersonaToolkit()->GetPreviewScene()->BroadcastOnSelectedLODChanged();
}

void FPersonaMeshDetails::UpdateLODCategoryVisibility() const
{
	if (GetPersonaToolkit()->GetCustomData(CustomDataKey_LODEditMode) > 0)
	{
		//Do not change the Category visibility if we are in custom mode
		return;
	}
	bool bAutoLod = false;
	if (GetPersonaToolkit()->GetPreviewMeshComponent() != nullptr)
	{
		bAutoLod = GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel == 0;
	}
	int32 CurrentDisplayLOD = bAutoLod ? 0 : GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel - 1;
	if (LodCategories.IsValidIndex(CurrentDisplayLOD) && GetPersonaToolkit()->GetMesh())
	{
		int32 SkeletalMeshLodNumber = GetPersonaToolkit()->GetMesh()->GetLODNum();
		for (int32 LodCategoryIndex = 0; LodCategoryIndex < SkeletalMeshLodNumber; ++LodCategoryIndex)
		{
			LodCategories[LodCategoryIndex]->SetCategoryVisibility(CurrentDisplayLOD == LodCategoryIndex);
		}
	}

	//Reset the preview section since we do not edit the same LOD
	GetPersonaToolkit()->GetPreviewMeshComponent()->SetSectionPreview(INDEX_NONE);
	GetPersonaToolkit()->GetPreviewMeshComponent()->SetSelectedEditorSection(INDEX_NONE);
}

FText FPersonaMeshDetails::GetCurrentLodName() const
{
	bool bAutoLod = false;
	if (GetPersonaToolkit()->GetPreviewMeshComponent() != nullptr)
	{
		bAutoLod = GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel == 0;
	}
	int32 CurrentDisplayLOD = bAutoLod ? 0 : GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel - 1;
	return FText::FromString(bAutoLod ? FString(TEXT("Auto (LOD0)")) : (FString(TEXT("LOD")) + FString::FromInt(CurrentDisplayLOD)));
}

FText FPersonaMeshDetails::GetCurrentLodTooltip() const
{
	if (GetPersonaToolkit()->GetPreviewMeshComponent() != nullptr && GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel == 0)
	{
		return FText::FromString(TEXT("LOD0 is edit when selecting Auto LOD"));
	}
	return FText::GetEmpty();
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGenerateLodComboBoxForLodPicker()
{
	return SNew(SComboButton)
		.IsEnabled(this, &FPersonaMeshDetails::IsLodComboBoxEnabledForLodPicker)
		.OnGetMenuContent(this, &FPersonaMeshDetails::OnGenerateLodMenuForLodPicker)
		.VAlign(VAlign_Center)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FPersonaMeshDetails::GetCurrentLodName)
			.ToolTipText(this, &FPersonaMeshDetails::GetCurrentLodTooltip)
		];
}

EVisibility FPersonaMeshDetails::LodComboBoxVisibilityForLodPicker() const
{
	//No combo box when in Custom mode
	if (GetPersonaToolkit()->GetCustomData(CustomDataKey_LODEditMode) > 0)
	{
		return EVisibility::Hidden;
	}
	return EVisibility::All;
}

bool FPersonaMeshDetails::IsLodComboBoxEnabledForLodPicker() const
{
	//No combo box when in Custom mode
	return !(GetPersonaToolkit()->GetCustomData(CustomDataKey_LODEditMode) > 0);
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGenerateLodMenuForLodPicker()
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	if (SkelMesh == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	bool bAutoLod = false;
	if (GetPersonaToolkit()->GetPreviewMeshComponent() != nullptr)
	{
		bAutoLod = GetPersonaToolkit()->GetPreviewMeshComponent()->ForcedLodModel == 0;
	}
	const int32 SkelMeshLODCount = SkelMesh->GetLODNum();
	if(SkelMeshLODCount < 2)
	{
		return SNullWidget::NullWidget;
	}
	FMenuBuilder MenuBuilder(true, NULL);

	FText AutoLodText = FText::FromString((TEXT("Auto LOD")));
	FUIAction AutoLodAction(FExecuteAction::CreateSP(this, &FPersonaMeshDetails::SetCurrentLOD, 0));
	MenuBuilder.AddMenuEntry(AutoLodText, LOCTEXT("OnGenerateLodMenuForSectionList_Auto_ToolTip", "LOD0 is edit when selecting Auto LOD"), FSlateIcon(), AutoLodAction);
	// Add a menu item for each texture.  Clicking on the texture will display it in the content browser
	for (int32 AllLodIndex = 0; AllLodIndex < SkelMeshLODCount; ++AllLodIndex)
	{
		FText LODLevelString = FText::FromString((TEXT("LOD ") + FString::FromInt(AllLodIndex)));
		FUIAction Action(FExecuteAction::CreateSP(this, &FPersonaMeshDetails::SetCurrentLOD, AllLodIndex+1));
		MenuBuilder.AddMenuEntry(LODLevelString, FText::GetEmpty(), FSlateIcon(), Action);
	}

	return MenuBuilder.MakeWidget();
}

ECheckBoxState FPersonaMeshDetails::IsMaterialSelected(int32 MaterialIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	UDebugSkelMeshComponent* MeshComponent = GetPersonaToolkit()->GetPreviewMeshComponent();
	if (MeshComponent)
	{
		State = MeshComponent->GetSelectedEditorMaterial() == MaterialIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return State;
}

void FPersonaMeshDetails::OnMaterialSelectedChanged(ECheckBoxState NewState, int32 MaterialIndex)
{
	// Currently assumes that we only ever have one preview mesh in Persona.
	UDebugSkelMeshComponent* MeshComponent = GetPersonaToolkit()->GetPreviewMeshComponent();
	if (MeshComponent)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			MeshComponent->SetSelectedEditorMaterial(MaterialIndex);
			if (MeshComponent->GetMaterialPreview() != MaterialIndex)
			{
				// Unhide all mesh sections
				MeshComponent->SetMaterialPreview(INDEX_NONE);
			}
			//Remove any section isolate or highlight
			MeshComponent->SetSelectedEditorSection(INDEX_NONE);
			MeshComponent->SetSectionPreview(INDEX_NONE);
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			MeshComponent->SetSelectedEditorMaterial(INDEX_NONE);
		}
		MeshComponent->PushSelectionToProxy();
		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}
}

ECheckBoxState FPersonaMeshDetails::IsIsolateMaterialEnabled(int32 MaterialIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const UDebugSkelMeshComponent* MeshComponent = GetPersonaToolkit()->GetPreviewMeshComponent();
	if (MeshComponent)
	{
		State = MeshComponent->GetMaterialPreview() == MaterialIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return State;
}

void FPersonaMeshDetails::OnMaterialIsolatedChanged(ECheckBoxState NewState, int32 MaterialIndex)
{
	UDebugSkelMeshComponent * MeshComponent = GetPersonaToolkit()->GetPreviewMeshComponent();
	if (MeshComponent)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			MeshComponent->SetMaterialPreview(MaterialIndex);
			if (MeshComponent->GetSelectedEditorMaterial() != MaterialIndex)
			{
				MeshComponent->SetSelectedEditorMaterial(INDEX_NONE);
			}
			//Remove any section isolate or highlight
			MeshComponent->SetSelectedEditorSection(INDEX_NONE);
			MeshComponent->SetSectionPreview(INDEX_NONE);
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			MeshComponent->SetMaterialPreview(INDEX_NONE);
		}
		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}
}

ECheckBoxState FPersonaMeshDetails::IsSectionSelected(int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	UDebugSkelMeshComponent * MeshComponent = GetPersonaToolkit()->GetPreviewMeshComponent();
	if (MeshComponent)
	{
		State = MeshComponent->GetSelectedEditorSection() == SectionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return State;
}

void FPersonaMeshDetails::OnSectionSelectedChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	// Currently assumes that we only ever have one preview mesh in Persona.
	UDebugSkelMeshComponent* MeshComponent = GetPersonaToolkit()->GetPreviewMeshComponent();

	if (MeshComponent)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			MeshComponent->SetSelectedEditorSection(SectionIndex);
			if (MeshComponent->GetSectionPreview() != SectionIndex)
			{
				// Unhide all mesh sections
				MeshComponent->SetSectionPreview(INDEX_NONE);
			}
			MeshComponent->SetSelectedEditorMaterial(INDEX_NONE);
			MeshComponent->SetMaterialPreview(INDEX_NONE);
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			MeshComponent->SetSelectedEditorSection(INDEX_NONE);
		}
		MeshComponent->PushSelectionToProxy();
		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}
}

ECheckBoxState FPersonaMeshDetails::IsIsolateSectionEnabled(int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const UDebugSkelMeshComponent* MeshComponent = GetPersonaToolkit()->GetPreviewMeshComponent();
	if (MeshComponent)
	{
		State = MeshComponent->GetSectionPreview() == SectionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return State;
}

void FPersonaMeshDetails::OnSectionIsolatedChanged(ECheckBoxState NewState, int32 SectionIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	UDebugSkelMeshComponent * MeshComponent = GetPersonaToolkit()->GetPreviewMeshComponent();
	if (Mesh && MeshComponent)
	{
		if (NewState == ECheckBoxState::Checked)
		{
			MeshComponent->SetSectionPreview(SectionIndex);
			if (MeshComponent->GetSelectedEditorSection() != SectionIndex)
			{
				MeshComponent->SetSelectedEditorSection(INDEX_NONE);
			}
			MeshComponent->SetMaterialPreview(INDEX_NONE);
			MeshComponent->SetSelectedEditorMaterial(INDEX_NONE);
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			MeshComponent->SetSectionPreview(INDEX_NONE);
		}
		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}
}

ECheckBoxState FPersonaMeshDetails::IsShadowCastingEnabled(int32 MaterialIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	bool FirstValueSet = false;
	bool AllValueState = false;
	bool AllValueSame = true;
	if (Mesh == nullptr)
		return State;
	check(Mesh->GetImportedModel());
	for (int32 LODIdx = 0; LODIdx < Mesh->GetImportedModel()->LODModels.Num(); LODIdx++)
	{
		const FSkeletalMeshLODModel& LODModel = Mesh->GetImportedModel()->LODModels[LODIdx];
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
		{
			const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

			if (GetMaterialIndex(LODIdx, SectionIndex) != MaterialIndex)
				continue;

			if (!FirstValueSet)
			{
				FirstValueSet = true;
				AllValueState = Section.bCastShadow;
				AllValueSame = true;
			}
			else
			{
				if (AllValueState != Section.bCastShadow)
				{
					AllValueSame = false;
					break;
				}
			}
		}
	}

	State = AllValueSame ? (AllValueState ? ECheckBoxState::Checked : ECheckBoxState::Unchecked) : ECheckBoxState::Undetermined;
	return State;
}

void FPersonaMeshDetails::OnShadowCastingChanged(ECheckBoxState NewState, int32 MaterialIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh)
	{
		bool NewValue = false;
		if (NewState == ECheckBoxState::Checked)
		{
			const FScopedTransaction Transaction(LOCTEXT("PersonaSetShadowCastingFlag", "Persona editor: Set Shadow Casting For Material"));
			Mesh->Modify();
			NewValue = true;
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			const FScopedTransaction Transaction(LOCTEXT("PersonaClearShadowCastingFlag", "Persona editor: Clear Shadow Casting For Material"));
			Mesh->Modify();
			NewValue = false;
		}

		check(Mesh->GetImportedModel());
		for (int32 LODIdx = 0; LODIdx < Mesh->GetImportedModel()->LODModels.Num(); LODIdx++)
		{
			FSkeletalMeshLODModel& LODModel = Mesh->GetImportedModel()->LODModels[LODIdx];
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
			{
				FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

				if (Section.MaterialIndex != MaterialIndex)
					continue;

				Section.bCastShadow = NewValue;
			}
		}

		Mesh->PostEditChange(); // update derived mesh data, and recreate render state for components

		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}
}


ECheckBoxState FPersonaMeshDetails::IsRecomputeTangentEnabled(int32 MaterialIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh == nullptr)
		return State;

	bool FirstValueSet = false;
	bool AllValueState = false;
	bool AllValueSame = true;
	check(Mesh->GetImportedModel());
	for (int32 LODIdx = 0; LODIdx < Mesh->GetImportedModel()->LODModels.Num(); LODIdx++)
	{
		const FSkeletalMeshLODModel& LODModel = Mesh->GetImportedModel()->LODModels[LODIdx];
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
		{
			const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
			
			if (Section.MaterialIndex != MaterialIndex)
				continue;

			if (!FirstValueSet)
			{
				FirstValueSet = true;
				AllValueState = Section.bRecomputeTangent;
				AllValueSame = true;
			}
			else
			{
				if (AllValueState != Section.bRecomputeTangent)
				{
					AllValueSame = false;
					break;
				}
			}
		}
	}

	State = AllValueSame ? (AllValueState ? ECheckBoxState::Checked : ECheckBoxState::Unchecked) : ECheckBoxState::Undetermined;

	return State;
}

void FPersonaMeshDetails::OnRecomputeTangentChanged(ECheckBoxState NewState, int32 MaterialIndex)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if (Mesh)
	{
		bool NewValue = false;
		if (NewState == ECheckBoxState::Checked)
		{
			const FScopedTransaction Transaction(LOCTEXT("PersonaSetRecomputeTangentFlag", "Persona editor: Set Recompute Tangent For Material"));
			Mesh->Modify();
			NewValue = true;
		}
		else if (NewState == ECheckBoxState::Unchecked)
		{
			const FScopedTransaction Transaction(LOCTEXT("PersonaClearRecomputeTangentFlag", "Persona editor: Clear Recompute Tangent For Material"));
			Mesh->Modify();
			NewValue = false;
		}

		for (int32 LODIdx = 0; LODIdx < Mesh->GetImportedModel()->LODModels.Num(); LODIdx++)
		{
			FSkeletalMeshLODModel& LODModel = Mesh->GetImportedModel()->LODModels[LODIdx];
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
			{
				FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

				if (Section.MaterialIndex != MaterialIndex)
					continue;

				Section.bRecomputeTangent = NewValue;
			}
		}

		Mesh->PostEditChange(); // update derived mesh data, and recreate render state for components

		GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
	}
}

ECheckBoxState FPersonaMeshDetails::IsSectionShadowCastingEnabled(int32 LODIndex, int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh == nullptr)
		return State;
	
	check(Mesh->GetImportedModel());

	if (!Mesh->GetImportedModel()->LODModels.IsValidIndex(LODIndex))
		return State;

	const FSkeletalMeshLODModel& LODModel = Mesh->GetImportedModel()->LODModels[LODIndex];

	if (!LODModel.Sections.IsValidIndex(SectionIndex))
		return State;

	const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	State = Section.bCastShadow ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	return State;
}

void FPersonaMeshDetails::OnSectionShadowCastingChanged(ECheckBoxState NewState, int32 LODIndex, int32 SectionIndex)
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh == nullptr)
		return;

	check(Mesh->GetImportedModel());

	if (!Mesh->GetImportedModel()->LODModels.IsValidIndex(LODIndex))
		return;

	FSkeletalMeshLODModel& LODModel = Mesh->GetImportedModel()->LODModels[LODIndex];

	if (!LODModel.Sections.IsValidIndex(SectionIndex))
		return;

	FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	if (NewState == ECheckBoxState::Checked)
	{
		const FScopedTransaction Transaction(LOCTEXT("PersonaSetSectionShadowCastingFlag", "Persona editor: Set Shadow Casting For Section"));
		Mesh->Modify();
		Section.bCastShadow = true;
		}
	else if (NewState == ECheckBoxState::Unchecked)
	{
		const FScopedTransaction Transaction(LOCTEXT("PersonaClearSectionShadowCastingFlag", "Persona editor: Clear Shadow Casting For Section"));
		Mesh->Modify();
		Section.bCastShadow = false;
	}
	
	Mesh->PostEditChange(); // update derived mesh data, and recreate render state for components

	GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
}

ECheckBoxState FPersonaMeshDetails::IsSectionRecomputeTangentEnabled(int32 LODIndex, int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	const USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh == nullptr)
		return State;

	check(Mesh->GetImportedModel());

	if (!Mesh->GetImportedModel()->LODModels.IsValidIndex(LODIndex))
		return State;

	const FSkeletalMeshLODModel& LODModel = Mesh->GetImportedModel()->LODModels[LODIndex];

	if (!LODModel.Sections.IsValidIndex(SectionIndex))
		return State;

	const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	State = Section.bRecomputeTangent ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	return State;
}

void FPersonaMeshDetails::OnSectionRecomputeTangentChanged(ECheckBoxState NewState, int32 LODIndex, int32 SectionIndex)
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if (Mesh == nullptr)
		return;

	check(Mesh->GetImportedModel());

	if (!Mesh->GetImportedModel()->LODModels.IsValidIndex(LODIndex))
		return;

	FSkeletalMeshLODModel& LODModel = Mesh->GetImportedModel()->LODModels[LODIndex];

	if (!LODModel.Sections.IsValidIndex(SectionIndex))
		return;

	FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

	if (NewState == ECheckBoxState::Checked)
	{
		const FScopedTransaction Transaction(LOCTEXT("PersonaSetSectionRecomputeTangentFlag", "Persona editor: Set Recompute Tangent For Section"));
		Mesh->Modify();
		Section.bRecomputeTangent = true;
	}
	else if (NewState == ECheckBoxState::Unchecked)
	{
		const FScopedTransaction Transaction(LOCTEXT("PersonaClearSectionRecomputeTangentFlag", "Persona editor: Clear Recompute Tangent For Section"));
		Mesh->Modify();
		Section.bRecomputeTangent = false;
	}

	Mesh->PostEditChange(); // update derived mesh data, and recreate render state for components

	GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
}

EVisibility FPersonaMeshDetails::GetOverrideUVDensityVisibililty() const
{
	if (/*GetViewMode() == VMI_MeshUVDensityAccuracy*/ true)
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

ECheckBoxState FPersonaMeshDetails::IsUVDensityOverridden(int32 MaterialIndex) const
{
	USkeletalMesh* Mesh = SkeletalMeshPtr.Get();
	if (!Mesh || !Mesh->Materials.IsValidIndex(MaterialIndex))
	{
		return ECheckBoxState::Undetermined;
	}
	else if (Mesh->Materials[MaterialIndex].UVChannelData.bOverrideDensities)
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}


void FPersonaMeshDetails::OnOverrideUVDensityChanged(ECheckBoxState NewState, int32 MaterialIndex)
{
	USkeletalMesh* Mesh = SkeletalMeshPtr.Get();
	if (NewState != ECheckBoxState::Undetermined && Mesh && Mesh->Materials.IsValidIndex(MaterialIndex))
	{
		Mesh->Materials[MaterialIndex].UVChannelData.bOverrideDensities = (NewState == ECheckBoxState::Checked);
		Mesh->UpdateUVChannelData(true);
	}
}

EVisibility FPersonaMeshDetails::GetUVDensityVisibility(int32 MaterialIndex, int32 UVChannelIndex) const
{
	USkeletalMesh* Mesh = SkeletalMeshPtr.Get();
	if (/*MeshGetViewMode() == VMI_MeshUVDensityAccuracy && */ Mesh && IsUVDensityOverridden(MaterialIndex) == ECheckBoxState::Checked)
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

TOptional<float> FPersonaMeshDetails::GetUVDensityValue(int32 MaterialIndex, int32 UVChannelIndex) const
{
	USkeletalMesh* Mesh = SkeletalMeshPtr.Get();
	if (Mesh && Mesh->Materials.IsValidIndex(MaterialIndex))
	{
		float Value = Mesh->Materials[MaterialIndex].UVChannelData.LocalUVDensities[UVChannelIndex];
		return FMath::RoundToFloat(Value * 4.f) * .25f;
	}
	return TOptional<float>();
}

void FPersonaMeshDetails::SetUVDensityValue(float InDensity, ETextCommit::Type CommitType, int32 MaterialIndex, int32 UVChannelIndex)
{
	USkeletalMesh* Mesh = SkeletalMeshPtr.Get();
	if (Mesh && Mesh->Materials.IsValidIndex(MaterialIndex))
	{
		Mesh->Materials[MaterialIndex].UVChannelData.LocalUVDensities[UVChannelIndex] = FMath::Max<float>(0, InDensity);
		Mesh->UpdateUVChannelData(true);
	}
}

int32 FPersonaMeshDetails::GetMaterialIndex(int32 LODIndex, int32 SectionIndex) const
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	check(LODIndex < SkelMesh->GetLODNum());

	FSkeletalMeshLODInfo& Info = *(SkelMesh->GetLODInfo(LODIndex));
	if (LODIndex == 0 || Info.LODMaterialMap.Num() == 0 || SectionIndex >= Info.LODMaterialMap.Num())
	{
		FSkeletalMeshModel* ImportedResource = SkelMesh->GetImportedModel();
		check(ImportedResource && ImportedResource->LODModels.IsValidIndex(LODIndex));
		return ImportedResource->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex;
	}
	else
	{
		return Info.LODMaterialMap[SectionIndex];
	}
}

bool FPersonaMeshDetails::IsDuplicatedMaterialIndex(int32 LODIndex, int32 MaterialIndex)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	// finding whether this material index is being used in parent LODs
	for (int32 LODInfoIdx = 0; LODInfoIdx < LODIndex; LODInfoIdx++)
	{
		FSkeletalMeshLODInfo& Info = *(SkelMesh->GetLODInfo(LODInfoIdx));
		if (LODIndex == 0 || Info.LODMaterialMap.Num() == 0)
		{
			FSkeletalMeshModel* ImportedResource = SkelMesh->GetImportedModel();

			if (ImportedResource && ImportedResource->LODModels.IsValidIndex(LODInfoIdx))
			{
				FSkeletalMeshLODModel& Model = ImportedResource->LODModels[LODInfoIdx];

				for (int32 SectionIdx = 0; SectionIdx < Model.Sections.Num(); SectionIdx++)
				{
					if (MaterialIndex == Model.Sections[SectionIdx].MaterialIndex)
					{
						return true;
					}
				}
			}
		}
		else // if LODMaterialMap exists
		{
			for (int32 MapIdx = 0; MapIdx < Info.LODMaterialMap.Num(); MapIdx++)
			{
				if (MaterialIndex == Info.LODMaterialMap[MapIdx])
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FPersonaMeshDetails::OnSectionChanged(int32 LODIndex, int32 SectionIndex, int32 NewMaterialSlotIndex, FName NewMaterialSlotName)
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();
	if(Mesh)
	{
		FSkeletalMeshModel* ImportedResource = Mesh->GetImportedModel();
		check(ImportedResource && ImportedResource->LODModels.IsValidIndex(LODIndex));
		const int32 TotalSectionCount = ImportedResource->LODModels[LODIndex].Sections.Num();

		check(TotalSectionCount > SectionIndex);

		int32 NewSkeletalMaterialIndex = INDEX_NONE;
		for (int SkeletalMaterialIndex = 0; SkeletalMaterialIndex < Mesh->Materials.Num(); ++SkeletalMaterialIndex)
		{
			if (NewMaterialSlotIndex == SkeletalMaterialIndex && Mesh->Materials[SkeletalMaterialIndex].MaterialSlotName == NewMaterialSlotName)
			{
				NewSkeletalMaterialIndex = SkeletalMaterialIndex;
				break;
			}
		}

		check(NewSkeletalMaterialIndex != INDEX_NONE);

		// Begin a transaction for undo/redo the first time we encounter a material to replace.  
		// There is only one transaction for all replacement
		FScopedTransaction Transaction(LOCTEXT("PersonaOnSectionChangedTransaction", "Persona editor: Section material slot changed"));
		Mesh->Modify();

		FSkeletalMeshLODInfo& Info = *(Mesh->GetLODInfo(LODIndex));
		if (LODIndex == 0 || Info.LODMaterialMap.Num() == 0)
		{
			ImportedResource->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex = NewSkeletalMaterialIndex;
		}
		else
		{
			check(SectionIndex < Info.LODMaterialMap.Num());
			Info.LODMaterialMap[SectionIndex] = NewSkeletalMaterialIndex;
		}

		Mesh->PostEditChange();

		// Redraw viewports to reflect the material changes 
		GUnrealEd->RedrawLevelEditingViewports();
	}
}

//
// Generate slate UI for Clothing category
//
void FPersonaMeshDetails::CustomizeClothingProperties(IDetailLayoutBuilder& DetailLayout, IDetailCategoryBuilder& ClothingFilesCategory)
{
	TSharedRef<IPropertyHandle> ClothingAssetsProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalMesh, MeshClothingAssets), USkeletalMesh::StaticClass());

	if( ClothingAssetsProperty->IsValidHandle() )
	{
		TSharedRef<FDetailArrayBuilder> ClothingAssetsPropertyBuilder = MakeShareable( new FDetailArrayBuilder( ClothingAssetsProperty ) );
		ClothingAssetsPropertyBuilder->OnGenerateArrayElementWidget( FOnGenerateArrayElementWidget::CreateSP( this, &FPersonaMeshDetails::OnGenerateElementForClothingAsset, &DetailLayout) );

		ClothingFilesCategory.AddCustomBuilder(ClothingAssetsPropertyBuilder, false);
	}

#if WITH_APEX_CLOTHING
	// Button to add a new clothing file
	ClothingFilesCategory.AddCustomRow( LOCTEXT("AddAPEXClothingFileFilterString", "Add APEX clothing file"))
	[
		SNew(SHorizontalBox)
		 
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked(this, &FPersonaMeshDetails::OnOpenClothingFileClicked, &DetailLayout)
			.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("AddClothingButtonTooltip", "Select a new APEX clothing file and add it to the skeletal mesh."),
				NULL,
				TEXT("Shared/Editors/Persona"),
				TEXT("AddClothing")))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("AddAPEXClothingFile", "Add APEX clothing file..."))
			]
		]
	];
#endif
}

//
// Generate each ClothingAsset array entry
//
void FPersonaMeshDetails::OnGenerateElementForClothingAsset( TSharedRef<IPropertyHandle> StructProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout )
{
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();

	// Remove and reimport asset buttons
	ChildrenBuilder.AddCustomRow( FText::GetEmpty() ) 
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)

		// re-import button
		+ SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.Padding(2)
		.AutoWidth()
		[
			SNew( SButton )
			.Text( LOCTEXT("ReimportButtonLabel", "Reimport") )
			.OnClicked(this, &FPersonaMeshDetails::OnReimportApexFileClicked, ElementIndex, DetailLayout)
			.IsFocusable( false )
			.ContentPadding(0)
			.ForegroundColor( FSlateColor::UseForeground() )
			.ButtonColorAndOpacity(FLinearColor(1.0f,1.0f,1.0f,0.0f))
			.ToolTipText(LOCTEXT("ReimportApexFileTip", "Reimport this APEX asset"))
			[ 
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("Persona.ReimportAsset") )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
		]

		// remove button
		+ SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.Padding(2)
		.AutoWidth()
		[
			SNew( SButton )
			.Text( LOCTEXT("ClearButtonLabel", "Remove") )
			.OnClicked( this, &FPersonaMeshDetails::OnRemoveApexFileClicked, ElementIndex, DetailLayout )
			.IsFocusable( false )
			.ContentPadding(0)
			.ForegroundColor( FSlateColor::UseForeground() )
			.ButtonColorAndOpacity(FLinearColor(1.0f,1.0f,1.0f,0.0f))
			.ToolTipText(LOCTEXT("RemoveApexFileTip", "Remove this APEX asset"))
			[ 
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Clear") )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
		]
	];	

	USkeletalMesh* CurrentMesh = GetPersonaToolkit()->GetMesh();
	UClothingAssetBase* CurrentAsset = CurrentMesh->MeshClothingAssets[ElementIndex];

	ChildrenBuilder.AddCustomRow(LOCTEXT("ClothingAsset_Search_Name", "Name"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ClothingAsset_Label_Name", "Name"))
		.Font(DetailFontInfo)
	]
	.ValueContent()
	.HAlign(HAlign_Fill)
	[
		SNew(STextBlock)
		.Text(FText::FromString(CurrentAsset->GetName()))
	];

	ChildrenBuilder.AddCustomRow(LOCTEXT("ClothingAsset_Search_Details", "Details"))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Details", "Details"))
		.Font(DetailFontInfo)
	]
	.ValueContent()
	.HAlign(HAlign_Fill)
	[
		MakeClothingDetailsWidget(ElementIndex)
	];	
	
	// Properties are now inside UClothingAsset, so we just add a new inspector and handle everything through that
	FDetailWidgetRow& ClothPropRow = ChildrenBuilder.AddCustomRow(LOCTEXT("ClothingAsset_Search_Properties", "Properties"));

	TSharedPtr<SKismetInspector> Inspector = nullptr;

	ClothPropRow.WholeRowWidget
	[
		SNew(SExpandableArea)
		.InitiallyCollapsed(true)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Properties_Header", "Clothing Properties"))
		]
		.BodyContent()
		[
			SAssignNew(Inspector, SKismetInspector)
			.ShowTitleArea(false)
			.ShowPublicViewControl(false)
			.HideNameArea(true)
			.IsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &FPersonaMeshDetails::IsClothingPanelEnabled))
			.OnFinishedChangingProperties(FOnFinishedChangingProperties::FDelegate::CreateSP(this, &FPersonaMeshDetails::OnFinishedChangingClothingProperties, ElementIndex))
		]
	];

	SKismetInspector::FShowDetailsOptions Options;
	Options.bHideFilterArea = true;
	Options.bShowComponents = false;

	Inspector->ShowDetailsForSingleObject(CurrentAsset, Options);
}

TSharedRef<SUniformGridPanel> FPersonaMeshDetails::MakeClothingDetailsWidget(int32 AssetIndex) const
{
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();

	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	UClothingAsset* ClothingAsset = Cast<UClothingAsset>(SkelMesh->MeshClothingAssets[AssetIndex]);
	check(ClothingAsset);

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(2.0f);

	int32 NumLODs = ClothingAsset->LodData.Num();
	int32 RowNumber = 0;

	for(int32 LODIndex=0; LODIndex < NumLODs; LODIndex++)
	{
		Grid->AddSlot(0, RowNumber) // x, y
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Font(DetailFontInfo)
			.Text(FText::Format(LOCTEXT("LODIndex", "LOD {0}"), FText::AsNumber(LODIndex)))			
		];

		RowNumber++;

		FClothLODData& LodData = ClothingAsset->LodData[LODIndex];
		FClothPhysicalMeshData& PhysMeshData = LodData.PhysicalMeshData;
		FClothCollisionData& CollisionData = LodData.CollisionData;

		Grid->AddSlot(0, RowNumber) 
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("SimulVertexCount", "Simul Verts"))
			];

		Grid->AddSlot(0, RowNumber + 1)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(FText::AsNumber(PhysMeshData.Vertices.Num() - PhysMeshData.NumFixedVerts))
			];

		Grid->AddSlot(1, RowNumber)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("FixedVertexCount", "Fixed Verts"))
			];

		Grid->AddSlot(1, RowNumber + 1)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(FText::AsNumber(PhysMeshData.NumFixedVerts))
			];

		Grid->AddSlot(2, RowNumber)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("TriangleCount", "Sim Triangles"))
			];

		Grid->AddSlot(2, RowNumber + 1)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(FText::AsNumber(PhysMeshData.Indices.Num() / 3))
			];

		Grid->AddSlot(3, RowNumber)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("NumUsedBones", "Bones"))
			];

		Grid->AddSlot(3, RowNumber + 1)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(FText::AsNumber(PhysMeshData.MaxBoneWeights))
			];

		Grid->AddSlot(4, RowNumber)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(LOCTEXT("NumBoneSpheres", "Spheres"))
			];

		Grid->AddSlot(4, RowNumber + 1)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DetailFontInfo)
				.Text(FText::AsNumber(CollisionData.Spheres.Num()))
			];

		RowNumber += 2;
	}

	return Grid;
}

FReply FPersonaMeshDetails::OnReimportApexFileClicked(int32 AssetIndex, IDetailLayoutBuilder* DetailLayout)
{
#if WITH_APEX_CLOTHING
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	check(SkelMesh && SkelMesh->MeshClothingAssets.IsValidIndex(AssetIndex));

	UClothingAssetBase* AssetToReimport = SkelMesh->MeshClothingAssets[AssetIndex];
	check(AssetToReimport);

	FString ReimportPath = AssetToReimport->ImportedFilePath;

	if(ReimportPath.IsEmpty())
	{
		const FText MessageText = LOCTEXT("Warning_NoReimportPath", "There is no reimport path available for this asset, it was likely created in the Editor. Would you like to select a file and overwrite this asset?");
		EAppReturnType::Type MessageReturn = FMessageDialog::Open(EAppMsgType::YesNo, MessageText);

		if(MessageReturn == EAppReturnType::Yes)
		{
			ReimportPath = ApexClothingUtils::PromptForClothingFile();
		}
	}

	if(ReimportPath.IsEmpty())
	{
		return FReply::Handled();
	}

	// Retry if the file isn't there
	if(!FPaths::FileExists(ReimportPath))
	{
		const FText MessageText = LOCTEXT("Warning_NoFileFound", "Could not find an asset to reimport, select a new file on disk?");
		EAppReturnType::Type MessageReturn = FMessageDialog::Open(EAppMsgType::YesNo, MessageText);

		if(MessageReturn == EAppReturnType::Yes)
		{
			ReimportPath = ApexClothingUtils::PromptForClothingFile();
		}
	}

	FClothingSystemEditorInterfaceModule& ClothingEditorInterface = FModuleManager::Get().LoadModuleChecked<FClothingSystemEditorInterfaceModule>("ClothingSystemEditorInterface");
	UClothingAssetFactoryBase* Factory = ClothingEditorInterface.GetClothingAssetFactory();

	if(Factory && Factory->CanImport(ReimportPath))
	{
		Factory->Reimport(ReimportPath, SkelMesh, AssetToReimport);

		UpdateClothingEntries();
		RefreshClothingComboBoxes();

		// Force layout to refresh
		DetailLayout->ForceRefreshDetails();
	}
#endif

	return FReply::Handled();
}

FReply FPersonaMeshDetails::OnRemoveApexFileClicked(int32 AssetIndex, IDetailLayoutBuilder* DetailLayout)
{
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();
	check(SkelMesh);

	TArray<UActorComponent*> ComponentsToReregister;
	for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		if(USkeletalMesh* UsedMesh = (*It)->SkeletalMesh)
		{
			if(UsedMesh == SkelMesh)
			{
				ComponentsToReregister.Add(*It);
			}
		}
	}

	{
		// Need to unregister our components so they shut down their current clothing simulation
		FMultiComponentReregisterContext ReregisterContext(ComponentsToReregister);

		// Now we can remove the asset.
		if(SkelMesh->MeshClothingAssets.IsValidIndex(AssetIndex))
		{
			UClothingAssetBase* AssetToRemove = SkelMesh->MeshClothingAssets[AssetIndex];
			check(AssetToRemove);

			AssetToRemove->UnbindFromSkeletalMesh(SkelMesh);

			SkelMesh->MeshClothingAssets.RemoveAt(AssetIndex);

			// Need to fix up asset indices on sections.
			if(FSkeletalMeshModel* MeshResource = SkelMesh->GetImportedModel())
			{
				for(FSkeletalMeshLODModel& LodModel : MeshResource->LODModels)
				{
					for(FSkelMeshSection& Section : LodModel.Sections)
					{
						if(Section.CorrespondClothAssetIndex > AssetIndex)
						{
							--Section.CorrespondClothAssetIndex;
						}
					}
				}
			}

			// Need to regenerate derived data after changing LOD model
			SkelMesh->PostEditChange();
		}
	}

	UpdateClothingEntries();
	RefreshClothingComboBoxes();

	// Force layout to refresh
	DetailLayout->ForceRefreshDetails();
	
	return FReply::Handled();
}

FReply FPersonaMeshDetails::OnOpenClothingFileClicked(IDetailLayoutBuilder* DetailLayout)
{
#if WITH_APEX_CLOTHING
	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	if(SkelMesh)
	{
		ApexClothingUtils::PromptAndImportClothing(SkelMesh);
		
		UpdateClothingEntries();
		RefreshClothingComboBoxes();
	}
#endif

	return FReply::Handled();
}

void FPersonaMeshDetails::UpdateClothingEntries()
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	NewClothingAssetEntries.Empty();

	ClothingNoneEntry = MakeShared<FClothingEntry>();
	ClothingNoneEntry->AssetIndex = INDEX_NONE;
	ClothingNoneEntry->Asset = nullptr;

	NewClothingAssetEntries.Add(ClothingNoneEntry);

	const int32 NumClothingAssets = Mesh->MeshClothingAssets.Num();
	for(int32 Idx = 0; Idx < NumClothingAssets; ++Idx)
	{
		UClothingAsset* Asset = CastChecked<UClothingAsset>(Mesh->MeshClothingAssets[Idx]);

		const int32 NumAssetLods = Asset->LodData.Num();
		for(int32 AssetLodIndex = 0; AssetLodIndex < NumAssetLods; ++AssetLodIndex)
		{
			TSharedPtr<FClothingEntry> NewEntry = MakeShared<FClothingEntry>();

			NewEntry->Asset = Mesh->MeshClothingAssets[Idx];
			NewEntry->AssetIndex = Idx;
			NewEntry->AssetLodIndex = AssetLodIndex;

			NewClothingAssetEntries.Add(NewEntry);
		}
	}
}

void FPersonaMeshDetails::RefreshClothingComboBoxes()
{
	for(const SClothComboBoxPtr& BoxPtr : ClothComboBoxes)
	{
		if(BoxPtr.IsValid())
		{
			BoxPtr->RefreshOptions();
		}
	}
}

void FPersonaMeshDetails::OnClothingComboBoxOpening()
{
	UpdateClothingEntries();
	RefreshClothingComboBoxes();
}

TSharedRef<SWidget> FPersonaMeshDetails::OnGenerateWidgetForClothingEntry(TSharedPtr<FClothingEntry> InEntry)
{
	UClothingAsset* Asset = Cast<UClothingAsset>(InEntry->Asset.Get());

	FText EntryText;
	if(Asset)
	{
		EntryText = FText::Format(LOCTEXT("ClothingAssetEntry_Name", "{0} - LOD{1}"), FText::FromString(Asset->GetName()), FText::AsNumber(InEntry->AssetLodIndex));
	}
	else
	{
		EntryText = LOCTEXT("NoClothingEntry", "None");
	}

	return SNew(STextBlock)
		.Text(EntryText);
}

FText FPersonaMeshDetails::OnGetClothingComboText(int32 InLodIdx, int32 InSectionIdx) const
{
	USkeletalMesh* Mesh = GetPersonaToolkit()->GetMesh();

	if(Mesh)
	{
		UClothingAsset* ClothingAsset = Cast<UClothingAsset>(Mesh->GetSectionClothingAsset(InLodIdx, InSectionIdx));

		if(ClothingAsset && ClothingAsset->LodMap.IsValidIndex(InLodIdx))
		{
			const int32 ClothingLOD = ClothingAsset->LodMap[InLodIdx];
			return FText::Format(LOCTEXT("ClothingAssetEntry_Name", "{0} - LOD{1}"), FText::FromString(ClothingAsset->GetName()), FText::AsNumber(ClothingLOD));
		}
	}

	return LOCTEXT("ClothingCombo_None", "None");
}

void FPersonaMeshDetails::OnClothingSelectionChanged(TSharedPtr<FClothingEntry> InNewEntry, ESelectInfo::Type InSelectType, int32 BoxIndex, int32 InLodIdx, int32 InSectionIdx)
{
	if(InNewEntry.IsValid())
	{
		USkeletalMesh* Mesh = SkeletalMeshPtr.Get();

		if(UClothingAsset* ClothingAsset = Cast<UClothingAsset>(InNewEntry->Asset.Get()))
		{
			// Look for a currently bound asset an unbind it if necessary first
			if(UClothingAssetBase* CurrentAsset = Mesh->GetSectionClothingAsset(InLodIdx, InSectionIdx))
			{
				CurrentAsset->UnbindFromSkeletalMesh(Mesh, InLodIdx);
			}

			if(!ClothingAsset->BindToSkeletalMesh(Mesh, InLodIdx, InSectionIdx, InNewEntry->AssetLodIndex))
			{
				// We failed to bind the clothing asset, reset box selection to "None"
				SClothComboBoxPtr BoxPtr = ClothComboBoxes[BoxIndex];
				if(BoxPtr.IsValid())
				{
					BoxPtr->SetSelectedItem(ClothingNoneEntry);
				}
			}
		}
		else if(Mesh)
		{
			if(UClothingAssetBase* CurrentAsset = Mesh->GetSectionClothingAsset(InLodIdx, InSectionIdx))
			{
				CurrentAsset->UnbindFromSkeletalMesh(Mesh, InLodIdx);
			}
		}
	}
}

bool FPersonaMeshDetails::IsClothingPanelEnabled() const
{
	return !GEditor->bIsSimulatingInEditor && !GEditor->PlayWorld;
}

void FPersonaMeshDetails::OnFinishedChangingClothingProperties(const FPropertyChangedEvent& Event, int32 InAssetIndex)
{
	if(Event.ChangeType != EPropertyChangeType::Interactive)
	{
		if(Event.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FClothConfig, SelfCollisionRadius) ||
			Event.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FClothConfig, SelfCollisionCullScale))
		{
			USkeletalMesh* CurrentMesh = GetPersonaToolkit()->GetMesh();
			if(CurrentMesh->MeshClothingAssets.IsValidIndex(InAssetIndex))
			{
				UClothingAsset* Asset = CastChecked<UClothingAsset>(CurrentMesh->MeshClothingAssets[InAssetIndex]);

				Asset->BuildSelfCollisionData();
			}
		}
	}

	if(UDebugSkelMeshComponent* PreviewComponent = GetPersonaToolkit()->GetPreviewMeshComponent())
	{
		// Reregister our preview component to apply the change
		FComponentReregisterContext Context(PreviewComponent);
	}
}

bool FPersonaMeshDetails::CanDeleteMaterialElement(int32 LODIndex, int32 SectionIndex) const
{
	// Only allow deletion of extra elements
	return SectionIndex != 0;
}

FReply FPersonaMeshDetails::OnDeleteButtonClicked(int32 LODIndex, int32 SectionIndex)
{
	ensure(SectionIndex != 0);

	int32 MaterialIndex = GetMaterialIndex(LODIndex, SectionIndex);

	USkeletalMesh* SkelMesh = GetPersonaToolkit()->GetMesh();

	// Move any mappings pointing to the requested material to point to the first
	// and decrement any above it
	if(SkelMesh)
	{

		const FScopedTransaction Transaction(LOCTEXT("PersonaOnDeleteButtonClickedTransaction", "Persona editor: Delete material slot"));
		UProperty* MaterialProperty = FindField<UProperty>( USkeletalMesh::StaticClass(), "Materials" );
		SkelMesh->PreEditChange( MaterialProperty );

		// Patch up LOD mapping indices
		int32 NumLODInfos = SkelMesh->GetLODNum();
		for(int32 LODInfoIdx=0; LODInfoIdx < NumLODInfos; LODInfoIdx++)
		{
			for(auto LodMaterialIter = SkelMesh->GetLODInfo(LODInfoIdx)->LODMaterialMap.CreateIterator() ; LodMaterialIter ; ++LodMaterialIter)
			{
				int32 CurrentMapping = *LodMaterialIter;

				if (CurrentMapping == MaterialIndex)
				{
					// Set to first material
					*LodMaterialIter = 0;
				}
				else if (CurrentMapping > MaterialIndex)
				{
					// Decrement to keep correct reference after removal
					*LodMaterialIter = CurrentMapping - 1;
				}
			}
		}
		
		// Patch up section indices
		for(auto ModelIter = SkelMesh->GetImportedModel()->LODModels.CreateIterator() ; ModelIter ; ++ModelIter)
		{
			FSkeletalMeshLODModel& Model = *ModelIter;
			for(auto SectionIter = Model.Sections.CreateIterator() ; SectionIter ; ++SectionIter)
			{
				FSkelMeshSection& Section = *SectionIter;

				if (Section.MaterialIndex == MaterialIndex)
				{
					Section.MaterialIndex = 0;
				}
				else if (Section.MaterialIndex > MaterialIndex)
				{
					Section.MaterialIndex--;
				}
			}
		}

		SkelMesh->Materials.RemoveAt(MaterialIndex);

		// Notify the change in material
		FPropertyChangedEvent PropertyChangedEvent( MaterialProperty );
		SkelMesh->PostEditChangeProperty(PropertyChangedEvent);
	}

	return FReply::Handled();
}

void FPersonaMeshDetails::OnPreviewMeshChanged(USkeletalMesh* OldSkeletalMesh, USkeletalMesh* NewMesh)
{
	if (IsApplyNeeded())
	{
		MeshDetailLayout->ForceRefreshDetails();
	}
}

bool FPersonaMeshDetails::FilterOutBakePose(const FAssetData& AssetData, USkeleton* Skeleton) const
{
	FString SkeletonName;
	AssetData.GetTagValue("Skeleton", SkeletonName);
	FAssetData SkeletonData(Skeleton);
	return (SkeletonName != SkeletonData.GetExportTextName());
}

#undef LOCTEXT_NAMESPACE
