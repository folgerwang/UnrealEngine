// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/VectorPropertyTrackEditor.h"
#include "Tracks/MovieSceneVectorTrack.h"
#include "MatineeImportTools.h"
#include "Matinee/InterpTrackVectorProp.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "MovieSceneToolHelpers.h"
#include "Evaluation/MovieScenePropertyTemplate.h"


FName FVectorPropertyTrackEditor::XName( "X" );
FName FVectorPropertyTrackEditor::YName( "Y" );
FName FVectorPropertyTrackEditor::ZName( "Z" );
FName FVectorPropertyTrackEditor::WName( "W" );


TSharedRef<ISequencerTrackEditor> FVectorPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FVectorPropertyTrackEditor( InSequencer ) );
}

void FVectorPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, FGeneratedTrackKeys& OutGeneratedKeys )
{
	const UStructProperty* StructProp = Cast<const UStructProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );
	if (!StructProp)
	{
		return;
	}
	FName StructName = StructProp->Struct->GetFName();

	bool bIsVector2D = StructName == NAME_Vector2D;
	bool bIsVector = StructName == NAME_Vector;
	bool bIsVector4 = StructName == NAME_Vector4;

	FVector4 VectorValues;
	int32 Channels;

	if ( bIsVector2D )
	{
		FVector2D Vector2DValue = PropertyChangedParams.GetPropertyValue<FVector2D>();
		VectorValues.X = Vector2DValue.X;
		VectorValues.Y = Vector2DValue.Y;
		Channels = 2;
	}
	else if ( bIsVector )
	{
		FVector Vector3DValue = PropertyChangedParams.GetPropertyValue<FVector>();
		VectorValues.X = Vector3DValue.X;
		VectorValues.Y = Vector3DValue.Y;
		VectorValues.Z = Vector3DValue.Z;
		Channels = 3;
	}
	else // if ( bIsVector4 )
	{
		VectorValues = PropertyChangedParams.GetPropertyValue<FVector4>();
		Channels = 4;
	}

	FPropertyPath StructPath = PropertyChangedParams.StructPathToKey;
	FName ChannelName = StructPath.GetNumProperties() != 0 ? StructPath.GetLeafMostProperty().Property->GetFName() : NAME_None;

	const bool bKeyX = ChannelName == NAME_None || ChannelName == XName;
	const bool bKeyY = ChannelName == NAME_None || ChannelName == YName;

	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(0, VectorValues.X, bKeyX));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(1, VectorValues.Y, bKeyY));

	if ( Channels >= 3 )
	{
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(2, VectorValues.Z, ChannelName == NAME_None || ChannelName == ZName));
	}

	if ( Channels >= 4 )
	{
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(3, VectorValues.W, ChannelName == NAME_None || ChannelName == WName));
	}
}

void FVectorPropertyTrackEditor::InitializeNewTrack( UMovieSceneVectorTrack* NewTrack, FPropertyChangedParams PropertyChangedParams )
{
	FPropertyTrackEditor::InitializeNewTrack( NewTrack, PropertyChangedParams );
	const UStructProperty* StructProp = Cast<const UStructProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );
	FName StructName = StructProp->Struct->GetFName();

	if ( StructName == NAME_Vector2D )
	{
		NewTrack->SetNumChannelsUsed( 2 );
	}
	if ( StructName == NAME_Vector )
	{
		NewTrack->SetNumChannelsUsed( 3 );
	}
	if ( StructName == NAME_Vector4 )
	{
		NewTrack->SetNumChannelsUsed( 4 );
	}
}

void CopyInterpVectorTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackVectorProp* MatineeVectorTrack, UMovieSceneVectorTrack* VectorTrack)
{
	if (FMatineeImportTools::CopyInterpVectorTrack(MatineeVectorTrack, VectorTrack))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}

void FVectorPropertyTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UInterpTrackVectorProp* MatineeVectorTrack = nullptr;
	for ( UObject* CopyPasteObject : GUnrealEd->MatineeCopyPasteBuffer )
	{
		MatineeVectorTrack = Cast<UInterpTrackVectorProp>( CopyPasteObject );
		if ( MatineeVectorTrack != nullptr )
		{
			break;
		}
	}
	UMovieSceneVectorTrack* VectorTrack = Cast<UMovieSceneVectorTrack>( Track );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "PasteMatineeVectorTrack", "Paste Matinee Vector Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeVectorTrackTooltip", "Pastes keys from a Matinee vector track into this track." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateStatic( &CopyInterpVectorTrack, GetSequencer().ToSharedRef(), MatineeVectorTrack, VectorTrack ),
			FCanExecuteAction::CreateLambda( [=]()->bool { return MatineeVectorTrack != nullptr && MatineeVectorTrack->GetNumKeys() > 0 && VectorTrack != nullptr && VectorTrack->GetNumChannelsUsed() == 3; } ) ) );

	MenuBuilder.AddMenuSeparator();
	FKeyframeTrackEditor::BuildTrackContextMenu(MenuBuilder, Track);
}


bool FVectorPropertyTrackEditor::ModifyGeneratedKeysByCurrentAndWeight(UObject *Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber KeyTime, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const
{

	FFrameRate TickResolution = GetSequencer()->GetFocusedTickResolution();

	UMovieSceneVectorTrack* VectorTrack = Cast<UMovieSceneVectorTrack>(Track);
	FMovieSceneEvaluationTrack EvalTrack = Track->GenerateTrackTemplate();

	if (VectorTrack)
	{
		FMovieSceneInterrogationData InterrogationData;
		GetSequencer()->GetEvaluationTemplate().CopyActuators(InterrogationData.GetAccumulator());

		FMovieSceneContext Context(FMovieSceneEvaluationRange(KeyTime, GetSequencer()->GetFocusedTickResolution()));
		EvalTrack.Interrogate(Context, InterrogationData, Object);

		switch (VectorTrack->GetNumChannelsUsed())
		{
		case 2:
			{
				FVector2D Val(0.0f, 0.0f);
				for (const FVector2D& InVector: InterrogationData.Iterate<FVector2D>(FMovieScenePropertySectionTemplate::GetVector2DInterrogationKey()))
				{
					Val = InVector;
					break;
				}
				FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
				GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.X, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Y, Weight);
			}
			break;
		case 3:
			{
				FVector Val(0.0f, 0.0f, 0.0f);
				for (const FVector& InVector : InterrogationData.Iterate<FVector>(FMovieScenePropertySectionTemplate::GetVectorInterrogationKey()))
				{
					Val = InVector;
					break;
				}
				FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
				GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.X, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Y, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Z, Weight);
			}
			break;
		case 4:
			{
				FVector4 Val(0.0f, 0.0f, 0.0f, 0.0f);
				for (const FVector4& InVector : InterrogationData.Iterate<FVector4>(FMovieScenePropertySectionTemplate::GetVector4InterrogationKey()))
				{
					Val = InVector;
					break;
				}
				FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
				GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.X, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Y, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Z, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.W, Weight);

			}
			break;
		}
		return true;
	}
	return false;
}