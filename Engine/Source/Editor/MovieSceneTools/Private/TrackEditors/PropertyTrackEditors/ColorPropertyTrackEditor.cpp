// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/ColorPropertyTrackEditor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Sections/ColorPropertySection.h"
#include "MatineeImportTools.h"
#include "Matinee/InterpTrackLinearColorProp.h"
#include "Matinee/InterpTrackColorProp.h"
#include "Evaluation/MovieScenePropertyTemplate.h"

FName FColorPropertyTrackEditor::RedName( "R" );
FName FColorPropertyTrackEditor::GreenName( "G" );
FName FColorPropertyTrackEditor::BlueName( "B" );
FName FColorPropertyTrackEditor::AlphaName( "A" );
FName FColorPropertyTrackEditor::SpecifiedColorName( "SpecifiedColor" );

TSharedRef<ISequencerTrackEditor> FColorPropertyTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FColorPropertyTrackEditor(InSequencer));
}


TSharedRef<ISequencerSection> FColorPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FColorPropertyTrackEditor"));
	return MakeShared<FColorPropertySection>(SectionObject, ObjectBinding, GetSequencer());
}


void FColorPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, FGeneratedTrackKeys& OutGeneratedKeys )
{
	UProperty* Property = PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get();
	if (!Property)
	{
		return;
	}

	const UStructProperty* StructProp = Cast<const UStructProperty>( Property );
	if (!StructProp)
	{
		return;
	}

	FName StructName = StructProp->Struct->GetFName();
	FName PropertyName = Property->GetFName();

	bool bIsFColor = StructName == NAME_Color;

	FLinearColor ColorValue;

	if (bIsFColor)
	{
		ColorValue = FLinearColor( PropertyChangedParams.GetPropertyValue<FColor>() );
	}
	else
	{
		ColorValue = PropertyChangedParams.GetPropertyValue<FLinearColor>();
	}

	if( StructProp->HasMetaData("HideAlphaChannel") )
	{
		ColorValue.A = 1;
	}

	FPropertyPath StructPath = PropertyChangedParams.StructPathToKey;
	FName ChannelName = StructPath.GetNumProperties() != 0 ? StructPath.GetLeafMostProperty().Property->GetFName() : NAME_None;

	const bool bKeyRed   =  ChannelName == NAME_None || ChannelName == RedName   || ChannelName == SpecifiedColorName;
	const bool bKeyGreen =  ChannelName == NAME_None || ChannelName == GreenName || ChannelName == SpecifiedColorName;
	const bool bKeyBlue  =  ChannelName == NAME_None || ChannelName == BlueName  || ChannelName == SpecifiedColorName;
	const bool bKeyAlpha =  ChannelName == NAME_None || ChannelName == AlphaName || ChannelName == SpecifiedColorName;

	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(0, ColorValue.R, bKeyRed));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(1, ColorValue.G, bKeyGreen));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(2, ColorValue.B, bKeyBlue));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(3, ColorValue.A, bKeyAlpha));
}

void CopyInterpColorTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackColorProp* ColorPropTrack, UMovieSceneColorTrack* ColorTrack)
{
	if (FMatineeImportTools::CopyInterpColorTrack(ColorPropTrack, ColorTrack))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}

void CopyInterpLinearColorTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackLinearColorProp* LinearColorPropTrack, UMovieSceneColorTrack* ColorTrack)
{
	if (FMatineeImportTools::CopyInterpLinearColorTrack(LinearColorPropTrack, ColorTrack))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}

void FColorPropertyTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UInterpTrackColorProp* ColorPropTrack = nullptr;
	UInterpTrackLinearColorProp* LinearColorPropTrack = nullptr;
	for ( UObject* CopyPasteObject : GUnrealEd->MatineeCopyPasteBuffer )
	{
		ColorPropTrack = Cast<UInterpTrackColorProp>( CopyPasteObject );
		LinearColorPropTrack = Cast<UInterpTrackLinearColorProp>( CopyPasteObject );
		if ( ColorPropTrack != nullptr || LinearColorPropTrack != nullptr )
		{
			break;
		}
	}
	UMovieSceneColorTrack* ColorTrack = Cast<UMovieSceneColorTrack>( Track );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "PasteMatineeColorTrack", "Paste Matinee Color Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeColorTrackTooltip", "Pastes keys from a Matinee color track into this track." ),
		FSlateIcon(),
		FUIAction(
			ColorPropTrack != nullptr ? 
			FExecuteAction::CreateStatic( &CopyInterpColorTrack, GetSequencer().ToSharedRef(), ColorPropTrack, ColorTrack ) : 
			FExecuteAction::CreateStatic( &CopyInterpLinearColorTrack, GetSequencer().ToSharedRef(), LinearColorPropTrack, ColorTrack ),			
			FCanExecuteAction::CreateLambda( [=]()->bool { return ((ColorPropTrack != nullptr && ColorPropTrack->GetNumKeys() > 0) || (LinearColorPropTrack != nullptr && LinearColorPropTrack->GetNumKeys() > 0)) && ColorTrack != nullptr; } ) ) );

	MenuBuilder.AddMenuSeparator();
	FKeyframeTrackEditor::BuildTrackContextMenu(MenuBuilder, Track);
}


bool FColorPropertyTrackEditor::ModifyGeneratedKeysByCurrentAndWeight(UObject *Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber KeyTime, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const
{
	FFrameRate TickResolution = GetSequencer()->GetFocusedTickResolution();

	UMovieSceneColorTrack* ColorTrack = Cast<UMovieSceneColorTrack>(Track);
	FMovieSceneEvaluationTrack EvalTrack = Track->GenerateTrackTemplate();

	if (ColorTrack)
	{
		FMovieSceneInterrogationData InterrogationData;
		GetSequencer()->GetEvaluationTemplate().CopyActuators(InterrogationData.GetAccumulator());

		FMovieSceneContext Context(FMovieSceneEvaluationRange(KeyTime, GetSequencer()->GetFocusedTickResolution()));
		EvalTrack.Interrogate(Context, InterrogationData, Object);

		FLinearColor Val(0.0f, 0.0f, 0.0f, 0.0f);
		for (const FLinearColor& InColor : InterrogationData.Iterate<FLinearColor>(FMovieScenePropertySectionTemplate::GetColorInterrogationKey()))
		{
			Val = InColor;
			break;
		}
		FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
		GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.R, Weight);
		GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.G, Weight);
		GeneratedTotalKeys[2]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.B, Weight);
		GeneratedTotalKeys[3]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.A, Weight);
		return true;
	}
	return false;
}
