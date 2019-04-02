// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolHelpers.h"
#include "MovieSceneToolsModule.h"
#include "MovieScene.h"
#include "Layout/Margin.h"
#include "Misc/Paths.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "AssetData.h"
#include "Containers/ArrayView.h"
#include "ISequencer.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SComboBox.h"
#include "ScopedTransaction.h"
#include "EditorStyleSet.h"
#include "EditorDirectories.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "LevelSequence.h"
#include "AssetRegistryModule.h"
#include "DesktopPlatformModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "MovieSceneTranslatorEDL.h"
#include "MessageLogModule.h"
#include "IMessageLogListing.h"
#include "FbxImporter.h"
#include "MatineeImportTools.h"
#include "MovieSceneToolsProjectSettings.h"
#include "MovieSceneToolsUserSettings.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Math/UnitConversion.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Input/SButton.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "AssetToolsModule.h"
#include "Camera/CameraAnim.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackInstMove.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"

/* MovieSceneToolHelpers
 *****************************************************************************/

void MovieSceneToolHelpers::TrimSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, FQualifiedFrameTime Time, bool bTrimLeft)
{
	for (auto Section : Sections)
	{
		if (Section.IsValid())
		{
			Section->TrimSection(Time, bTrimLeft);
		}
	}
}


void MovieSceneToolHelpers::SplitSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, FQualifiedFrameTime Time)
{
	for (auto Section : Sections)
	{
		if (Section.IsValid())
		{
			Section->SplitSection(Time);
		}
	}
}

bool MovieSceneToolHelpers::ParseShotName(const FString& ShotName, FString& ShotPrefix, uint32& ShotNumber, uint32& TakeNumber)
{
	// Parse a shot name
	// 
	// sht010:
	//  ShotPrefix = sht
	//  ShotNumber = 10
	//  TakeNumber = 1 (default)
	// 
	// sp020_002
	//  ShotPrefix = sp
	//  ShotNumber = 20
	//  TakeNumber = 2
	//
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	uint32 FirstShotNumberIndex = INDEX_NONE;
	uint32 LastShotNumberIndex = INDEX_NONE;
	bool bInShotNumber = false;

	uint32 FirstTakeNumberIndex = INDEX_NONE;
	uint32 LastTakeNumberIndex = INDEX_NONE;
	bool bInTakeNumber = false;

	bool bFoundTakeSeparator = false;
	TOptional<uint32> ParsedTakeNumber;
	TakeNumber = ProjectSettings->FirstTakeNumber;

	for (int32 CharIndex = 0; CharIndex < ShotName.Len(); ++CharIndex)
	{
		if (FChar::IsDigit(ShotName[CharIndex]))
		{
			// Find shot number indices
			if (FirstShotNumberIndex == INDEX_NONE)
			{
				bInShotNumber = true;
				FirstShotNumberIndex = CharIndex;
			}
			if (bInShotNumber)
			{
				LastShotNumberIndex = CharIndex;
			}

			if (FirstShotNumberIndex != INDEX_NONE && LastShotNumberIndex != INDEX_NONE)
			{
				if (bFoundTakeSeparator)
				{
					// Find take number indices
					if (FirstTakeNumberIndex == INDEX_NONE)
					{
						bInTakeNumber = true;
						FirstTakeNumberIndex = CharIndex;
					}
					if (bInTakeNumber)
					{
						LastTakeNumberIndex = CharIndex;
					}
				}
			}
		}

		if (FirstShotNumberIndex != INDEX_NONE && LastShotNumberIndex != INDEX_NONE)
		{
			if (ShotName[CharIndex] == ProjectSettings->TakeSeparator[0])
			{
				bFoundTakeSeparator = true;
			}
		}
	}

	if (FirstShotNumberIndex != INDEX_NONE)
	{
		ShotPrefix = ShotName.Left(FirstShotNumberIndex);
		ShotNumber = FCString::Atoi(*ShotName.Mid(FirstShotNumberIndex, LastShotNumberIndex-FirstShotNumberIndex+1));
	}

	if (FirstTakeNumberIndex != INDEX_NONE)
	{
		FString TakeStr = ShotName.Mid(FirstTakeNumberIndex, LastTakeNumberIndex-FirstTakeNumberIndex+1);
		if (TakeStr.IsNumeric())
		{
			ParsedTakeNumber = FCString::Atoi(*TakeStr);
		}
	}

	// If take number wasn't found, search backwards to find the first take separator and assume [shot prefix]_[take number]
	//
	if (!ParsedTakeNumber.IsSet())
	{
		int32 LastSlashPos = ShotName.Find(ProjectSettings->TakeSeparator, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (LastSlashPos != INDEX_NONE)
		{
			ShotPrefix = ShotName.Left(LastSlashPos);
			ShotNumber = INDEX_NONE; // Nullify the shot number since we only have a shot prefix
			TakeNumber = FCString::Atoi(*ShotName.RightChop(LastSlashPos+1));
			return true;
		}
	}

	if (ParsedTakeNumber.IsSet())
	{
		TakeNumber = ParsedTakeNumber.GetValue();
	}

	return FirstShotNumberIndex != INDEX_NONE;
}


FString MovieSceneToolHelpers::ComposeShotName(const FString& ShotPrefix, uint32 ShotNumber, uint32 TakeNumber)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	FString ShotName = ShotPrefix;

	if (ShotNumber != INDEX_NONE)
	{
		ShotName += FString::Printf(TEXT("%0*d"), ProjectSettings->ShotNumDigits, ShotNumber);
	}

	if (TakeNumber != INDEX_NONE)
	{
		FString TakeFormat = TEXT("%0") + FString::Printf(TEXT("%d"), ProjectSettings->TakeNumDigits) + TEXT("d");
		
		ShotName += ProjectSettings->TakeSeparator;
		ShotName += FString::Printf(TEXT("%0*d"), ProjectSettings->TakeNumDigits, TakeNumber);
	}
	return ShotName;
}

bool IsPackageNameUnique(const TArray<FAssetData>& ObjectList, FString& NewPackageName)
{
	for (auto AssetObject : ObjectList)
	{
		if (AssetObject.PackageName.ToString() == NewPackageName)
		{
			return false;
		}
	}
	return true;
}

FString MovieSceneToolHelpers::GenerateNewShotPath(UMovieScene* SequenceMovieScene, FString& NewShotName)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> ObjectList;
	AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), ObjectList);

	UObject* SequenceAsset = SequenceMovieScene->GetOuter();
	UPackage* SequencePackage = SequenceAsset->GetOutermost();
	FString SequencePackageName = SequencePackage->GetName(); // ie. /Game/cine/max/master
	int32 LastSlashPos = SequencePackageName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	FString SequencePath = SequencePackageName.Left(LastSlashPos);

	FString NewShotPrefix;
	uint32 NewShotNumber = INDEX_NONE;
	uint32 NewTakeNumber = INDEX_NONE;
	ParseShotName(NewShotName, NewShotPrefix, NewShotNumber, NewTakeNumber);

	FString NewShotDirectory = ComposeShotName(NewShotPrefix, NewShotNumber, INDEX_NONE);
	FString NewShotPath = SequencePath;

	FString ShotDirectory = ProjectSettings->ShotDirectory;
	if (!ShotDirectory.IsEmpty())
	{
		NewShotPath /= ShotDirectory;
	}
	NewShotPath /= NewShotDirectory; // put this in the shot directory, ie. /Game/cine/max/shots/shot0010

	// Make sure this shot path is unique
	FString NewPackageName = NewShotPath;
	NewPackageName /= NewShotName; // ie. /Game/cine/max/shots/shot0010/shot0010_001
	if (!IsPackageNameUnique(ObjectList, NewPackageName))
	{
		while (1)
		{
			NewShotNumber += ProjectSettings->ShotIncrement;
			NewShotName = ComposeShotName(NewShotPrefix, NewShotNumber, NewTakeNumber);
			NewShotDirectory = ComposeShotName(NewShotPrefix, NewShotNumber, INDEX_NONE);
			NewShotPath = SequencePath;
			if (!ShotDirectory.IsEmpty())
			{
				NewShotPath /= ShotDirectory;
			}
			NewShotPath /= NewShotDirectory;

			NewPackageName = NewShotPath;
			NewPackageName /= NewShotName;
			if (IsPackageNameUnique(ObjectList, NewPackageName))
			{
				break;
			}
		}
	}

	return NewShotPath;
}


