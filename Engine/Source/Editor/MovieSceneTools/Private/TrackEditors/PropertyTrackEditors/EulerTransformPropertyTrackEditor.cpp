// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/EulerTransformPropertyTrackEditor.h"
#include "MatineeImportTools.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "SequencerUtilities.h"
#include "TransformPropertySection.h"
#include "MovieSceneToolHelpers.h"
#include "Evaluation/MovieScenePropertyTemplate.h"

TSharedRef<ISequencerTrackEditor> FEulerTransformPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FEulerTransformPropertyTrackEditor( InSequencer ) );
}


TSharedRef<ISequencerSection> FEulerTransformPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FEulerTransformPropertyTrackEditor"));

	return MakeShared<FTransformSection>(SectionObject, GetSequencer());
}


TSharedPtr<SWidget> FEulerTransformPropertyTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencer();

	const int32 RowIndex = Params.TrackInsertRowIndex;
	auto SubMenuCallback = [=]() -> TSharedRef<SWidget>
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		FSequencerUtilities::PopulateMenu_CreateNewSection(MenuBuilder, RowIndex, Track, WeakSequencer);

		return MenuBuilder.MakeWidget();
	};

	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		FSequencerUtilities::MakeAddButton(NSLOCTEXT("FEulerTransformPropertyTrackEditor", "AddSection", "Section"), FOnGetContent::CreateLambda(SubMenuCallback), Params.NodeIsHovered, GetSequencer())
	];
}


void FEulerTransformPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, FGeneratedTrackKeys& OutGeneratedKeys)
{
	const TCHAR* ChannelNames[9] = {
		TEXT("Location.X"),    TEXT("Location.Y"),     TEXT("Location.Z"),
		TEXT("Rotation.Roll"), TEXT("Rotation.Pitch"), TEXT("Rotation.Yaw"),
		TEXT("Scale.X"),       TEXT("Scale.Y"),        TEXT("Scale.Z")
	};

	bool bKeyChannels[9] = {
		true, true, true,
		true, true, true,
		true, true, true,
	};

	FString LeafPath;
	FString QualifiedLeafPath;

	int32 NumKeyedProperties = PropertyChangedParams.StructPathToKey.GetNumProperties();
	if (NumKeyedProperties >= 1)
	{
		LeafPath = PropertyChangedParams.StructPathToKey.GetPropertyInfo(NumKeyedProperties-1).Property->GetName();
	}

	if (NumKeyedProperties >= 2)
	{
		QualifiedLeafPath = PropertyChangedParams.StructPathToKey.GetPropertyInfo(NumKeyedProperties-2).Property->GetName();
		QualifiedLeafPath.AppendChar('.');
		QualifiedLeafPath += LeafPath;
	}

	if (LeafPath.Len() > 0)
	{
		for (int32 ChannelIndex = 0; ChannelIndex < ARRAY_COUNT(ChannelNames); ++ChannelIndex)
		{
			// If it doesn't match the fully qualified path, and doesn't start with the leaf path, don't add a key
			bool bMatchesQualifiedPath = FPlatformString::Stricmp(*QualifiedLeafPath, ChannelNames[ChannelIndex]) == 0;
			bool bMatchesLeafPath      = FCString::Strnicmp(ChannelNames[ChannelIndex], *LeafPath, LeafPath.Len()) == 0;

			if (!bMatchesQualifiedPath && !bMatchesLeafPath)
			{
				bKeyChannels[ChannelIndex] = false;
			}
		}
	}

	FEulerTransform Transform = PropertyChangedParams.GetPropertyValue<FEulerTransform>();

	FVector Translation = Transform.Location;
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(0, Translation.X, bKeyChannels[0]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(1, Translation.Y, bKeyChannels[1]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(2, Translation.Z, bKeyChannels[2]));

	FRotator Rotator = Transform.Rotation;
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(3, Rotator.Roll,  bKeyChannels[3]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(4, Rotator.Pitch, bKeyChannels[4]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(5, Rotator.Yaw,   bKeyChannels[5]));

	FVector Scale = Transform.Scale;
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(6, Scale.X, bKeyChannels[6]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(7, Scale.Y, bKeyChannels[7]));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(8, Scale.Z, bKeyChannels[8]));
}

bool FEulerTransformPropertyTrackEditor::ModifyGeneratedKeysByCurrentAndWeight(UObject *Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber KeyTime, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const
{

	FFrameRate TickResolution = GetSequencer()->GetFocusedTickResolution();

	FMovieSceneEvaluationTrack EvalTrack = Track->GenerateTrackTemplate();
	
	FMovieSceneInterrogationData InterrogationData;
	GetSequencer()->GetEvaluationTemplate().CopyActuators(InterrogationData.GetAccumulator());

	FMovieSceneContext Context(FMovieSceneEvaluationRange(KeyTime, GetSequencer()->GetFocusedTickResolution()));
	EvalTrack.Interrogate(Context, InterrogationData, Object);

	FVector CurrentPos; FRotator CurrentRot;
	FVector CurrentScale;
	for (const FEulerTransform& Transform : InterrogationData.Iterate<FEulerTransform>(FMovieScenePropertySectionTemplate::GetEulerTransformInterrogationKey()))
	{
		CurrentPos = Transform.Location;
		CurrentRot = Transform.Rotation;
		CurrentScale = Transform.Scale;
		break;
	}
	FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
	GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&CurrentPos.X, Weight);
	GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&CurrentPos.Y, Weight);
	GeneratedTotalKeys[2]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&CurrentPos.Z, Weight);
	GeneratedTotalKeys[3]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&CurrentRot.Roll, Weight);
	GeneratedTotalKeys[4]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&CurrentRot.Pitch, Weight);
	GeneratedTotalKeys[5]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&CurrentRot.Yaw, Weight);
	GeneratedTotalKeys[6]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&CurrentScale.X, Weight);
	GeneratedTotalKeys[7]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&CurrentScale.Y, Weight);
	GeneratedTotalKeys[8]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&CurrentScale.Z, Weight);
	return true;

}