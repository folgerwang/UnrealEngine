// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "ITakeRecorderModule.h"
#include "ITakeRecorderDropHandler.h"
#include "TakeRecorderLevelSequenceSource.h"
#include "Input/DragAndDrop.h"
#include "Recorder/TakeRecorder.h"
#include "Recorder/TakeRecorderParameters.h"
#include "TakesCoreBlueprintLibrary.h"
#include "TakesCoreFwd.h"
#include "TakeMetaData.h"
#include "TakeRecorderActorSource.h"
#include "TakeRecorderSources.h"
#include "TakeRecorderSettings.h"
#include "Features/IModularFeatures.h"
#include "DragAndDrop/ActorDragDropOp.h"
#include "SceneOutlinerDragDrop.h"
#include "EngineUtils.h"
#include "Algo/Sort.h"
#include "ScopedTransaction.h"
#include "LevelSequenceActor.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Widgets/Layout/SBox.h"

#include "Engine/LevelScriptActor.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"

#include "TakeRecorderMicrophoneAudioSource.h"
#include "TakeRecorderWorldSource.h"
#include "TrackRecorders/MovieSceneAnimationTrackRecorderSettings.h"

#define LOCTEXT_NAMESPACE "TakeRecorderSources"

static void AddActorSources(UTakeRecorderSources* Sources, TArrayView<AActor* const> InActors)
{
	if (InActors.Num() > 0)
	{
		FScopedTransaction Transaction(FText::Format(NSLOCTEXT("TakeRecorderSources", "AddSources", "Add Recording {0}|plural(one=Source, other=Sources)"), InActors.Num()));
		Sources->Modify();

		for (AActor* Actor : InActors)
		{
			if (Actor->IsA<ALevelSequenceActor>())
			{
				ALevelSequenceActor* LevelSequenceActor = Cast<ALevelSequenceActor>(Actor);

				UTakeRecorderLevelSequenceSource* LevelSequenceSource = nullptr;

				for (UTakeRecorderSource* Source : Sources->GetSources())
				{
					if (Source->IsA<UTakeRecorderLevelSequenceSource>())
					{
						LevelSequenceSource = Cast<UTakeRecorderLevelSequenceSource>(Source);
						break;
					}
				}

				if (!LevelSequenceSource)
				{
					LevelSequenceSource = Sources->AddSource<UTakeRecorderLevelSequenceSource>();
				}

				ULevelSequence* Sequence = LevelSequenceActor->GetSequence();
				if (Sequence)
				{
					if (!LevelSequenceSource->LevelSequencesToTrigger.Contains(Sequence))
					{
						LevelSequenceSource->LevelSequencesToTrigger.Add(Sequence);
					}
				}
			}
			else
			{
				UTakeRecorderActorSource* NewSource = Sources->AddSource<UTakeRecorderActorSource>();

				if (AActor* EditorActor = EditorUtilities::GetEditorWorldCounterpartActor(Actor))
				{
					NewSource->Target = EditorActor;
				}
				else
				{
					NewSource->Target = Actor;
				}

				// Send a PropertyChangedEvent so the class catches the callback and rebuilds the property map.
				FPropertyChangedEvent PropertyChangedEvent(UTakeRecorderActorSource::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UTakeRecorderActorSource, Target)), EPropertyChangeType::ValueSet);
				NewSource->PostEditChangeProperty(PropertyChangedEvent);
			}
		}
	}
}


namespace
{
	static AActor* FindActorByLabel(const FString& ActorNameStr, UWorld* InWorld, bool bFuzzy = false)
	{
		// search for the actor by name
		for (ULevel* Level : InWorld->GetLevels())
		{
			if (Level)
			{
				for (AActor* Actor : Level->Actors)
				{
					if (Actor)
					{
						if (Actor->GetActorLabel() == ActorNameStr)
						{
							return Actor;
						}
					}
				}
			}
		}

		// if we want to do a fuzzy search then we return the first actor whose name that starts 
		// the specified string
		if (bFuzzy)
		{
			for (ULevel* Level : InWorld->GetLevels())
			{
				if (Level)
				{
					for (AActor* Actor : Level->Actors)
					{
						if (Actor)
						{
							if (Actor->GetActorLabel().StartsWith(ActorNameStr))
							{
								return Actor;
							}
						}
					}
				}
			}
		}

		return nullptr;
	}