FString MovieSceneToolHelpers::GenerateNewShotName(const TArray<UMovieSceneSection*>& AllSections, FFrameNumber Time)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	UMovieSceneCinematicShotSection* BeforeShot = nullptr;
	UMovieSceneCinematicShotSection* NextShot = nullptr;

	FFrameNumber MinEndDiff = TNumericLimits<int32>::Max();
	FFrameNumber MinStartDiff = TNumericLimits<int32>::Max();

	for (auto Section : AllSections)
	{
		if (Section->HasEndFrame() && Section->GetExclusiveEndFrame() >= Time)
		{
			FFrameNumber EndDiff = Section->GetExclusiveEndFrame() - Time;
			if (MinEndDiff > EndDiff)
			{
				MinEndDiff = EndDiff;
				BeforeShot = Cast<UMovieSceneCinematicShotSection>(Section);
			}
		}
		if (Section->HasStartFrame() && Section->GetInclusiveStartFrame() <= Time)
		{
			FFrameNumber StartDiff = Time - Section->GetInclusiveStartFrame();
			if (MinStartDiff > StartDiff)
			{
				MinStartDiff = StartDiff;
				NextShot = Cast<UMovieSceneCinematicShotSection>(Section);
			}
		}
	}
	
	// There aren't any shots, let's create the first shot name
	if (BeforeShot == nullptr || NextShot == nullptr)
	{
		// Default case
	}
	// This is the last shot
	else if (BeforeShot == NextShot)
	{
		FString NextShotPrefix = ProjectSettings->ShotPrefix;
		uint32 NextShotNumber = ProjectSettings->FirstShotNumber;
		uint32 NextTakeNumber = ProjectSettings->FirstTakeNumber;

		if (ParseShotName(NextShot->GetShotDisplayName(), NextShotPrefix, NextShotNumber, NextTakeNumber))
		{
			uint32 NewShotNumber = NextShotNumber + ProjectSettings->ShotIncrement;
			return ComposeShotName(NextShotPrefix, NewShotNumber, ProjectSettings->FirstTakeNumber);
		}
	}
	// This is in between two shots
	else 
	{
		FString BeforeShotPrefix = ProjectSettings->ShotPrefix;
		uint32 BeforeShotNumber = ProjectSettings->FirstShotNumber;
		uint32 BeforeTakeNumber = ProjectSettings->FirstTakeNumber;

		FString NextShotPrefix = ProjectSettings->ShotPrefix;
		uint32 NextShotNumber = ProjectSettings->FirstShotNumber;
		uint32 NextTakeNumber = ProjectSettings->FirstTakeNumber;

		if (ParseShotName(BeforeShot->GetShotDisplayName(), BeforeShotPrefix, BeforeShotNumber, BeforeTakeNumber) &&
			ParseShotName(NextShot->GetShotDisplayName(), NextShotPrefix, NextShotNumber, NextTakeNumber))
		{
			if (BeforeShotNumber < NextShotNumber)
			{
				uint32 NewShotNumber = BeforeShotNumber + ( (NextShotNumber - BeforeShotNumber) / 2); // what if we can't find one? or conflicts with another?
				return ComposeShotName(BeforeShotPrefix, NewShotNumber, ProjectSettings->FirstTakeNumber);
			}
		}
	}

	// Default case
	return ComposeShotName(ProjectSettings->ShotPrefix, ProjectSettings->FirstShotNumber, ProjectSettings->FirstTakeNumber);
}

void MovieSceneToolHelpers::GatherTakes(const UMovieSceneSection* Section, TArray<uint32>& TakeNumbers, uint32& CurrentTakeNumber)
{
	const UMovieSceneSubSection* SubSection = Cast<const UMovieSceneSubSection>(Section);
	
	if (SubSection->GetSequence() == nullptr)
	{
		return;
	}

	if (FMovieSceneToolsModule::Get().GatherTakes(Section, TakeNumbers, CurrentTakeNumber))
	{
		return;
	}

	FAssetData ShotData(SubSection->GetSequence()->GetOuter());

	FString ShotPackagePath = ShotData.PackagePath.ToString();

	FString ShotPrefix;
	uint32 ShotNumber = INDEX_NONE;
	CurrentTakeNumber = INDEX_NONE;

	FString SubSectionName = SubSection->GetSequence()->GetName();
	if (SubSection->IsA<UMovieSceneCinematicShotSection>())
	{
		const UMovieSceneCinematicShotSection* ShotSection = Cast<UMovieSceneCinematicShotSection>(SubSection);
		SubSectionName = ShotSection->GetShotDisplayName();
	}

	if (ParseShotName(SubSectionName, ShotPrefix, ShotNumber, CurrentTakeNumber))
	{
		// Gather up all level sequence assets
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> ObjectList;
		AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), ObjectList);

		for (auto AssetObject : ObjectList)
		{
			FString AssetPackagePath = AssetObject.PackagePath.ToString();

			if (AssetPackagePath == ShotPackagePath)
			{
				FString AssetShotPrefix;
				uint32 AssetShotNumber = INDEX_NONE;
				uint32 AssetTakeNumber = INDEX_NONE;

				if (ParseShotName(AssetObject.AssetName.ToString(), AssetShotPrefix, AssetShotNumber, AssetTakeNumber))
				{
					if (AssetShotPrefix == ShotPrefix && AssetShotNumber == ShotNumber)
					{
						TakeNumbers.Add(AssetTakeNumber);
					}
				}
			}
		}
	}

	TakeNumbers.Sort();
}

UObject* MovieSceneToolHelpers::GetTake(const UMovieSceneSection* Section, uint32 TakeNumber)
{
	const UMovieSceneSubSection* SubSection = Cast<const UMovieSceneSubSection>(Section);

	if (UObject* TakeObject = FMovieSceneToolsModule::Get().GetTake(Section, TakeNumber))
	{
		return TakeObject;
	}

	FAssetData ShotData(SubSection->GetSequence()->GetOuter());

	FString ShotPackagePath = ShotData.PackagePath.ToString();
	int32 ShotLastSlashPos = INDEX_NONE;
	ShotPackagePath.FindLastChar(TCHAR('/'), ShotLastSlashPos);
	ShotPackagePath = ShotPackagePath.Left(ShotLastSlashPos);

	FString ShotPrefix;
	uint32 ShotNumber = INDEX_NONE;
	uint32 TakeNumberDummy = INDEX_NONE;

	FString SubSectionName = SubSection->GetSequence()->GetName();
	if (SubSection->IsA<UMovieSceneCinematicShotSection>())
	{
		const UMovieSceneCinematicShotSection* ShotSection = Cast<UMovieSceneCinematicShotSection>(SubSection);
		SubSectionName = ShotSection->GetShotDisplayName();
	}

	if (ParseShotName(SubSectionName, ShotPrefix, ShotNumber, TakeNumberDummy))
	{
		// Gather up all level sequence assets
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> ObjectList;
		AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), ObjectList);

		for (auto AssetObject : ObjectList)
		{
			FString AssetPackagePath = AssetObject.PackagePath.ToString();
			int32 AssetLastSlashPos = INDEX_NONE;
			AssetPackagePath.FindLastChar(TCHAR('/'), AssetLastSlashPos);
			AssetPackagePath = AssetPackagePath.Left(AssetLastSlashPos);

			if (AssetPackagePath == ShotPackagePath)
			{
				FString AssetShotPrefix;
				uint32 AssetShotNumber = INDEX_NONE;
				uint32 AssetTakeNumber = INDEX_NONE;

				if (ParseShotName(AssetObject.AssetName.ToString(), AssetShotPrefix, AssetShotNumber, AssetTakeNumber))
				{
					if (AssetShotPrefix == ShotPrefix && AssetShotNumber == ShotNumber && TakeNumber == AssetTakeNumber)
					{
						return AssetObject.GetAsset();
					}
				}
			}
		}
	}

	return nullptr;
}

int32 MovieSceneToolHelpers::FindAvailableRowIndex(UMovieSceneTrack* InTrack, UMovieSceneSection* InSection)
{
	for (int32 RowIndex = 0; RowIndex <= InTrack->GetMaxRowIndex(); ++RowIndex)
	{
		bool bFoundIntersect = false;
		for (UMovieSceneSection* Section : InTrack->GetAllSections())
		{
			if (!Section->HasStartFrame() || !Section->HasEndFrame() || !InSection->HasStartFrame() || !InSection->HasEndFrame())
			{
				bFoundIntersect = true;
				break;
			}

			if (Section != InSection && Section->GetRowIndex() == RowIndex && Section->GetRange().Overlaps(InSection->GetRange()))
			{
				bFoundIntersect = true;
				break;
			}
		}
		if (!bFoundIntersect)
		{
			return RowIndex;
		}
	}

	return InTrack->GetMaxRowIndex() + 1;
}

class SEnumCombobox : public SComboBox<TSharedPtr<int32>>
{
public:
	SLATE_BEGIN_ARGS(SEnumCombobox) {}

