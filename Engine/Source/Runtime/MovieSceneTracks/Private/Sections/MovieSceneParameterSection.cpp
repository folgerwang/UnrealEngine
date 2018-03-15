// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneParameterSection.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"

FScalarParameterNameAndCurve::FScalarParameterNameAndCurve( FName InParameterName )
{
	ParameterName = InParameterName;
}

FVectorParameterNameAndCurves::FVectorParameterNameAndCurves( FName InParameterName )
{
	ParameterName = InParameterName;
}

FColorParameterNameAndCurves::FColorParameterNameAndCurves( FName InParameterName )
{
	ParameterName = InParameterName;
}

UMovieSceneParameterSection::UMovieSceneParameterSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	bSupportsInfiniteRange = true;
	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? 
			EMovieSceneCompletionMode::KeepState : 
			GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);
}

void UMovieSceneParameterSection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		ReconstructChannelProxy();
	}
}

void UMovieSceneParameterSection::ReconstructChannelProxy()
{
	FMovieSceneChannelData Channels;

#if WITH_EDITOR

	for ( FScalarParameterNameAndCurve& Scalar : GetScalarParameterNamesAndCurves() )
	{
		FMovieSceneChannelEditorData EditorData(Scalar.ParameterName, FText::FromName(Scalar.ParameterName));
		// Prevent single channels from collapsing to the track node
		EditorData.bCanCollapseToTrack = false;
		Channels.Add(Scalar.ParameterCurve, EditorData, TMovieSceneExternalValue<float>());
	}
	for ( FVectorParameterNameAndCurves& Vector : GetVectorParameterNamesAndCurves() )
	{
		FString ParameterString = Vector.ParameterName.ToString();
		FText Group = FText::FromString(ParameterString);

		Channels.Add(Vector.XCurve, FMovieSceneChannelEditorData(*(ParameterString + TEXT(".X")), FCommonChannelData::ChannelX, Group), TMovieSceneExternalValue<float>());
		Channels.Add(Vector.YCurve, FMovieSceneChannelEditorData(*(ParameterString + TEXT(".Y")), FCommonChannelData::ChannelY, Group), TMovieSceneExternalValue<float>());
		Channels.Add(Vector.ZCurve, FMovieSceneChannelEditorData(*(ParameterString + TEXT(".Z")), FCommonChannelData::ChannelZ, Group), TMovieSceneExternalValue<float>());
	}
	for ( FColorParameterNameAndCurves& Color : GetColorParameterNamesAndCurves() )
	{
		FString ParameterString = Color.ParameterName.ToString();
		FText Group = FText::FromString(ParameterString);

		FMovieSceneChannelEditorData RChannelData(*(ParameterString + TEXT("R")), FCommonChannelData::ChannelR, Group);
		RChannelData.SortOrder = 0;
		RChannelData.Color = FCommonChannelData::RedChannelColor;

		FMovieSceneChannelEditorData GChannelData(*(ParameterString + TEXT("G")), FCommonChannelData::ChannelG, Group);
		GChannelData.SortOrder = 1;
		GChannelData.Color = FCommonChannelData::GreenChannelColor;

		FMovieSceneChannelEditorData BChannelData(*(ParameterString + TEXT("B")), FCommonChannelData::ChannelB, Group);
		BChannelData.SortOrder = 2;
		BChannelData.Color = FCommonChannelData::BlueChannelColor;

		FMovieSceneChannelEditorData AChannelData(*(ParameterString + TEXT("A")), FCommonChannelData::ChannelA, Group);
		AChannelData.SortOrder = 3;

		Channels.Add(Color.RedCurve,   RChannelData, TMovieSceneExternalValue<float>());
		Channels.Add(Color.GreenCurve, GChannelData, TMovieSceneExternalValue<float>());
		Channels.Add(Color.BlueCurve,  BChannelData, TMovieSceneExternalValue<float>());
		Channels.Add(Color.AlphaCurve, AChannelData, TMovieSceneExternalValue<float>());
	}

#else

	for ( FScalarParameterNameAndCurve& Scalar : GetScalarParameterNamesAndCurves() )
	{
		Channels.Add(Scalar.ParameterCurve);
	}
	for ( FVectorParameterNameAndCurves& Vector : GetVectorParameterNamesAndCurves() )
	{
		Channels.Add(Vector.XCurve);
		Channels.Add(Vector.YCurve);
		Channels.Add(Vector.ZCurve);
	}
	for ( FColorParameterNameAndCurves& Color : GetColorParameterNamesAndCurves() )
	{
		Channels.Add(Color.RedCurve);
		Channels.Add(Color.GreenCurve);
		Channels.Add(Color.BlueCurve);
		Channels.Add(Color.AlphaCurve);
	}

#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

void UMovieSceneParameterSection::AddScalarParameterKey( FName InParameterName, FFrameNumber InTime, float InValue )
{
	FMovieSceneFloatChannel* ExistingChannel = nullptr;
	for ( FScalarParameterNameAndCurve& ScalarParameterNameAndCurve : ScalarParameterNamesAndCurves )
	{
		if ( ScalarParameterNameAndCurve.ParameterName == InParameterName )
		{
			ExistingChannel = &ScalarParameterNameAndCurve.ParameterCurve;
			break;
		}
	}
	if ( ExistingChannel == nullptr )
	{
		const int32 NewIndex = ScalarParameterNamesAndCurves.Add( FScalarParameterNameAndCurve( InParameterName ) );
		ExistingChannel = &ScalarParameterNamesAndCurves[NewIndex].ParameterCurve;

		ReconstructChannelProxy();
	}

	ExistingChannel->AddCubicKey(InTime, InValue);

	if (TryModify())
	{
		SetRange(TRange<FFrameNumber>::Hull(TRange<FFrameNumber>(InTime), GetRange()));
	}
}

void UMovieSceneParameterSection::AddVectorParameterKey( FName InParameterName, FFrameNumber InTime, FVector InValue )
{
	FVectorParameterNameAndCurves* ExistingCurves = nullptr;
	for ( FVectorParameterNameAndCurves& VectorParameterNameAndCurve : VectorParameterNamesAndCurves )
	{
		if ( VectorParameterNameAndCurve.ParameterName == InParameterName )
		{
			ExistingCurves = &VectorParameterNameAndCurve;
			break;
		}
	}
	if ( ExistingCurves == nullptr )
	{
		int32 NewIndex = VectorParameterNamesAndCurves.Add( FVectorParameterNameAndCurves( InParameterName ) );
		ExistingCurves = &VectorParameterNamesAndCurves[NewIndex];

		ReconstructChannelProxy();
	}

	ExistingCurves->XCurve.AddCubicKey(InTime, InValue.X);
	ExistingCurves->YCurve.AddCubicKey(InTime, InValue.Y);
	ExistingCurves->ZCurve.AddCubicKey(InTime, InValue.Z);

	if (TryModify())
	{
		SetRange(TRange<FFrameNumber>::Hull(TRange<FFrameNumber>(InTime), GetRange()));
	}
}

void UMovieSceneParameterSection::AddColorParameterKey( FName InParameterName, FFrameNumber InTime, FLinearColor InValue )
{
	FColorParameterNameAndCurves* ExistingCurves = nullptr;
	for ( FColorParameterNameAndCurves& ColorParameterNameAndCurve : ColorParameterNamesAndCurves )
	{
		if ( ColorParameterNameAndCurve.ParameterName == InParameterName )
		{
			ExistingCurves = &ColorParameterNameAndCurve;
			break;
		}
	}
	if ( ExistingCurves == nullptr )
	{
		int32 NewIndex = ColorParameterNamesAndCurves.Add( FColorParameterNameAndCurves( InParameterName ) );
		ExistingCurves = &ColorParameterNamesAndCurves[NewIndex];

		ReconstructChannelProxy();
	}

	ExistingCurves->RedCurve.AddCubicKey(   InTime, InValue.R );
	ExistingCurves->GreenCurve.AddCubicKey( InTime, InValue.G );
	ExistingCurves->BlueCurve.AddCubicKey(  InTime, InValue.B );
	ExistingCurves->AlphaCurve.AddCubicKey( InTime, InValue.A );

	if (TryModify())
	{
		SetRange(TRange<FFrameNumber>::Hull(TRange<FFrameNumber>(InTime), GetRange()));
	}
}

bool UMovieSceneParameterSection::RemoveScalarParameter( FName InParameterName )
{
	for ( int32 i = 0; i < ScalarParameterNamesAndCurves.Num(); i++ )
	{
		if ( ScalarParameterNamesAndCurves[i].ParameterName == InParameterName )
		{
			ScalarParameterNamesAndCurves.RemoveAt(i);
			ReconstructChannelProxy();
			return true;
		}
	}
	return false;
}

bool UMovieSceneParameterSection::RemoveVectorParameter( FName InParameterName )
{
	for ( int32 i = 0; i < VectorParameterNamesAndCurves.Num(); i++ )
	{
		if ( VectorParameterNamesAndCurves[i].ParameterName == InParameterName )
		{
			VectorParameterNamesAndCurves.RemoveAt( i );
			ReconstructChannelProxy();
			return true;
		}
	}
	return false;
}

bool UMovieSceneParameterSection::RemoveColorParameter( FName InParameterName )
{
	for ( int32 i = 0; i < ColorParameterNamesAndCurves.Num(); i++ )
	{
		if ( ColorParameterNamesAndCurves[i].ParameterName == InParameterName )
		{
			ColorParameterNamesAndCurves.RemoveAt( i );
			ReconstructChannelProxy();
			return true;
		}
	}
	return false;
}

TArray<FScalarParameterNameAndCurve>& UMovieSceneParameterSection::GetScalarParameterNamesAndCurves()
{
	return ScalarParameterNamesAndCurves;
}

const TArray<FScalarParameterNameAndCurve>& UMovieSceneParameterSection::GetScalarParameterNamesAndCurves() const
{
	return ScalarParameterNamesAndCurves;
}

TArray<FVectorParameterNameAndCurves>& UMovieSceneParameterSection::GetVectorParameterNamesAndCurves()
{
	return VectorParameterNamesAndCurves;
}

const TArray<FVectorParameterNameAndCurves>& UMovieSceneParameterSection::GetVectorParameterNamesAndCurves() const
{
	return VectorParameterNamesAndCurves;
}

TArray<FColorParameterNameAndCurves>& UMovieSceneParameterSection::GetColorParameterNamesAndCurves()
{
	return ColorParameterNamesAndCurves;
}

const TArray<FColorParameterNameAndCurves>& UMovieSceneParameterSection::GetColorParameterNamesAndCurves() const
{
	return ColorParameterNamesAndCurves;
}

void UMovieSceneParameterSection::GetParameterNames( TSet<FName>& ParameterNames ) const
{
	for ( const FScalarParameterNameAndCurve& ScalarParameterNameAndCurve : ScalarParameterNamesAndCurves )
	{
		ParameterNames.Add( ScalarParameterNameAndCurve.ParameterName );
	}
	for ( const FVectorParameterNameAndCurves& VectorParameterNameAndCurves : VectorParameterNamesAndCurves )
	{
		ParameterNames.Add( VectorParameterNameAndCurves.ParameterName );
	}
	for ( const FColorParameterNameAndCurves& ColorParameterNameAndCurves : ColorParameterNamesAndCurves )
	{
		ParameterNames.Add( ColorParameterNameAndCurves.ParameterName );
	}
}