	static void FindActorsOfClass(UClass* Class, UWorld* InWorld, TArray<AActor*>& OutActors)
	{
		for (ULevel* Level : InWorld->GetLevels())
		{
			if (Level)
			{
				for (AActor* Actor : Level->Actors)
				{
					if (Actor && Actor->IsA(Class) && !Actor->IsA(ALevelScriptActor::StaticClass()) && !Actor->IsA(ALevelSequenceActor::StaticClass()))
					{
						OutActors.AddUnique(Actor);
					}
				}
			}
		}
	}
}

struct FActorTakeRecorderDropHandler : ITakeRecorderDropHandler
{
	virtual void HandleOperation(TSharedPtr<FDragDropOperation> InOperation, UTakeRecorderSources* Sources) override
	{
		TArray<AActor*> ActorsToAdd = GetValidDropActors(InOperation, Sources);
		AddActorSources(Sources, ActorsToAdd);
	}

	virtual bool CanHandleOperation(TSharedPtr<FDragDropOperation> InOperation, UTakeRecorderSources* Sources) override
	{
		using namespace SceneOutliner;

		if (InOperation)
		{
			if (InOperation->IsOfType<FActorDragDropOp>())
			{
				return StaticCastSharedPtr<FActorDragDropOp>(InOperation)->Actors.Num() > 0;
			}
			else if (InOperation->IsOfType<FSceneOutlinerDragDropOp>())
			{
				return true;
			}
		}

		return false;
	}

	TArray<AActor*> GetValidDropActors(TSharedPtr<FDragDropOperation> InOperation, UTakeRecorderSources* Sources)
	{
		using namespace SceneOutliner;

		FActorDragDropOp*  ActorDrag = nullptr;
		FFolderDragDropOp* FolderDrag = nullptr;

		FDragDropOperation* OperationPtr = InOperation.Get();
		if (!OperationPtr)
		{
			return TArray<AActor*>();
		}
		else if (OperationPtr->IsOfType<FSceneOutlinerDragDropOp>())
		{
			FSceneOutlinerDragDropOp* OutlinerOp = static_cast<FSceneOutlinerDragDropOp*>(OperationPtr);
			FolderDrag = OutlinerOp->FolderOp.Get();
			ActorDrag  = OutlinerOp->ActorOp.Get();

		}
		else if (OperationPtr->IsOfType<FActorDragDropOp>())
		{
			ActorDrag = static_cast<FActorDragDropOp*>(OperationPtr);
		}
		else if (OperationPtr->IsOfType<FFolderDragDropOp>())
		{
			FolderDrag = static_cast<FFolderDragDropOp*>(OperationPtr);
		}

		TArray<AActor*> DraggedActors;

		if (ActorDrag)
		{
			DraggedActors.Reserve(ActorDrag->Actors.Num());
			for (TWeakObjectPtr<AActor> WeakActor : ActorDrag->Actors)
			{
				if (AActor* Actor = WeakActor.Get())
				{
					DraggedActors.Add(Actor);
				}
			}
		}

		if (FolderDrag)
		{
			// Copy the array onto the stack if it's within a reasonable size
			TArray<FName, TInlineAllocator<16>> DraggedFolders(FolderDrag->Folders);

			// Find any actors in the global editor world that have any of the dragged paths.
			// WARNING: Actor iteration can be very slow, so this needs to be optimized
			for (FActorIterator ActorIt(GWorld); ActorIt; ++ActorIt)
			{
				FName ActorPath = ActorIt->GetFolderPath();
				if (ActorPath.IsNone() || !DraggedFolders.Contains(ActorPath))
				{
					continue;
				}

				DraggedActors.Add(*ActorIt);
			}
		}

		TArray<AActor*> ExistingActors;
		for (UTakeRecorderSource* Source : Sources->GetSources())
		{
			UTakeRecorderActorSource* ActorSource = Cast<UTakeRecorderActorSource>(Source);
			AActor* ExistingActor = ActorSource ? ActorSource->Target.Get() : nullptr;
			if (ExistingActor)
			{
				ExistingActors.Add(ExistingActor);
			}
		}

		if (ExistingActors.Num() && DraggedActors.Num())
		{
			// Remove any actors that are already added as a source. We do this by sorting both arrays,
			// then iterating them together, removing any that are the same
			Algo::Sort(ExistingActors);
			Algo::Sort(DraggedActors);

			for (int32 DragIndex = 0, PredIndex = 0;
				DragIndex < DraggedActors.Num() && PredIndex < ExistingActors.Num();
				/** noop*/)
			{
				AActor* Dragged   = DraggedActors[DragIndex];
				AActor* Predicate = ExistingActors[PredIndex];

				if (Dragged < Predicate)
				{
					++DragIndex;
				}
				else if (Dragged == Predicate)
				{
					DraggedActors.RemoveAt(DragIndex, 1, false);
				}
				else // (Dragged > Predicate)
				{
					++PredIndex;
				}
			}
		}

		return DraggedActors;
	}
};