	SLATE_ATTRIBUTE(int32, CurrentValue)
	SLATE_ARGUMENT(FOnEnumSelectionChanged, OnEnumSelectionChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const UEnum* InEnum)
	{
		Enum = InEnum;
		CurrentValue = InArgs._CurrentValue;
		check(CurrentValue.IsBound());
		OnEnumSelectionChangedDelegate = InArgs._OnEnumSelectionChanged;

		bUpdatingSelectionInternally = false;

		for (int32 i = 0; i < Enum->NumEnums() - 1; i++)
		{
			if (Enum->HasMetaData( TEXT("Hidden"), i ) == false)
			{
				VisibleEnumNameIndices.Add(MakeShareable(new int32(i)));
			}
		}

		SComboBox::Construct(SComboBox<TSharedPtr<int32>>::FArguments()
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Light")
			.OptionsSource(&VisibleEnumNameIndices)
			.OnGenerateWidget_Lambda([this](TSharedPtr<int32> InItem)
			{
				return SNew(STextBlock)
					.Text(Enum->GetDisplayNameTextByIndex(*InItem));
			})
			.OnSelectionChanged(this, &SEnumCombobox::OnComboSelectionChanged)
			.OnComboBoxOpening(this, &SEnumCombobox::OnComboMenuOpening)
			.ContentPadding(FMargin(2, 0))
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
				.Text(this, &SEnumCombobox::GetCurrentValue)
			]);
	}

private:
	FText GetCurrentValue() const
	{
		int32 CurrentNameIndex = Enum->GetIndexByValue(CurrentValue.Get());
		return Enum->GetDisplayNameTextByIndex(CurrentNameIndex);
	}

	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<int32> InItem)
	{
		return SNew(STextBlock)
			.Text(Enum->GetDisplayNameTextByIndex(*InItem));
	}

	void OnComboSelectionChanged(TSharedPtr<int32> InSelectedItem, ESelectInfo::Type SelectInfo)
	{
		if (bUpdatingSelectionInternally == false)
		{
			OnEnumSelectionChangedDelegate.ExecuteIfBound(*InSelectedItem, SelectInfo);
		}
	}

	void OnComboMenuOpening()
	{
		int32 CurrentNameIndex = Enum->GetIndexByValue(CurrentValue.Get());
		TSharedPtr<int32> FoundNameIndexItem;
		for ( int32 i = 0; i < VisibleEnumNameIndices.Num(); i++ )
		{
			if ( *VisibleEnumNameIndices[i] == CurrentNameIndex )
			{
				FoundNameIndexItem = VisibleEnumNameIndices[i];
				break;
			}
		}
		if ( FoundNameIndexItem.IsValid() )
		{
			bUpdatingSelectionInternally = true;
			SetSelectedItem(FoundNameIndexItem);
			bUpdatingSelectionInternally = false;
		}
	}	

private:
	const UEnum* Enum;

	TAttribute<int32> CurrentValue;

	TArray<TSharedPtr<int32>> VisibleEnumNameIndices;

	bool bUpdatingSelectionInternally;

	FOnEnumSelectionChanged OnEnumSelectionChangedDelegate;
};

TSharedRef<SWidget> MovieSceneToolHelpers::MakeEnumComboBox(const UEnum* InEnum, TAttribute<int32> InCurrentValue, FOnEnumSelectionChanged InOnSelectionChanged)
{
	return SNew(SEnumCombobox, InEnum)
		.CurrentValue(InCurrentValue)
		.OnEnumSelectionChanged(InOnSelectionChanged);
}

bool MovieSceneToolHelpers::ShowImportEDLDialog(UMovieScene* InMovieScene, FFrameRate InFrameRate, FString InOpenDirectory)
{
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		FString ExtensionStr;
		ExtensionStr += TEXT("CMX 3600 EDL (*.edl)|*.edl|");

		bOpen = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("MovieSceneToolHelpers", "ImportEDL", "Import EDL from...").ToString(), 
			InOpenDirectory,
			TEXT(""), 
			*ExtensionStr,
			EFileDialogFlags::None,
			OpenFilenames
			);
	}
	if (!bOpen)
	{
		return false;
	}

	if (!OpenFilenames.Num())
	{
		return false;
	}

	const FScopedTransaction Transaction( NSLOCTEXT( "MovieSceneTools", "ImportEDLTransaction", "Import EDL" ) );

	return MovieSceneTranslatorEDL::ImportEDL(InMovieScene, InFrameRate, OpenFilenames[0]);
}

bool MovieSceneToolHelpers::ShowExportEDLDialog(const UMovieScene* InMovieScene, FFrameRate InFrameRate, FString InSaveDirectory, int32 InHandleFrames, FString InMovieExtension)
{
	TArray<FString> SaveFilenames;
	FString SequenceName = InMovieScene->GetOuter()->GetName();

	// Pop open a dialog to request the location of the edl
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bSave = false;
	if (DesktopPlatform)
	{
		FString ExtensionStr;
		ExtensionStr += TEXT("CMX 3600 EDL (*.edl)|*.edl|");
		ExtensionStr += TEXT("RV (*.rv)|*.rv|");

		bSave = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("MovieSceneTools", "ExportEDL", "Export EDL to...").ToString(), 
			InSaveDirectory,
			SequenceName + TEXT(".edl"), 
			*ExtensionStr,
			EFileDialogFlags::None,
			SaveFilenames
			);
	}
	if (!bSave)
	{
		return false;
	}

	if (!SaveFilenames.Num())
	{
		return false;
	}

	if (MovieSceneTranslatorEDL::ExportEDL(InMovieScene, InFrameRate, SaveFilenames[0], InHandleFrames, InMovieExtension))
	{
		const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(SaveFilenames[0]);
		const FString SaveDirectory = FPaths::GetPath(AbsoluteFilename);

		FNotificationInfo NotificationInfo(NSLOCTEXT("MovieSceneTools", "EDLExportFinished", "EDL Export finished"));
		NotificationInfo.ExpireDuration = 5.f;
		NotificationInfo.Hyperlink = FSimpleDelegate::CreateStatic( [](FString InDirectory) { FPlatformProcess::ExploreFolder( *InDirectory ); }, SaveDirectory);
		NotificationInfo.HyperlinkText = NSLOCTEXT("MovieSceneTools", "OpenEDLExportFolder", "Open EDL Export Folder...");
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		
		return true;
	}

	return false;
}

bool MovieSceneToolHelpers::MovieSceneTranslatorImport(FMovieSceneImporter* InImporter, UMovieScene* InMovieScene, FFrameRate InFrameRate, FString InOpenDirectory)
{
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		FString FileTypeDescription = InImporter->GetFileTypeDescription().ToString();
		FString DialogTitle = InImporter->GetDialogTitle().ToString();

		bOpen = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			DialogTitle,
			InOpenDirectory,
			TEXT(""),
			FileTypeDescription,
			EFileDialogFlags::None,
			OpenFilenames
		);
	}

	if (!bOpen || !OpenFilenames.Num())
	{
		return false;
	}

	FScopedTransaction Transaction(InImporter->GetTransactionDescription());

	TSharedRef<FMovieSceneTranslatorContext> ImportContext(new FMovieSceneTranslatorContext);
	ImportContext->Init();

	bool bSuccess = InImporter->Import(InMovieScene, InFrameRate, OpenFilenames[0], ImportContext);

	// Display any messages in context
	MovieSceneTranslatorLogMessages(InImporter, ImportContext, true);

	// Roll back transaction when import fails.
	if (!bSuccess)
	{
		Transaction.Cancel();
	}

	return bSuccess;
}

