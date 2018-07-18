// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Animation/Sequencer2DTransformTrackEditor.h"
#include "Animation/Sequencer2DTransformSection.h"
#include "Slate/WidgetTransform.h"
#include "ISectionLayoutBuilder.h"


FName F2DTransformTrackEditor::TranslationName( "Translation" );
FName F2DTransformTrackEditor::ScaleName( "Scale" );
FName F2DTransformTrackEditor::ShearName( "Shear" );
FName F2DTransformTrackEditor::AngleName( "Angle" );
FName F2DTransformTrackEditor::ChannelXName( "X" );
FName F2DTransformTrackEditor::ChannelYName( "Y" );

TSharedRef<ISequencerTrackEditor> F2DTransformTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F2DTransformTrackEditor( InSequencer ) );
}


TSharedRef<ISequencerSection> F2DTransformTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShared<F2DTransformSection>(SectionObject, GetSequencer());
}


void F2DTransformTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, FGeneratedTrackKeys& OutGeneratedKeys )
{
	FPropertyPath StructPath = PropertyChangedParams.StructPathToKey;

	bool bKeyTranslationX = false;
	bool bKeyTranslationY = false;
	bool bKeyAngle = false;
	bool bKeyScaleX = false;
	bool bKeyScaleY = false;
	bool bKeyShearX = false;
	bool bKeyShearY = false;

	if (StructPath.GetNumProperties() == 0)
	{
		bKeyTranslationX = bKeyTranslationY = bKeyAngle = bKeyScaleX = bKeyScaleY = bKeyShearX = bKeyShearY = true;
	}
	else
	{
		if (StructPath.GetRootProperty().Property->GetFName() == TranslationName)
		{
			if (StructPath.GetLeafMostProperty() != StructPath.GetRootProperty())
			{
				bKeyTranslationX = StructPath.GetLeafMostProperty().Property->GetFName() == ChannelXName;
				bKeyTranslationY = StructPath.GetLeafMostProperty().Property->GetFName() == ChannelYName;
			}
			else
			{
				bKeyTranslationX = bKeyTranslationY = true;
			}
		}

		if (StructPath.GetRootProperty().Property->GetFName() == AngleName)
		{
			bKeyAngle = true;
		}

		if (StructPath.GetRootProperty().Property->GetFName() == ScaleName)
		{
			if (StructPath.GetLeafMostProperty() != StructPath.GetRootProperty())
			{
				bKeyScaleX = StructPath.GetLeafMostProperty().Property->GetFName() == ChannelXName;
				bKeyScaleY = StructPath.GetLeafMostProperty().Property->GetFName() == ChannelYName;
			}
			else
			{
				bKeyScaleX = bKeyScaleY = true;
			}
		}

		if (StructPath.GetRootProperty().Property->GetFName() == ShearName)
		{
			if (StructPath.GetLeafMostProperty() != StructPath.GetRootProperty())
			{
				bKeyShearX = StructPath.GetLeafMostProperty().Property->GetFName() == ChannelXName;
				bKeyShearY = StructPath.GetLeafMostProperty().Property->GetFName() == ChannelYName;
			}
			else
			{
				bKeyShearX = bKeyShearY = true;
			}
		}
	}

	FWidgetTransform Transform = PropertyChangedParams.GetPropertyValue<FWidgetTransform>();

	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(0, Transform.Translation.X, bKeyTranslationX));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(1, Transform.Translation.Y, bKeyTranslationY));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(2, Transform.Angle,         bKeyAngle));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(3, Transform.Scale.X,       bKeyScaleX));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(4, Transform.Scale.Y,       bKeyScaleY));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(5, Transform.Shear.X,       bKeyShearX));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(6, Transform.Shear.Y,       bKeyShearY));
}