class FTakeRecorderSourcesModule : public IModuleInterface, private FSelfRegisteringExec
{
public:

	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(ITakeRecorderDropHandler::ModularFeatureName, &ActorDropHandler);

		ITakeRecorderModule& TakeRecorderModule = FModuleManager::Get().LoadModuleChecked<ITakeRecorderModule>("TakeRecorder");

		SourcesMenuExtension = TakeRecorderModule.RegisterSourcesMenuExtension(FOnExtendSourcesMenu::CreateStatic(ExtendSourcesMenu));

		TakeRecorderModule.RegisterSettingsObject(GetMutableDefault<UTakeRecorderMicrophoneAudioSourceSettings>());
		TakeRecorderModule.RegisterSettingsObject(GetMutableDefault<UMovieSceneAnimationTrackRecorderEditorSettings>());
		TakeRecorderModule.RegisterSettingsObject(GetMutableDefault<UTakeRecorderWorldSourceSettings>());
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(ITakeRecorderDropHandler::ModularFeatureName, &ActorDropHandler);

		ITakeRecorderModule* TakeRecorderModule = FModuleManager::Get().GetModulePtr<ITakeRecorderModule>("TakeRecorder");
		if (TakeRecorderModule)
		{
			TakeRecorderModule->UnregisterSourcesMenuExtension(SourcesMenuExtension);
		}
	}

	static void ExtendSourcesMenu(TSharedRef<FExtender> Extender, UTakeRecorderSources* Sources)
	{
		Extender->AddMenuExtension("Sources", EExtensionHook::Before, nullptr, FMenuExtensionDelegate::CreateStatic(PopulateSourcesMenu, Sources));
	}

	static void PopulateSourcesMenu(FMenuBuilder& MenuBuilder, UTakeRecorderSources* Sources)
	{
		FName ExtensionName = "ActorSourceSubMenu";
		MenuBuilder.AddSubMenu(
			NSLOCTEXT("TakeRecorderSources", "ActorList_Label", "From Actor"),
			NSLOCTEXT("TakeRecorderSources", "ActorList_Tip", "Add a new recording source from an actor in the current world"),
			FNewMenuDelegate::CreateStatic(PopulateActorSubMenu, Sources),
			FUIAction(),
			ExtensionName,
			EUserInterfaceActionType::Button
		);
	}

	static void PopulateActorSubMenu(FMenuBuilder& MenuBuilder, UTakeRecorderSources* Sources)
	{
		TSet<const AActor*> ExistingActors;

		for (UTakeRecorderSource* Source : Sources->GetSources())
		{
			UTakeRecorderActorSource* ActorSource   = Cast<UTakeRecorderActorSource>(Source);
			AActor*                   ExistingActor = ActorSource ? ActorSource->Target.Get() : nullptr;

			if (ExistingActor)
			{
				ExistingActors.Add(ExistingActor);
			}
		}

		auto OutlinerFilterPredicate = [InExistingActors = MoveTemp(ExistingActors)](const AActor* InActor)
		{
			return !InExistingActors.Contains(InActor);
		};

		// Set up a menu entry to add the selected actor(s) to the sequencer
		TArray<AActor*> SelectedActors;
		GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);
		SelectedActors.RemoveAll([&](const AActor* In){ return !OutlinerFilterPredicate(In); });

		FText SelectedLabel;
		FSlateIcon ActorIcon = FSlateIconFinder::FindIconForClass(AActor::StaticClass());
		if (SelectedActors.Num() == 1)
		{
			SelectedLabel = FText::Format(LOCTEXT("AddSpecificActor", "Add '{0}'"), FText::FromString(SelectedActors[0]->GetActorLabel()));
			ActorIcon = FSlateIconFinder::FindIconForClass(SelectedActors[0]->GetClass());
		}
		else if (SelectedActors.Num() > 1)
		{
			SelectedLabel = FText::Format(LOCTEXT("AddCurrentActorSelection", "Add Current Selection ({0} actors)"), FText::AsNumber(SelectedActors.Num()));
		}

		if (!SelectedLabel.IsEmpty())
		{
			MenuBuilder.AddMenuEntry(
				SelectedLabel,
				FText(),
				ActorIcon,
				FExecuteAction::CreateLambda([Sources, SelectedActors]{
					AddActorSources(Sources, SelectedActors);
				})
			);
		}

		MenuBuilder.BeginSection("ChooseActorSection", LOCTEXT("ChooseActor", "Choose Actor:"));
		{
			using namespace SceneOutliner;

			// Set up a menu entry to add any arbitrary actor to the sequencer
			FInitializationOptions InitOptions;
			{
				InitOptions.Mode = ESceneOutlinerMode::ActorPicker;

				// We hide the header row to keep the UI compact.
				InitOptions.bShowHeaderRow = false;
				InitOptions.bShowSearchBox = true;
				InitOptions.bShowCreateNewFolder = false;
				InitOptions.bFocusSearchBoxWhenOpened = true;

				// Only want the actor label column
				InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));

				// Only display actors that are not possessed already
				InitOptions.Filters->AddFilterPredicate(FActorFilterPredicate::CreateLambda(OutlinerFilterPredicate));
			}

			// actor selector to allow the user to choose an actor
			FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
			TSharedRef< SWidget > MiniSceneOutliner =
				SNew(SBox)
				.MaxDesiredHeight(400.0f)
				.WidthOverride(300.0f)
				[
					SceneOutlinerModule.CreateSceneOutliner(
						InitOptions,
						FOnActorPicked::CreateLambda([Sources](AActor* Actor){
							// Create a new binding for this actor
							FSlateApplication::Get().DismissAllMenus();
							AddActorSources(Sources, MakeArrayView(&Actor, 1));
						})
					)
				];

			MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
		}
		MenuBuilder.EndSection();
	}

	bool HandleRecordTakeCommand(UWorld* InWorld, const TCHAR* InStr, FOutputDevice& Ar)
	{
#if WITH_EDITOR
		enum class EFilterType : int32
		{
			None,
			All,
			Actor,
			Class
		};

		const TCHAR* Str = InStr;
		EFilterType FilterType = EFilterType::None;
		TCHAR Filter[128];
		if (FParse::Token(Str, Filter, ARRAY_COUNT(Filter), 0))
		{
			FString const FilterStr = Filter;
			if (FilterStr == TEXT("all"))
			{
				FilterType = EFilterType::All;
			}
			else if (FilterStr == TEXT("actor"))
			{
				FilterType = EFilterType::Actor;
			}
			else if (FilterStr == TEXT("class"))
			{
				FilterType = EFilterType::Class;
			}
			else
			{
				UE_LOG(LogTakesCore, Warning, TEXT("Couldn't parse recording filter, using actor filters from settings."));
			}
		}

		TArray<AActor*> ActorsToRecord;

		if (FilterType == EFilterType::Actor || FilterType == EFilterType::Class)
		{
			TCHAR Specifier[128];
			if (FParse::Token(Str, Specifier, ARRAY_COUNT(Specifier), 0))
			{
				FString const SpecifierStr = FString(Specifier).TrimStart();

				TArray<FString> Splits;
				SpecifierStr.ParseIntoArray(Splits, TEXT(","));

				for (FString Split : Splits)
				{
					if (FilterType == EFilterType::Actor)
					{
						AActor* FoundActor = FindActorByLabel(Split, InWorld, true);
						if (FoundActor)
						{
							ActorsToRecord.Add(FoundActor);
						}
					}
					else
					{
						UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *Split);
						if (FoundClass != nullptr)
						{
							FindActorsOfClass(FoundClass, InWorld, ActorsToRecord);
						}
						else
						{
							UE_LOG(LogTakesCore, Warning, TEXT("Couldn't parse class filter, aborting recording."));
						}
					}
				}
			}
		}
		else
		{
			FindActorsOfClass(AActor::StaticClass(), InWorld, ActorsToRecord);
		}

		if (ActorsToRecord.Num())
		{
			FTakeRecorderParameters Parameters;
			Parameters.User = GetDefault<UTakeRecorderUserSettings>()->Settings;
			Parameters.Project = GetDefault<UTakeRecorderProjectSettings>()->Settings;

			FText ErrorText = NSLOCTEXT("TakeRecorderModule", "UnknownError", "An unknown error occurred when trying to start recording");

			UTakeMetaData* MetaData = UTakeMetaData::CreateFromDefaults(GetTransientPackage(), NAME_None);
			MetaData->SetFlags(RF_Transactional | RF_Transient);

			MetaData->SetSlate(GetDefault<UTakeRecorderProjectSettings>()->Settings.DefaultSlate);

			// Compute the correct starting take number
			int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(MetaData->GetSlate());
			MetaData->SetTakeNumber(NextTakeNumber);

			UTakeRecorderSources* Sources = NewObject<UTakeRecorderSources>(GetTransientPackage(), NAME_None, RF_Transient);

			for (AActor* ActorToRecord : ActorsToRecord)
			{
				UTakeRecorderActorSource::AddSourceForActor(ActorToRecord, Sources);
			}

			ULevelSequence* LevelSequence = NewObject<ULevelSequence>(GetTransientPackage(), NAME_None, RF_Transient);
			LevelSequence->Initialize();

			UTakeRecorder* NewRecorder = NewObject<UTakeRecorder>(GetTransientPackage(), NAME_None, RF_Transient);
			if (NewRecorder->Initialize(LevelSequence, Sources, MetaData, Parameters, &ErrorText))
			{
				return true;
			}
		}
		else
		{
			UE_LOG(LogTakesCore, Warning, TEXT("Couldn't find any actors to record, aborting recording."));
		}

#endif
		return false;
	}

	bool HandleStopRecordTakeCommand(UWorld* InWorld, const TCHAR* InStr, FOutputDevice& Ar)
	{
#if WITH_EDITOR
		if (UTakeRecorder* ActiveRecorder = UTakeRecorder::GetActiveRecorder())
		{
			ActiveRecorder->Stop();
		}
		return true;
#else
		return false;
#endif
	}

	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
#if WITH_EDITOR
		if (FParse::Command(&Cmd, TEXT("RecordTake")))
		{
			return HandleRecordTakeCommand(InWorld, Cmd, Ar);
		}
		else if (FParse::Command(&Cmd, TEXT("StopRecordingTake")))
		{
			return HandleStopRecordTakeCommand(InWorld, Cmd, Ar);
		}
#endif
		return false;
	}


	FActorTakeRecorderDropHandler ActorDropHandler;
	FDelegateHandle SourcesMenuExtension;
};


IMPLEMENT_MODULE(FTakeRecorderSourcesModule, TakeRecorderSources);

#undef LOCTEXT_NAMESPACE