bool MovieSceneToolHelpers::MovieSceneTranslatorExport(FMovieSceneExporter* InExporter, const UMovieScene* InMovieScene, const FMovieSceneCaptureSettings& Settings)
{
	if (InExporter == nullptr || InMovieScene == nullptr)
	{
		return false;
	}

	FString SaveDirectory = FPaths::ConvertRelativePathToFull(Settings.OutputDirectory.Path);
	int32 HandleFrames = Settings.HandleFrames;
	// @todo: generate filename based on filename format, currently outputs {shot}.avi
	FString FilenameFormat = Settings.OutputFormat;
	FFrameRate FrameRate = Settings.FrameRate;
	uint32 ResX = Settings.Resolution.ResX;
	uint32 ResY = Settings.Resolution.ResY;
	FString MovieExtension = Settings.MovieExtension;

	TArray<FString> SaveFilenames;
	FString SequenceName = InMovieScene->GetOuter()->GetName();

	// Pop open a dialog to request the location of the edl
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bSave = false;
	if (DesktopPlatform)
	{
		FString FileTypeDescription = InExporter->GetFileTypeDescription().ToString();
		FString DialogTitle = InExporter->GetDialogTitle().ToString();
		FString FileExtension = InExporter->GetDefaultFileExtension().ToString();

		bSave = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			DialogTitle,
			SaveDirectory,
			SequenceName + TEXT(".") + FileExtension,
			FileTypeDescription,
			EFileDialogFlags::None,
			SaveFilenames
		);
	}

	if (!bSave || !SaveFilenames.Num())
	{
		return false;
	}

	TSharedRef<FMovieSceneTranslatorContext> ExportContext(new FMovieSceneTranslatorContext);
	ExportContext->Init();

	bool bSuccess = InExporter->Export(InMovieScene, FilenameFormat, FrameRate, ResX, ResY, HandleFrames, SaveFilenames[0], ExportContext, MovieExtension);
	
	// Display any messages in context
	MovieSceneTranslatorLogMessages(InExporter, ExportContext, true);

	if (bSuccess)
	{
		const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(SaveFilenames[0]);
		const FString ActualSaveDirectory = FPaths::GetPath(AbsoluteFilename);

		FNotificationInfo NotificationInfo(InExporter->GetNotificationExportFinished());
		NotificationInfo.ExpireDuration = 5.f;
		NotificationInfo.Hyperlink = FSimpleDelegate::CreateStatic([](FString InDirectory) { FPlatformProcess::ExploreFolder(*InDirectory); }, ActualSaveDirectory);
		NotificationInfo.HyperlinkText = InExporter->GetNotificationHyperlinkText();
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	}

	return bSuccess;
}

void MovieSceneToolHelpers::MovieSceneTranslatorLogMessages(FMovieSceneTranslator *InTranslator, TSharedRef<FMovieSceneTranslatorContext> InContext, bool bDisplayMessages)
{
	if (InTranslator == nullptr || InContext->GetMessages().Num() == 0)
	{
		return;
	}
	
	// Clear any old messages after an import or export
	const FName LogTitle = InTranslator->GetMessageLogWindowTitle();
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	TSharedRef<IMessageLogListing> LogListing = MessageLogModule.GetLogListing(LogTitle);
	LogListing->SetLabel(InTranslator->GetMessageLogLabel());
	LogListing->ClearMessages();

	for (TSharedRef<FTokenizedMessage> Message : InContext->GetMessages())
	{
		LogListing->AddMessage(Message);
	}

	if (bDisplayMessages)
	{
		MessageLogModule.OpenMessageLog(LogTitle);
	}
}

void MovieSceneToolHelpers::MovieSceneTranslatorLogOutput(FMovieSceneTranslator *InTranslator, TSharedRef<FMovieSceneTranslatorContext> InContext)
{
	if (InTranslator == nullptr || InContext->GetMessages().Num() == 0)
	{
		return;
	}

	for (TSharedRef<FTokenizedMessage> Message : InContext->GetMessages())
	{
		if (Message->GetSeverity() == EMessageSeverity::Error)
		{
			UE_LOG(LogMovieScene, Error, TEXT("%s"), *Message->ToText().ToString());
		}
		else if (Message->GetSeverity() == EMessageSeverity::Warning)
		{
			UE_LOG(LogMovieScene, Warning, TEXT("%s"), *Message->ToText().ToString());

		}
	}
}

bool ImportFBXProperty(FString NodeName, FString AnimatedPropertyName, FGuid ObjectBinding, UnFbx::FFbxCurvesAPI& CurveAPI, UMovieScene* InMovieScene, ISequencer& InSequencer)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();
	const UMovieSceneUserImportFBXSettings* ImportFBXSettings = GetDefault<UMovieSceneUserImportFBXSettings>();

	TArrayView<TWeakObjectPtr<>> BoundObjects = InSequencer.FindBoundObjects(ObjectBinding, InSequencer.GetFocusedTemplateID());

	for (auto FbxSetting : ProjectSettings->FbxSettings)
	{
		if (FCString::Strcmp(*FbxSetting.FbxPropertyName.ToUpper(), *AnimatedPropertyName.ToUpper()) != 0)
		{
			continue;
		}

		for (TWeakObjectPtr<>& WeakObject : BoundObjects)
		{
			UObject* FoundObject = WeakObject.Get();

			if (!FoundObject)
			{
				continue;
			}
			
			UObject* PropertyOwner = FoundObject;
			if (!FbxSetting.PropertyPath.ComponentName.IsEmpty())
			{
				PropertyOwner = FindObjectFast<UObject>(FoundObject, *FbxSetting.PropertyPath.ComponentName);
			}

			if (!PropertyOwner)
			{
				continue;
			}
		
			FGuid PropertyOwnerGuid = InSequencer.GetHandleToObject(PropertyOwner);
			if (!PropertyOwnerGuid.IsValid())
			{
				continue;
			}

			UMovieSceneFloatTrack* FloatTrack = InMovieScene->FindTrack<UMovieSceneFloatTrack>(PropertyOwnerGuid, *FbxSetting.PropertyPath.PropertyName);
			if (!FloatTrack)
			{
				InMovieScene->Modify();
				FloatTrack = InMovieScene->AddTrack<UMovieSceneFloatTrack>(PropertyOwnerGuid);
				FloatTrack->SetPropertyNameAndPath(*FbxSetting.PropertyPath.PropertyName, *FbxSetting.PropertyPath.PropertyName);
			}

			if (FloatTrack)
			{
				FloatTrack->Modify();
				FloatTrack->RemoveAllAnimationData();

				FFrameRate FrameRate = FloatTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();

				bool bSectionAdded = false;
				UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(FloatTrack->FindOrAddSection(0, bSectionAdded));
				if (!FloatSection)
				{
					continue;
				}

				FloatSection->Modify();

				if (bSectionAdded)
				{
					FloatSection->SetRange(TRange<FFrameNumber>::All());
				}

				const int32 ChannelIndex = 0;
				const int32 CompositeIndex = 0;
				FRichCurve Source;
				const bool bNegative = false;
				CurveAPI.GetCurveData(NodeName, AnimatedPropertyName, ChannelIndex, CompositeIndex, Source, bNegative);

				FMovieSceneFloatChannel* Channel = FloatSection->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
				TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = Channel->GetData();

				ChannelData.Reset();
				double DecimalRate = FrameRate.AsDecimal();

				for (auto SourceIt = Source.GetKeyHandleIterator(); SourceIt; ++SourceIt)
				{
					FRichCurveKey &Key = Source.GetKey(*SourceIt);
					float ArriveTangent = Key.ArriveTangent;
					FKeyHandle PrevKeyHandle = Source.GetPreviousKey(*SourceIt);
					if (Source.IsKeyHandleValid(PrevKeyHandle))
					{
						FRichCurveKey &PrevKey = Source.GetKey(PrevKeyHandle);
						ArriveTangent = ArriveTangent / ((Key.Time - PrevKey.Time) * DecimalRate);

					}
					float LeaveTangent = Key.LeaveTangent;
					FKeyHandle NextKeyHandle = Source.GetNextKey(*SourceIt);
					if (Source.IsKeyHandleValid(NextKeyHandle))
					{
						FRichCurveKey &NextKey = Source.GetKey(NextKeyHandle);
						LeaveTangent = LeaveTangent / ((NextKey.Time - Key.Time) * DecimalRate);
					}

					FFrameNumber KeyTime = (Key.Time * FrameRate).RoundToFrame();
					FMatineeImportTools::SetOrAddKey(ChannelData, KeyTime, Key.Value, ArriveTangent, LeaveTangent,
						MovieSceneToolHelpers::RichCurveInterpolationToMatineeInterpolation(Key.InterpMode, Key.TangentMode), FrameRate, Key.TangentWeightMode,
						Key.ArriveTangentWeight, Key.LeaveTangentWeight);

				}

				Channel->AutoSetTangents();

				if (ImportFBXSettings->bReduceKeys)
				{
					FKeyDataOptimizationParams Params;
					Params.Tolerance = ImportFBXSettings->ReduceKeysTolerance;
					Params.DisplayRate = FrameRate;
					Params.bAutoSetInterpolation = true; //we use this to perform the AutoSetTangents after the keys are reduced.
					Channel->Optimize(Params);
				}

				return true;
			}
		}
	}
	return false;
}

void ImportTransformChannel(const FRichCurve& Source, FMovieSceneFloatChannel* Dest, FFrameRate DestFrameRate, bool bNegateTangents)
{
	// If there are no keys, don't clear the existing channel
	if (!Source.GetNumKeys())
	{
		return;
	}

	TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = Dest->GetData();
	ChannelData.Reset();
	for (auto SourceIt = Source.GetKeyHandleIterator(); SourceIt; ++SourceIt)
	{
		const FRichCurveKey Key = Source.GetKey(*SourceIt);
		float ArriveTangent = Key.ArriveTangent;
		FKeyHandle PrevKeyHandle = Source.GetPreviousKey(*SourceIt);
		if (Source.IsKeyHandleValid(PrevKeyHandle))
		{
			const FRichCurveKey PrevKey = Source.GetKey(PrevKeyHandle);
			ArriveTangent = ArriveTangent / (Key.Time - PrevKey.Time);

		}
		float LeaveTangent = Key.LeaveTangent;
		FKeyHandle NextKeyHandle = Source.GetNextKey(*SourceIt);
		if (Source.IsKeyHandleValid(NextKeyHandle))
		{
			const FRichCurveKey NextKey = Source.GetKey(NextKeyHandle);
			LeaveTangent = LeaveTangent / (NextKey.Time - Key.Time);
		}

		if (bNegateTangents)
		{
			ArriveTangent = -ArriveTangent;
			LeaveTangent = -LeaveTangent;
		}

		FFrameNumber KeyTime = (Key.Time * DestFrameRate).RoundToFrame();
		FMatineeImportTools::SetOrAddKey(ChannelData, KeyTime, Key.Value, ArriveTangent, LeaveTangent,
			MovieSceneToolHelpers::RichCurveInterpolationToMatineeInterpolation(Key.InterpMode, Key.TangentMode), DestFrameRate, Key.TangentWeightMode,
			Key.ArriveTangentWeight, Key.LeaveTangentWeight);

	}

	Dest->AutoSetTangents();

	const UMovieSceneUserImportFBXSettings* ImportFBXSettings = GetDefault<UMovieSceneUserImportFBXSettings>();
	if (ImportFBXSettings->bReduceKeys)
	{
		FKeyDataOptimizationParams Params;
		Params.Tolerance = ImportFBXSettings->ReduceKeysTolerance;
		Params.DisplayRate = DestFrameRate;
		Dest->Optimize(Params);
	}
}

bool ImportFBXTransform(FString NodeName, FGuid ObjectBinding, UnFbx::FFbxCurvesAPI& CurveAPI, UMovieScene* InMovieScene)
{
	const UMovieSceneUserImportFBXSettings* ImportFBXSettings = GetDefault<UMovieSceneUserImportFBXSettings>();

	// Look for transforms explicitly
	FRichCurve Translation[3];
	FRichCurve EulerRotation[3];
	FRichCurve Scale[3];
	FTransform DefaultTransform;
	CurveAPI.GetConvertedTransformCurveData(NodeName, Translation[0], Translation[1], Translation[2], EulerRotation[0], EulerRotation[1], EulerRotation[2], Scale[0], Scale[1], Scale[2], DefaultTransform);

 	UMovieScene3DTransformTrack* TransformTrack = InMovieScene->FindTrack<UMovieScene3DTransformTrack>(ObjectBinding); 
	if (!TransformTrack)
	{
		InMovieScene->Modify();
		TransformTrack = InMovieScene->AddTrack<UMovieScene3DTransformTrack>(ObjectBinding);
	}
	TransformTrack->Modify();

	bool bSectionAdded = false;
	UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->FindOrAddSection(0, bSectionAdded));
	if (!TransformSection)
	{
		return false;
	}

	TransformSection->Modify();

	FFrameRate FrameRate = TransformSection->GetTypedOuter<UMovieScene>()->GetTickResolution();

	if (bSectionAdded)
	{
		TransformSection->SetRange(TRange<FFrameNumber>::All());
	}

	FVector Location = DefaultTransform.GetLocation(), Rotation = DefaultTransform.GetRotation().Euler(), Scale3D = DefaultTransform.GetScale3D();

	TArrayView<FMovieSceneFloatChannel*> Channels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

	Channels[0]->SetDefault(Location.X);
	Channels[1]->SetDefault(Location.Y);
	Channels[2]->SetDefault(Location.Z);

	Channels[3]->SetDefault(Rotation.X);
	Channels[4]->SetDefault(Rotation.Y);
	Channels[5]->SetDefault(Rotation.Z);

	Channels[6]->SetDefault(Scale3D.X);
	Channels[7]->SetDefault(Scale3D.Y);
	Channels[8]->SetDefault(Scale3D.Z);

	ImportTransformChannel(Translation[0],   Channels[0], FrameRate, false);
	ImportTransformChannel(Translation[1],   Channels[1], FrameRate, true);
	ImportTransformChannel(Translation[2],   Channels[2], FrameRate, false);

	ImportTransformChannel(EulerRotation[0], Channels[3], FrameRate, false);
	ImportTransformChannel(EulerRotation[1], Channels[4], FrameRate, true);
	ImportTransformChannel(EulerRotation[2], Channels[5], FrameRate, true);

	ImportTransformChannel(Scale[0],         Channels[6], FrameRate, false);
	ImportTransformChannel(Scale[1],         Channels[7], FrameRate, false);
	ImportTransformChannel(Scale[2],         Channels[8], FrameRate, false);

	return true;
}

bool ImportFBXNode(FString NodeName, UnFbx::FFbxCurvesAPI& CurveAPI, UMovieScene* InMovieScene, ISequencer& InSequencer, FGuid ObjectBinding)
{
	// Look for animated float properties
	TArray<FString> AnimatedPropertyNames;
	CurveAPI.GetNodeAnimatedPropertyNameArray(NodeName, AnimatedPropertyNames);
		
	for (auto AnimatedPropertyName : AnimatedPropertyNames)
	{
		ImportFBXProperty(NodeName, AnimatedPropertyName, ObjectBinding, CurveAPI, InMovieScene, InSequencer);
	}
	
	ImportFBXTransform(NodeName, ObjectBinding, CurveAPI, InMovieScene);

	return true;
}

void GetCameras( FbxNode* Parent, TArray<FbxCamera*>& Cameras )
{
	FbxCamera* Camera = Parent->GetCamera();
	if( Camera )
	{
		Cameras.Add(Camera);
	}

	int32 NodeCount = Parent->GetChildCount();
	for ( int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex )
	{
		FbxNode* Child = Parent->GetChild( NodeIndex );
		GetCameras(Child, Cameras);
	}
}

FbxCamera* FindCamera( FbxNode* Parent )
{
	FbxCamera* Camera = Parent->GetCamera();
	if( !Camera )
	{
		int32 NodeCount = Parent->GetChildCount();
		for ( int32 NodeIndex = 0; NodeIndex < NodeCount && !Camera; ++NodeIndex )
		{
			FbxNode* Child = Parent->GetChild( NodeIndex );
			Camera = Child->GetCamera();
		}
	}

	return Camera;
}

FbxNode* RetrieveObjectFromName(const TCHAR* ObjectName, FbxNode* Root)
{
	if (!Root)
	{
		return nullptr;
	}
	
	for (int32 ChildIndex=0;ChildIndex<Root->GetChildCount();++ChildIndex)
	{
		FbxNode* Node = Root->GetChild(ChildIndex);
		if (Node)
		{
			FString NodeName = FString(Node->GetName());

			if ( !FCString::Strcmp(ObjectName,UTF8_TO_TCHAR(Node->GetName())))
			{
				return Node;
			}

			if (FbxNode* NextNode = RetrieveObjectFromName(ObjectName,Node))
			{
				return NextNode;
			}
		}
	}

	return nullptr;
}

void CopyCameraProperties(FbxCamera* CameraNode, AActor* InCameraActor)
{
	float FieldOfView;
	float FocalLength;

	if (CameraNode->GetApertureMode() == FbxCamera::eFocalLength)
	{
		FocalLength = CameraNode->FocalLength.Get();
		FieldOfView = CameraNode->ComputeFieldOfView(FocalLength);
	}
	else
	{
		FieldOfView = CameraNode->FieldOfView.Get();
		FocalLength = CameraNode->ComputeFocalLength(FieldOfView);
	}

	float ApertureWidth = CameraNode->GetApertureWidth();
	float ApertureHeight = CameraNode->GetApertureHeight();

	UCameraComponent* CameraComponent = nullptr;

	if (ACineCameraActor* CineCameraActor = Cast<ACineCameraActor>(InCameraActor))
	{
		CameraComponent = CineCameraActor->GetCineCameraComponent();

		UCineCameraComponent* CineCameraComponent = CineCameraActor->GetCineCameraComponent();
		CineCameraComponent->FilmbackSettings.SensorWidth = FUnitConversion::Convert(ApertureWidth, EUnit::Inches, EUnit::Millimeters);
		CineCameraComponent->FilmbackSettings.SensorHeight = FUnitConversion::Convert(ApertureHeight, EUnit::Inches, EUnit::Millimeters);
		CineCameraComponent->FocusSettings.ManualFocusDistance = CameraNode->FocusDistance;
		if (FocalLength < CineCameraComponent->LensSettings.MinFocalLength)
		{
			CineCameraComponent->LensSettings.MinFocalLength = FocalLength;
		}
		if (FocalLength > CineCameraComponent->LensSettings.MaxFocalLength)
		{
			CineCameraComponent->LensSettings.MaxFocalLength = FocalLength;
		}
		CineCameraComponent->CurrentFocalLength = FocalLength;
	}
	else if (ACameraActor* CameraActor = Cast<ACameraActor>(InCameraActor))
	{
		CameraComponent = CameraActor->GetCameraComponent();
	}

	if (!CameraComponent)
	{
		return;
	}

	CameraComponent->SetProjectionMode(CameraNode->ProjectionType.Get() == FbxCamera::ePerspective ? ECameraProjectionMode::Perspective : ECameraProjectionMode::Orthographic);
	CameraComponent->SetAspectRatio(CameraNode->AspectWidth.Get() / CameraNode->AspectHeight.Get());
	CameraComponent->SetOrthoNearClipPlane(CameraNode->NearPlane.Get());
	CameraComponent->SetOrthoFarClipPlane(CameraNode->FarPlane.Get());
	CameraComponent->SetOrthoWidth(CameraNode->OrthoZoom.Get());
	CameraComponent->SetFieldOfView(FieldOfView);
}

FString GetCameraName(FbxCamera* InCamera)
{
	FbxNode* CameraNode = InCamera->GetNode();
	if (CameraNode)
	{
		return CameraNode->GetName();
	}

	return InCamera->GetName();
}

void ImportFBXCamera(UnFbx::FFbxImporter* FbxImporter, UMovieScene* InMovieScene, ISequencer& InSequencer, TMap<FGuid, FString>& InObjectBindingMap, bool bMatchByNameOnly, bool bCreateCameras)
{
	if (bCreateCameras)
	{
		TArray<FbxCamera*> AllCameras;
		GetCameras(FbxImporter->Scene->GetRootNode(), AllCameras);

		UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;

		// Find unmatched cameras
		TArray<FbxCamera*> UnmatchedCameras;
		for (auto Camera : AllCameras)
		{
			FString NodeName = GetCameraName(Camera);

			bool bMatched = false;
			for (auto InObjectBinding : InObjectBindingMap)
			{		
				FString ObjectName = InObjectBinding.Value;
				if (ObjectName == NodeName)
				{
					// Look for a valid bound object, otherwise need to create a new camera and assign this binding to it
					bool bFoundBoundObject = false;
					TArrayView<TWeakObjectPtr<>> BoundObjects = InSequencer.FindBoundObjects(InObjectBinding.Key, InSequencer.GetFocusedTemplateID());
					for (auto BoundObject : BoundObjects)
					{
						if (BoundObject.IsValid())
						{
							bFoundBoundObject = true;
							break;
						}
					}

					if (!bFoundBoundObject)
					{
						FNotificationInfo Info(FText::Format(NSLOCTEXT("MovieSceneTools", "NoBoundObjectsError", "Existing binding has no objects. Creating a new camera and binding for {0}"), FText::FromString(ObjectName)));
						Info.ExpireDuration = 5.0f;
						FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
					}
				}
			}

			if (!bMatched)
			{
				UnmatchedCameras.Add(Camera);
			}
		}

		// If there are new cameras, clear the object binding map so that we're only assigning values to the newly created cameras
		if (UnmatchedCameras.Num() != 0)
		{
			InObjectBindingMap.Reset();
			bMatchByNameOnly = true;
		}

		// Add any unmatched cameras
		for (auto UnmatchedCamera : UnmatchedCameras)
		{
			FString CameraName = GetCameraName(UnmatchedCamera);

			AActor* NewCamera = nullptr;
			if (UnmatchedCamera->GetApertureMode() == FbxCamera::eFocalLength)
			{
				FActorSpawnParameters SpawnParams;
				NewCamera = World->SpawnActor<ACineCameraActor>(SpawnParams);
				NewCamera->SetActorLabel(*CameraName);
			}
			else
			{
				FActorSpawnParameters SpawnParams;
				NewCamera = World->SpawnActor<ACameraActor>(SpawnParams);
				NewCamera->SetActorLabel(*CameraName);
			}

			// Copy camera properties before adding default tracks so that initial camera properties match and can be restored after sequencer finishes
			CopyCameraProperties(UnmatchedCamera, NewCamera);

			TArray<TWeakObjectPtr<AActor> > NewCameras;
			NewCameras.Add(NewCamera);
			TArray<FGuid> NewCameraGuids = InSequencer.AddActors(NewCameras);

			if (NewCameraGuids.Num())
			{
				InObjectBindingMap.Add(NewCameraGuids[0]);
				InObjectBindingMap[NewCameraGuids[0]] = CameraName;
			}
		}
	}


	for (auto InObjectBinding : InObjectBindingMap)
	{
		TArrayView<TWeakObjectPtr<>> BoundObjects = InSequencer.FindBoundObjects(InObjectBinding.Key, InSequencer.GetFocusedTemplateID());
		
		FString ObjectName = InObjectBinding.Value;
		FbxCamera* CameraNode = nullptr;
		FbxNode* Node = RetrieveObjectFromName(*ObjectName, FbxImporter->Scene->GetRootNode());
		if (Node)
		{
			CameraNode = FindCamera(Node);
		}

		if (!CameraNode)
		{
			if (bMatchByNameOnly)
			{
				FNotificationInfo Info(FText::Format(NSLOCTEXT("MovieSceneTools", "NoMatchingCameraError", "Failed to find any matching camera for {0}"), FText::FromString(ObjectName)));
				Info.ExpireDuration = 5.0f;
				FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);

				continue;
			}

			CameraNode = FindCamera(FbxImporter->Scene->GetRootNode());
			if (CameraNode)
			{
				FString CameraName = GetCameraName(CameraNode);
				FNotificationInfo Info(FText::Format(NSLOCTEXT("MovieSceneTools", "NoMatchingCameraWarning", "Failed to find any matching camera for {0}. Importing onto first camera from fbx {1}"), FText::FromString(ObjectName), FText::FromString(CameraName)));
				Info.ExpireDuration = 5.0f;
				FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}

		if (!CameraNode)
		{
			continue;
		}

		float FieldOfView;
		float FocalLength;

		if (CameraNode->GetApertureMode() == FbxCamera::eFocalLength)
		{
			FocalLength = CameraNode->FocalLength.Get();
			FieldOfView = CameraNode->ComputeFieldOfView(FocalLength);
		}
		else
		{
			FieldOfView = CameraNode->FieldOfView.Get();
			FocalLength = CameraNode->ComputeFocalLength(FieldOfView);
		}

		for (TWeakObjectPtr<>& WeakObject : BoundObjects)
		{
			UObject* FoundObject = WeakObject.Get();
			if (FoundObject && FoundObject->GetClass()->IsChildOf(ACameraActor::StaticClass()))
			{
				CopyCameraProperties(CameraNode, Cast<AActor>(FoundObject));

				UCameraComponent* CameraComponent = nullptr;
				FName TrackName;
				float TrackValue;
				
				if (ACineCameraActor* CineCameraActor = Cast<ACineCameraActor>(FoundObject))
				{
					CameraComponent = CineCameraActor->GetCineCameraComponent();
					TrackName = TEXT("CurrentFocalLength");
					TrackValue = FocalLength;
				}
				else if (ACameraActor* CameraActor = Cast<ACameraActor>(FoundObject))
				{
					CameraComponent = CameraActor->GetCameraComponent();
					TrackName = TEXT("FieldOfView");
					TrackValue = FieldOfView;
				}
				else
				{
					continue;
				}

				// Set the default value of the current focal length or field of view section
				FGuid PropertyOwnerGuid = InSequencer.GetHandleToObject(CameraComponent);
				if (!PropertyOwnerGuid.IsValid())
				{
					continue;
				}

				UMovieSceneFloatTrack* FloatTrack = InMovieScene->FindTrack<UMovieSceneFloatTrack>(PropertyOwnerGuid, TrackName);
				if (FloatTrack)
				{
					FloatTrack->Modify();
					FloatTrack->RemoveAllAnimationData();

					bool bSectionAdded = false;
					UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(FloatTrack->FindOrAddSection(0, bSectionAdded));
					if (!FloatSection)
					{
						continue;
					}

					FloatSection->Modify();

					if (bSectionAdded)
					{
						FloatSection->SetRange(TRange<FFrameNumber>::All());
					}

					FloatSection->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0)->SetDefault(TrackValue);
				}
			}
		}
	}
}

FGuid FindCameraGuid(FbxCamera* Camera, TMap<FGuid, FString>& InObjectBindingMap)
{
	FString CameraName = GetCameraName(Camera);

	for (auto& Pair : InObjectBindingMap)
	{
		if (Pair.Value == CameraName)
		{
			return Pair.Key;
		}
	}
	return FGuid();
}

UMovieSceneCameraCutTrack* GetCameraCutTrack(UMovieScene* InMovieScene)
{
	// Get the camera cut
	UMovieSceneTrack* CameraCutTrack = InMovieScene->GetCameraCutTrack();
	if (CameraCutTrack == nullptr)
	{
		InMovieScene->Modify();
		CameraCutTrack = InMovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
	}
	return CastChecked<UMovieSceneCameraCutTrack>(CameraCutTrack);
}

void ImportCameraCut(UnFbx::FFbxImporter* FbxImporter, UMovieScene* InMovieScene, ISequencer& InSequencer, TMap<FGuid, FString>& InObjectBindingMap)
{
	// Find a camera switcher
	FbxCameraSwitcher* CameraSwitcher = FbxImporter->Scene->GlobalCameraSettings().GetCameraSwitcher();
	if (CameraSwitcher == nullptr)
	{
		return;
	}
	// Get the animation layer
	FbxAnimStack* AnimStack = FbxImporter->Scene->GetMember<FbxAnimStack>(0);
	if (AnimStack == nullptr)
	{
		return;
	}
	FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
	if (AnimLayer == nullptr)
	{
		return;
	}

	// The camera switcher camera index refer to depth-first found order of the camera in the FBX
	TArray<FbxCamera*> AllCameras;
	GetCameras(FbxImporter->Scene->GetRootNode(), AllCameras);

	UMovieSceneCameraCutTrack* CameraCutTrack = GetCameraCutTrack(InMovieScene);
	FFrameRate FrameRate = CameraCutTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();

	FbxAnimCurve* AnimCurve = CameraSwitcher->CameraIndex.GetCurve(AnimLayer);
	if (AnimCurve)
	{
		for (int i = 0; i < AnimCurve->KeyGetCount(); ++i)
		{
			FbxAnimCurveKey key = AnimCurve->KeyGet(i);
			int value = (int)key.GetValue() - 1;
			if (value >= 0 && value < AllCameras.Num())
			{
				FGuid CameraGuid = FindCameraGuid(AllCameras[value], InObjectBindingMap);
				if (CameraGuid != FGuid())
				{
					CameraCutTrack->AddNewCameraCut(FMovieSceneObjectBindingID(CameraGuid, MovieSceneSequenceID::Root), (key.GetTime().GetSecondDouble() * FrameRate).RoundToFrame());
				}
			}
		}
		InSequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

class SMovieSceneImportFBXSettings : public SCompoundWidget, public FGCObject
{
	SLATE_BEGIN_ARGS(SMovieSceneImportFBXSettings) {}
		SLATE_ARGUMENT(FString, ImportFilename)
		SLATE_ARGUMENT(UMovieScene*, MovieScene)
		SLATE_ARGUMENT(ISequencer*, Sequencer)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bShowPropertyMatrixButton = false;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.ViewIdentifier = "Import FBX Settings";

		DetailView = PropertyEditor.CreateDetailView(DetailsViewArgs);

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			[
				DetailView.ToSharedRef()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(5.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(10, 5))
				.Text(NSLOCTEXT("MovieSceneTools", "ImportFBXButtonText", "Import"))
				.OnClicked(this, &SMovieSceneImportFBXSettings::OnImportFBXClicked)
			]
			
		];

		ImportFilename = InArgs._ImportFilename;
		MovieScene = InArgs._MovieScene;
		Sequencer = InArgs._Sequencer;

		UMovieSceneUserImportFBXSettings* ImportFBXSettings = GetMutableDefault<UMovieSceneUserImportFBXSettings>();
		DetailView->SetObject(ImportFBXSettings);
	}

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject(MovieScene);
	}

	void SetObjectBindingMap(const TMap<FGuid, FString>& InObjectBindingMap)
	{
		ObjectBindingMap = InObjectBindingMap;
	}

	void SetCreateCameras(TOptional<bool> bInCreateCameras)
	{
		bCreateCameras = bInCreateCameras;
	}

private:

	FReply OnImportFBXClicked()
	{
		UMovieSceneUserImportFBXSettings* ImportFBXSettings = GetMutableDefault<UMovieSceneUserImportFBXSettings>();

		FEditorDirectories::Get().SetLastDirectory( ELastDirectory::FBX, FPaths::GetPath( ImportFilename ) ); // Save path as default for next time.
	
		UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();

		UnFbx::FBXImportOptions* ImportOptions = FbxImporter->GetImportOptions();
		bool bConvertSceneBackup = ImportOptions->bConvertScene;
		bool bConvertSceneUnitBackup = ImportOptions->bConvertSceneUnit;
		bool bForceFrontXAxisBackup = ImportOptions->bForceFrontXAxis;

		ImportOptions->bConvertScene = true;
		ImportOptions->bConvertSceneUnit = true;
		ImportOptions->bForceFrontXAxis = ImportFBXSettings->bForceFrontXAxis;

		const FString FileExtension = FPaths::GetExtension(ImportFilename);
		if (!FbxImporter->ImportFromFile(*ImportFilename, FileExtension, true))
		{
			// Log the error message and fail the import.
			FbxImporter->ReleaseScene();
			ImportOptions->bConvertScene = bConvertSceneBackup;
			ImportOptions->bConvertSceneUnit = bConvertSceneUnitBackup;
			ImportOptions->bForceFrontXAxis = bForceFrontXAxisBackup;
			return FReply::Unhandled();
		}
		
		if (MovieScene->IsReadOnly())
		{
			return FReply::Unhandled();
		}
		
		const bool bMatchByNameOnly = ImportFBXSettings->bMatchByNameOnly;

		const FScopedTransaction Transaction( NSLOCTEXT( "MovieSceneTools", "ImportFBXTransaction", "Import FBX" ) );

		// Import static cameras first
		ImportFBXCamera(FbxImporter, MovieScene, *Sequencer, ObjectBindingMap, bMatchByNameOnly, bCreateCameras.IsSet() ? bCreateCameras.GetValue() : ImportFBXSettings->bCreateCameras);

		UnFbx::FFbxCurvesAPI CurveAPI;
		FbxImporter->PopulateAnimatedCurveData(CurveAPI);
		TArray<FString> AllNodeNames;
		CurveAPI.GetAllNodeNameArray(AllNodeNames);

		FString RootNodeName = FbxImporter->Scene->GetRootNode()->GetName();

		// First try matching by name
		for (int32 NodeIndex = 0; NodeIndex < AllNodeNames.Num(); )
		{
			FString NodeName = AllNodeNames[NodeIndex];
			if (RootNodeName == NodeName)
			{
				++NodeIndex;
				continue;
			}

			bool bFoundMatch = false;
			for (auto It = ObjectBindingMap.CreateConstIterator(); It; ++It)
			{
				if (FCString::Strcmp(*It.Value().ToUpper(), *NodeName.ToUpper()) == 0)
				{
					ImportFBXNode(NodeName, CurveAPI, MovieScene, *Sequencer, It.Key());

					ObjectBindingMap.Remove(It.Key());
					AllNodeNames.RemoveAt(NodeIndex);

					bFoundMatch = true;
					break;
				}
			}

			if (bFoundMatch)
			{
				continue;
			}

			++NodeIndex;
		}

		// Otherwise, get the first available node that hasn't been imported onto yet
		if (!bMatchByNameOnly)
		{
			for (int32 NodeIndex = 0; NodeIndex < AllNodeNames.Num(); )
			{
				FString NodeName = AllNodeNames[NodeIndex];
				if (RootNodeName == NodeName)
				{
					++NodeIndex;
					continue;
				}

				auto It = ObjectBindingMap.CreateConstIterator();
				if (It)
				{
					ImportFBXNode(NodeName, CurveAPI, MovieScene, *Sequencer, It.Key());

					UE_LOG(LogMovieScene, Warning, TEXT("Fbx Import: Failed to find any matching node for (%s). Defaulting to first available (%s)."), *NodeName, *It.Value());
					ObjectBindingMap.Remove(It.Key());
					AllNodeNames.RemoveAt(NodeIndex);
					continue;
				}

				++NodeIndex;
			}
		}

		for (FString NodeName : AllNodeNames)
		{
			UE_LOG(LogMovieScene, Warning, TEXT("Fbx Import: Failed to find any matching node for (%s)."), *NodeName);
		}

		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);

		FbxImporter->ReleaseScene();
		ImportOptions->bConvertScene = bConvertSceneBackup;
		ImportOptions->bConvertSceneUnit = bConvertSceneUnitBackup;
		ImportOptions->bForceFrontXAxis = bForceFrontXAxisBackup;

		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());

		if ( Window.IsValid() )
		{
			Window->RequestDestroyWindow();
		}

		return FReply::Handled();
	}

	TSharedPtr<IDetailsView> DetailView;
	FString ImportFilename;
	UMovieScene* MovieScene;
	ISequencer* Sequencer;
	TMap<FGuid, FString> ObjectBindingMap;
	TOptional<bool> bCreateCameras;
};


bool MovieSceneToolHelpers::ImportFBX(UMovieScene* InMovieScene, ISequencer& InSequencer, const TMap<FGuid, FString>& InObjectBindingMap, TOptional<bool> bCreateCameras)
{
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		FString ExtensionStr;
		ExtensionStr += TEXT("FBX (*.fbx)|*.fbx|");

		bOpen = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("MovieSceneTools", "ImportFBX", "Import FBX from...").ToString(), 
			FEditorDirectories::Get().GetLastDirectory(ELastDirectory::FBX),
			TEXT(""), 
			*ExtensionStr,
			EFileDialogFlags::None,
			OpenFilenames
			);
	}
	if (!bOpen)
	{
		return false;
	}

	if (!OpenFilenames.Num())
	{
		return false;
	}

	const FText TitleText = NSLOCTEXT("MovieSceneTools", "ImportFBXTitle", "Import FBX");

	// Create the window to choose our options
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(TitleText)
		.HasCloseButton(true)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(400.0f, 200.0f))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false);

	TSharedRef<SMovieSceneImportFBXSettings> DialogWidget = SNew(SMovieSceneImportFBXSettings)
		.ImportFilename(OpenFilenames[0])
		.MovieScene(InMovieScene)
		.Sequencer(&InSequencer);
	DialogWidget->SetObjectBindingMap(InObjectBindingMap);
	DialogWidget->SetCreateCameras(bCreateCameras);
	Window->SetContent(DialogWidget);

	FSlateApplication::Get().AddWindow(Window);

	return true;
}


EInterpCurveMode MovieSceneToolHelpers::RichCurveInterpolationToMatineeInterpolation( ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode)
{
	switch ( InterpMode )
	{
	case ERichCurveInterpMode::RCIM_Constant:
		return CIM_Constant;
	case ERichCurveInterpMode::RCIM_Cubic:
		if (TangentMode == RCTM_Auto)
		{
			return CIM_CurveAuto;
		}
		else if (TangentMode == RCTM_Break)
		{
			return CIM_CurveBreak;
		}
		return CIM_CurveUser;  
	case ERichCurveInterpMode::RCIM_Linear:
		return CIM_Linear;
	default:
		return CIM_CurveAuto;
	}
}

void MovieSceneToolHelpers::CopyKeyDataToMoveAxis(const TMovieSceneChannelData<FMovieSceneFloatValue>& Channel, UInterpTrackMoveAxis* MoveAxis, FFrameRate InFrameRate)
{
	MoveAxis->FloatTrack.Points.Reset();

	static FName LookupName(NAME_None);
	
	TArrayView<const FFrameNumber>          Times  = Channel.GetTimes();
	TArrayView<const FMovieSceneFloatValue> Values = Channel.GetValues();

	for (int32 KeyIndex = 0; KeyIndex < Times.Num(); ++KeyIndex)
	{
		const float Time = Times[KeyIndex] / InFrameRate;
		const FMovieSceneFloatValue& Value = Values[KeyIndex];

		const int32 PointIndex = MoveAxis->FloatTrack.AddPoint(Time, Value.Value);
		MoveAxis->LookupTrack.AddPoint(Time, LookupName);

		FInterpCurvePoint<float>& Point = MoveAxis->FloatTrack.Points[PointIndex];
		Point.ArriveTangent = Value.Tangent.ArriveTangent;
		Point.LeaveTangent = Value.Tangent.LeaveTangent;
		Point.InterpMode = RichCurveInterpolationToMatineeInterpolation(Value.InterpMode, Value.TangentMode);
	}
}

UObject* MovieSceneToolHelpers::ExportToCameraAnim(UMovieScene* InMovieScene, FGuid& InObjectBinding)
{
	// Create a new camera anim
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UObject* NewAsset = nullptr;

	// Attempt to create a new asset
	for (TObjectIterator<UClass> It ; It ; ++It)
	{
		UClass* CurrentClass = *It;
		if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
			if (Factory->CanCreateNew() && Factory->ImportPriority >= 0 && Factory->SupportedClass == UCameraAnim::StaticClass())
			{
				NewAsset = AssetTools.CreateAssetWithDialog(UCameraAnim::StaticClass(), Factory);
				break;
			}
		}
	}

	if (!NewAsset)
	{
		return NewAsset;
	}

	static FName Transform("Transform");
	UMovieScene3DTransformTrack* TransformTrack = InMovieScene->FindTrack<UMovieScene3DTransformTrack>(InObjectBinding, Transform); 
	if (TransformTrack)
	{
		UCameraAnim* CameraAnim = CastChecked<UCameraAnim>(NewAsset);
		UInterpGroup* CameraInterpGroup = CameraAnim->CameraInterpGroup;
		CameraAnim->bRelativeToInitialTransform=false;

		UInterpGroupInst* CameraInst = NewObject<UInterpGroupInst>(CameraAnim, NAME_None, RF_Transactional);
		CameraInst->InitGroupInst(CameraInterpGroup, nullptr);

		UInterpTrackMove* MovementTrack = NewObject<UInterpTrackMove>(CameraInterpGroup, NAME_None, RF_Transactional);
		CameraInterpGroup->InterpTracks.Add(MovementTrack);
		
		UInterpTrackInstMove* MovementTrackInst = NewObject<UInterpTrackInstMove>(CameraInst, NAME_None, RF_Transactional);
		CameraInst->TrackInst.Add(MovementTrackInst);
		MovementTrackInst->InitTrackInst(MovementTrack);
			
		MovementTrack->CreateSubTracks(false);

		UInterpTrackMoveAxis* MoveAxies[6];
		for( int32 SubTrackIndex = 0; SubTrackIndex < 6; ++SubTrackIndex )
		{
			MoveAxies[ SubTrackIndex ] = Cast<UInterpTrackMoveAxis>( MovementTrack->SubTracks[ SubTrackIndex ] );
		}

		TArray<UMovieSceneSection*> Sections = TransformTrack->GetAllSections();

		if (Sections.Num())
		{
			if (Sections.Num() > 1)
			{
				UE_LOG(LogMovieScene, Error, TEXT("Export to Camera Anim: Failed to export, multiple sections (%d) are not supported"), Sections.Num());
			}
			else
			{
				FFrameRate TickResolution = InMovieScene->GetTickResolution();
				UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Sections[0]);
				TArrayView<FMovieSceneFloatChannel*> FloatChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

				CopyKeyDataToMoveAxis(FloatChannels[0]->GetData(), MoveAxies[AXIS_TranslationX], TickResolution);
				CopyKeyDataToMoveAxis(FloatChannels[1]->GetData(), MoveAxies[AXIS_TranslationY], TickResolution);
				CopyKeyDataToMoveAxis(FloatChannels[2]->GetData(), MoveAxies[AXIS_TranslationZ], TickResolution);
				CopyKeyDataToMoveAxis(FloatChannels[3]->GetData(), MoveAxies[AXIS_RotationX],    TickResolution);
				CopyKeyDataToMoveAxis(FloatChannels[4]->GetData(), MoveAxies[AXIS_RotationY],    TickResolution);
				CopyKeyDataToMoveAxis(FloatChannels[5]->GetData(), MoveAxies[AXIS_RotationZ],    TickResolution);
			}
		}
	}

	return NewAsset;
}


bool MovieSceneToolHelpers::HasHiddenMobility(const UClass* ObjectClass)
{
	if (ObjectClass)
	{
		static const FName NAME_HideCategories(TEXT("HideCategories"));
		if (ObjectClass->HasMetaData(NAME_HideCategories))
		{
			if (ObjectClass->GetMetaData(NAME_HideCategories).Contains(TEXT("Mobility")))
			{
				return true;
			}
		}
	}

	return false;
}

FMovieSceneEvaluationTrack* MovieSceneToolHelpers::GetEvaluationTrack(ISequencer *Sequencer, const FGuid& TrackSignature)
{
	FMovieSceneEvaluationTemplate* Template = Sequencer->GetEvaluationTemplate().FindTemplate(Sequencer->GetFocusedTemplateID());
	if (Template)
	{
		if (FMovieSceneEvaluationTrack* EvalTrack = Template->FindTrack(TrackSignature))
		{
			return EvalTrack;
		}
	}
	return nullptr;
}
