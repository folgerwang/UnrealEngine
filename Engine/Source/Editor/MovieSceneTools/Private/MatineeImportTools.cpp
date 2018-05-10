// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MatineeImportTools.h"
#include "MovieSceneSequence.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "ScopedTransaction.h"
#include "MovieSceneCommonHelpers.h"
#include "IMovieScenePlayer.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieScene.h"

#include "Matinee/MatineeActor.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpTrackLinearColorProp.h"
#include "Matinee/InterpTrackColorProp.h"
#include "Matinee/InterpTrackBoolProp.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/InterpTrackSound.h"
#include "Matinee/InterpTrackFade.h"
#include "Matinee/InterpTrackDirector.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpTrackVectorProp.h"
#include "Matinee/InterpTrackVisibility.h"

#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneParticleTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneVisibilityTrack.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneVectorTrack.h"

#include "Sections/MovieSceneColorSection.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sections/MovieSceneFadeSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneEventSection.h"
#include "Sections/MovieSceneVectorSection.h"


#include "Animation/AnimSequence.h"


ERichCurveInterpMode FMatineeImportTools::MatineeInterpolationToRichCurveInterpolation( EInterpCurveMode CurveMode )
{
	switch ( CurveMode )
	{
	case CIM_Constant:
		return ERichCurveInterpMode::RCIM_Constant;
	case CIM_CurveAuto:
	case CIM_CurveAutoClamped:
	case CIM_CurveBreak:
	case CIM_CurveUser:
		return ERichCurveInterpMode::RCIM_Cubic;
	case CIM_Linear:
		return ERichCurveInterpMode::RCIM_Linear;
	default:
		return ERichCurveInterpMode::RCIM_None;
	}
}


ERichCurveTangentMode FMatineeImportTools::MatineeInterpolationToRichCurveTangent( EInterpCurveMode CurveMode )
{
	switch ( CurveMode )
	{
	case CIM_CurveBreak:
		return ERichCurveTangentMode::RCTM_Break;
	case CIM_CurveUser:
	// Import auto-clamped curves as user curves because rich curves don't have support for clamped tangents, and if the
	// user moves the converted keys, the tangents will get mangled.
	case CIM_CurveAutoClamped:
		return ERichCurveTangentMode::RCTM_User;
	default:
		return ERichCurveTangentMode::RCTM_Auto;
	}
}

void CleanupCurveKeys(FMovieSceneFloatChannel* InChannel)
{
	FKeyDataOptimizationParams Params;
	Params.bAutoSetInterpolation = true;

	MovieScene::Optimize(InChannel, Params);
}


bool FMatineeImportTools::TryConvertMatineeToggleToOutParticleKey( ETrackToggleAction ToggleAction, EParticleKey& OutParticleKey )
{
	switch ( ToggleAction )
	{
	case ETrackToggleAction::ETTA_On:
		OutParticleKey = EParticleKey::Activate;
		return true;
	case ETrackToggleAction::ETTA_Off:
		OutParticleKey = EParticleKey::Deactivate;
		return true;
	case ETrackToggleAction::ETTA_Trigger:
		OutParticleKey = EParticleKey::Trigger;
		return true;
	}
	return false;
}


void FMatineeImportTools::SetOrAddKey(TMovieSceneChannelData<FMovieSceneFloatValue>& ChannelData, FFrameNumber Time, float Value, float ArriveTangent, float LeaveTangent, EInterpCurveMode MatineeInterpMode
	, ERichCurveTangentWeightMode WeightedMode, float ArriveTangentWeight, float LeaveTangentWeight )
{
	if (ChannelData.FindKey(Time) == INDEX_NONE)
	{
		FMovieSceneFloatValue NewKey(Value);

		NewKey.InterpMode = MatineeInterpolationToRichCurveInterpolation( MatineeInterpMode );
		NewKey.TangentMode = MatineeInterpolationToRichCurveTangent( MatineeInterpMode );
		NewKey.Tangent.ArriveTangent = ArriveTangent;
		NewKey.Tangent.LeaveTangent = LeaveTangent;
		NewKey.Tangent.TangentWeightMode = WeightedMode;
		NewKey.Tangent.ArriveTangentWeight = ArriveTangentWeight;
		NewKey.Tangent.LeaveTangentWeight = LeaveTangentWeight;
		ChannelData.AddKey( Time, NewKey );
	}
}


bool FMatineeImportTools::CopyInterpBoolTrack( UInterpTrackBoolProp* MatineeBoolTrack, UMovieSceneBoolTrack* BoolTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeFBoolTrack", "Paste Matinee Bool Track" ) );
	bool bSectionCreated = false;

	BoolTrack->Modify();

	FFrameRate   FrameRate    = BoolTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber FirstKeyTime = (MatineeBoolTrack->GetKeyframeTime( 0 ) * FrameRate).RoundToFrame();

	UMovieSceneBoolSection* Section = Cast<UMovieSceneBoolSection>( MovieSceneHelpers::FindSectionAtTime( BoolTrack->GetAllSections(), FirstKeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneBoolSection>( BoolTrack->CreateNewSection() );
		BoolTrack->AddSection( *Section );
		Section->SetRange(TRange<FFrameNumber>::All());
		bSectionCreated = true;
	}
	if (Section->TryModify())
	{
		TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

		TMovieSceneChannelData<bool> ChannelData = Section->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0)->GetData();
		for ( const auto& Point : MatineeBoolTrack->BoolTrack )
		{
			FFrameNumber KeyTime = (Point.Time * FrameRate).RoundToFrame();

			ChannelData.UpdateOrAddKey(KeyTime, Point.Value);

			KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
		}

		if (!KeyRange.IsEmpty())
		{
			Section->SetRange( KeyRange );
		}
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpFloatTrack( UInterpTrackFloatBase* MatineeFloatTrack, UMovieSceneFloatTrack* FloatTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeFloatTrack", "Paste Matinee Float Track" ) );
	bool bSectionCreated = false;

	FloatTrack->Modify();

	FFrameRate   FrameRate    = FloatTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber FirstKeyTime = (MatineeFloatTrack->GetKeyframeTime( 0 ) * FrameRate).RoundToFrame();;

	UMovieSceneFloatSection* Section = Cast<UMovieSceneFloatSection>( MovieSceneHelpers::FindSectionAtTime( FloatTrack->GetAllSections(), FirstKeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneFloatSection>( FloatTrack->CreateNewSection() );
		FloatTrack->AddSection( *Section );
		Section->SetRange(TRange<FFrameNumber>::All());
		bSectionCreated = true;
	}
	if (Section->TryModify())
	{
		TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

		FMovieSceneFloatChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
		TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = Channel->GetData();

		for ( const auto& Point : MatineeFloatTrack->FloatTrack.Points )
		{
			FFrameNumber KeyTime = (Point.InVal * FrameRate).RoundToFrame();

			FMatineeImportTools::SetOrAddKey( ChannelData, KeyTime, Point.OutVal, Point.ArriveTangent, Point.LeaveTangent, Point.InterpMode );

			KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
		}

		CleanupCurveKeys(Channel);

		if (!KeyRange.IsEmpty())
		{
			Section->SetRange( KeyRange );
		}
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpVectorTrack( UInterpTrackVectorProp* MatineeVectorTrack, UMovieSceneVectorTrack* VectorTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeVectorTrack", "Paste Matinee Vector Track" ) );
	bool bSectionCreated = false;

	VectorTrack->Modify();

	FFrameRate   FrameRate    = VectorTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber FirstKeyTime = (MatineeVectorTrack->GetKeyframeTime( 0 ) * FrameRate).RoundToFrame();

	UMovieSceneVectorSection* Section = Cast<UMovieSceneVectorSection>( MovieSceneHelpers::FindSectionAtTime( VectorTrack->GetAllSections(), FirstKeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneVectorSection>( VectorTrack->CreateNewSection() );
		VectorTrack->AddSection( *Section );
		Section->SetRange(TRange<FFrameNumber>::All());
		bSectionCreated = true;
	}
	if (Section->TryModify())
	{
		TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

		if (Section->GetChannelsUsed() == 3)
		{
			TArrayView<FMovieSceneFloatChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
			TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData[3] = { Channels[0]->GetData(), Channels[1]->GetData(), Channels[2]->GetData() };

			for ( const auto& Point : MatineeVectorTrack->VectorTrack.Points )
			{
				FFrameNumber KeyTime = (Point.InVal * FrameRate).RoundToFrame();

				FMatineeImportTools::SetOrAddKey( ChannelData[0], KeyTime, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
				FMatineeImportTools::SetOrAddKey( ChannelData[1], KeyTime, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
				FMatineeImportTools::SetOrAddKey( ChannelData[2], KeyTime, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );

				KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
			}
			
			CleanupCurveKeys(Channels[0]);
			CleanupCurveKeys(Channels[1]);
			CleanupCurveKeys(Channels[2]);
		}

		if (!KeyRange.IsEmpty())
		{
			Section->SetRange( KeyRange );
		}
	}

	return bSectionCreated;
}


bool FMatineeImportTools::CopyInterpColorTrack( UInterpTrackColorProp* ColorPropTrack, UMovieSceneColorTrack* ColorTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeColorTrack", "Paste Matinee Color Track" ) );
	bool bSectionCreated = false;

	ColorTrack->Modify();

	FFrameRate   FrameRate    = ColorTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber FirstKeyTime = (ColorPropTrack->GetKeyframeTime( 0 ) * FrameRate).RoundToFrame();

	UMovieSceneColorSection* Section = Cast<UMovieSceneColorSection>( MovieSceneHelpers::FindSectionAtTime( ColorTrack->GetAllSections(), FirstKeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneColorSection>( ColorTrack->CreateNewSection() );
		ColorTrack->AddSection( *Section );

		TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
		FloatChannels[0]->SetDefault(0.f);
		FloatChannels[1]->SetDefault(0.f);
		FloatChannels[2]->SetDefault(0.f);
		FloatChannels[3]->SetDefault(1.f);

		Section->SetRange(TRange<FFrameNumber>::All());
		bSectionCreated = true;
	}

	if (Section->TryModify())
	{
		TArrayView<FMovieSceneFloatChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
		TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData[3] = { Channels[0]->GetData(), Channels[1]->GetData(), Channels[2]->GetData() };

		TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

		for ( const FInterpCurvePoint<FVector>& Point : ColorPropTrack->VectorTrack.Points )
		{
			FFrameNumber KeyTime = (Point.InVal * FrameRate).RoundToFrame();

			FMatineeImportTools::SetOrAddKey( ChannelData[0], KeyTime, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( ChannelData[1], KeyTime, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( ChannelData[2], KeyTime, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );

			KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
		}

		CleanupCurveKeys(Channels[0]);
		CleanupCurveKeys(Channels[1]);
		CleanupCurveKeys(Channels[2]);

		if (!KeyRange.IsEmpty())
		{
			Section->SetRange( KeyRange );
		}
	}

	return bSectionCreated;
}


bool FMatineeImportTools::CopyInterpLinearColorTrack( UInterpTrackLinearColorProp* LinearColorPropTrack, UMovieSceneColorTrack* ColorTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeLinearColorTrack", "Paste Matinee Linear Color Track" ) );
	bool bSectionCreated = false;

	ColorTrack->Modify();

	FFrameRate   FrameRate    = ColorTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber FirstKeyTime = (LinearColorPropTrack->GetKeyframeTime( 0 ) * FrameRate).RoundToFrame();

	UMovieSceneColorSection* Section = Cast<UMovieSceneColorSection>( MovieSceneHelpers::FindSectionAtTime( ColorTrack->GetAllSections(), FirstKeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneColorSection>( ColorTrack->CreateNewSection() );
		ColorTrack->AddSection( *Section );

		TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
		FloatChannels[0]->SetDefault(0.f);
		FloatChannels[1]->SetDefault(0.f);
		FloatChannels[2]->SetDefault(0.f);
		FloatChannels[3]->SetDefault(1.f);

		Section->SetRange(TRange<FFrameNumber>::All());
		bSectionCreated = true;
	}

	if (Section->TryModify())
	{
		TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

		TArrayView<FMovieSceneFloatChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
		TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData[4] = { Channels[0]->GetData(), Channels[1]->GetData(), Channels[2]->GetData(), Channels[3]->GetData() };

		for ( const auto& Point : LinearColorPropTrack->LinearColorTrack.Points )
		{
			FFrameNumber KeyTime = (Point.InVal * FrameRate).RoundToFrame();

			FMatineeImportTools::SetOrAddKey( ChannelData[0], KeyTime, Point.OutVal.R, Point.ArriveTangent.R, Point.LeaveTangent.R, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( ChannelData[1], KeyTime, Point.OutVal.G, Point.ArriveTangent.G, Point.LeaveTangent.G, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( ChannelData[2], KeyTime, Point.OutVal.B, Point.ArriveTangent.B, Point.LeaveTangent.B, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( ChannelData[3], KeyTime, Point.OutVal.A, Point.ArriveTangent.A, Point.LeaveTangent.A, Point.InterpMode );

			KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
		}

		CleanupCurveKeys(Channels[0]);
		CleanupCurveKeys(Channels[1]);
		CleanupCurveKeys(Channels[2]);
		CleanupCurveKeys(Channels[3]);

		if (!KeyRange.IsEmpty())
		{
			Section->SetRange( KeyRange );
		}
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpMoveTrack( UInterpTrackMove* MoveTrack, UMovieScene3DTransformTrack* TransformTrack, const FVector& DefaultScale )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeMoveTrack", "Paste Matinee Move Track" ) );
	bool bSectionCreated = false;

	TransformTrack->Modify();

	FFrameRate   FrameRate    = TransformTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber FirstKeyTime = (MoveTrack->GetKeyframeTime( 0 ) * FrameRate).RoundToFrame();

	UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>( MovieSceneHelpers::FindSectionAtTime( TransformTrack->GetAllSections(), FirstKeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieScene3DTransformSection>( TransformTrack->CreateNewSection() );

		TransformTrack->AddSection( *Section );
		Section->SetRange(TRange<FFrameNumber>::All());
		bSectionCreated = true;

		TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
		FloatChannels[6]->SetDefault(DefaultScale.X);
		FloatChannels[7]->SetDefault(DefaultScale.Y);
		FloatChannels[8]->SetDefault(DefaultScale.Z);
	}

	if (Section->TryModify())
	{
		TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

		TArrayView<FMovieSceneFloatChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
		TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData[6] = {
			Channels[0]->GetData(), Channels[1]->GetData(), Channels[2]->GetData(),
			Channels[3]->GetData(), Channels[4]->GetData(), Channels[5]->GetData()
		};

		for ( const auto& Point : MoveTrack->PosTrack.Points )
		{
			FFrameNumber KeyTime = (Point.InVal * FrameRate).RoundToFrame();

			FMatineeImportTools::SetOrAddKey( ChannelData[0], KeyTime, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( ChannelData[1], KeyTime, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( ChannelData[2], KeyTime, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );

			KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
		}

		for ( const auto& Point : MoveTrack->EulerTrack.Points )
		{
			FFrameNumber KeyTime = (Point.InVal * FrameRate).RoundToFrame();

			FMatineeImportTools::SetOrAddKey( ChannelData[3], KeyTime, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( ChannelData[4], KeyTime, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( ChannelData[5], KeyTime, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );

			KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
		}

		for (auto SubTrack : MoveTrack->SubTracks)
		{
			if (SubTrack->IsA(UInterpTrackMoveAxis::StaticClass()))
			{
				UInterpTrackMoveAxis* MoveSubTrack = Cast<UInterpTrackMoveAxis>(SubTrack);
				if (MoveSubTrack)
				{
					int32 ChannelIndex = INDEX_NONE;

					if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_TranslationX)
					{
						ChannelIndex = 0;
					}
					else if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_TranslationY)
					{
						ChannelIndex = 1;
					}
					else if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_TranslationZ)
					{
						ChannelIndex = 2;
					}
					else if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_RotationX)
					{
						ChannelIndex = 3;
					}
					else if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_RotationY)
					{
						ChannelIndex = 4;
					}
					else if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_RotationZ)
					{
						ChannelIndex = 5;
					}

					if (ChannelIndex != INDEX_NONE)
					{
						for (const auto& Point : MoveSubTrack->FloatTrack.Points)
						{
							FFrameNumber KeyTime = (Point.InVal * FrameRate).RoundToFrame();

							FMatineeImportTools::SetOrAddKey( ChannelData[ChannelIndex], KeyTime, Point.OutVal, Point.ArriveTangent, Point.LeaveTangent, Point.InterpMode );

							KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
						}
					}
				}
			}
		}

		CleanupCurveKeys(Channels[0]);
		CleanupCurveKeys(Channels[1]);
		CleanupCurveKeys(Channels[2]);

		CleanupCurveKeys(Channels[3]);
		CleanupCurveKeys(Channels[4]);
		CleanupCurveKeys(Channels[5]);

		if (!KeyRange.IsEmpty())
		{
			Section->SetRange( KeyRange );
		}
	}

	return bSectionCreated;
}


bool FMatineeImportTools::CopyInterpParticleTrack( UInterpTrackToggle* MatineeToggleTrack, UMovieSceneParticleTrack* ParticleTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeParticleTrack", "Paste Matinee Particle Track" ) );
	bool bSectionCreated = false;

	ParticleTrack->Modify();

	FFrameRate   FrameRate    = ParticleTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber FirstKeyTime = (MatineeToggleTrack->GetKeyframeTime( 0 ) * FrameRate).RoundToFrame();

	UMovieSceneParticleSection* Section = Cast<UMovieSceneParticleSection>( MovieSceneHelpers::FindSectionAtTime( ParticleTrack->GetAllSections(), FirstKeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneParticleSection>( ParticleTrack->CreateNewSection() );
		ParticleTrack->AddSection( *Section );
		bSectionCreated = true;
	}

	if (Section->TryModify())
	{
		TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

		FMovieSceneParticleChannel* ParticleChannel = Section->GetChannelProxy().GetChannel<FMovieSceneParticleChannel>(0);
		check(ParticleChannel);
		TMovieSceneChannelData<uint8> ChannelData = ParticleChannel->GetData();

		for ( const auto& Key : MatineeToggleTrack->ToggleTrack )
		{
			FFrameNumber KeyTime = (Key.Time * FrameRate).RoundToFrame();

			EParticleKey ParticleKey;
			if ( TryConvertMatineeToggleToOutParticleKey( Key.ToggleAction, ParticleKey ) )
			{
				ChannelData.AddKey( KeyTime, (uint8)ParticleKey );
			}

			KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
		}

		if (!KeyRange.IsEmpty())
		{
			Section->SetRange( KeyRange );
		}
	}

	return bSectionCreated;
}


bool FMatineeImportTools::CopyInterpAnimControlTrack( UInterpTrackAnimControl* MatineeAnimControlTrack, UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack, FFrameNumber EndPlaybackRange )
{
	// @todo - Sequencer - Add support for slot names once they are implemented.
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeAnimTrack", "Paste Matinee Anim Track" ) );
	bool bSectionCreated = false;

	FFrameRate FrameRate = SkeletalAnimationTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();

	SkeletalAnimationTrack->Modify();
	SkeletalAnimationTrack->RemoveAllAnimationData();

	for (int32 i = 0; i < MatineeAnimControlTrack->AnimSeqs.Num(); i++)
	{
		const auto& AnimSeq = MatineeAnimControlTrack->AnimSeqs[i];

		float EndTime;
		if( AnimSeq.bLooping )
		{
			if( i < MatineeAnimControlTrack->AnimSeqs.Num() - 1 )
			{
				EndTime = MatineeAnimControlTrack->AnimSeqs[i + 1].StartTime;
			}
			else
			{
				EndTime = EndPlaybackRange / FrameRate;
			}
		}
		else
		{
			EndTime = AnimSeq.StartTime + ( ( ( AnimSeq.AnimSeq->SequenceLength - AnimSeq.AnimEndOffset ) - AnimSeq.AnimStartOffset ) / AnimSeq.AnimPlayRate );

			// Clamp to next clip's start time
			if (i+1 < MatineeAnimControlTrack->AnimSeqs.Num())
			{
				float NextStartTime = MatineeAnimControlTrack->AnimSeqs[i+1].StartTime;
				EndTime = FMath::Min(NextStartTime, EndTime);
			}
		}

		UMovieSceneSkeletalAnimationSection* NewSection = Cast<UMovieSceneSkeletalAnimationSection>( SkeletalAnimationTrack->CreateNewSection() );
		NewSection->SetRange( TRange<FFrameNumber>((AnimSeq.StartTime * FrameRate).RoundToFrame(), (EndTime * FrameRate).RoundToFrame() + 1) );
		NewSection->Params.StartOffset = AnimSeq.AnimStartOffset;
		NewSection->Params.EndOffset = AnimSeq.AnimEndOffset;
		NewSection->Params.PlayRate = AnimSeq.AnimPlayRate;
		NewSection->Params.Animation = AnimSeq.AnimSeq;
		NewSection->Params.SlotName = MatineeAnimControlTrack->SlotName;

		SkeletalAnimationTrack->AddSection( *NewSection );
		bSectionCreated = true;
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpSoundTrack( UInterpTrackSound* MatineeSoundTrack, UMovieSceneAudioTrack* AudioTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeSoundTrack", "Paste Matinee Sound Track" ) );
	bool bSectionCreated = false;

	FFrameRate FrameRate = AudioTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();

	AudioTrack->Modify();

	int MaxSectionRowIndex = -1;
	for ( UMovieSceneSection* Section : AudioTrack->GetAllSections() )
	{
		MaxSectionRowIndex = FMath::Max( MaxSectionRowIndex, Section->GetRowIndex() );
	}

	for ( const FSoundTrackKey& SoundTrackKey : MatineeSoundTrack->Sounds )
	{
		AudioTrack->AddNewSound( SoundTrackKey.Sound, (SoundTrackKey.Time * FrameRate).RoundToFrame() );

		UMovieSceneAudioSection* NewAudioSection = Cast<UMovieSceneAudioSection>(AudioTrack->GetAllSections().Last());
		NewAudioSection->SetRowIndex( MaxSectionRowIndex + 1 );
		
		TArrayView<FMovieSceneFloatChannel*> FloatChannels = NewAudioSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
		FloatChannels[0]->SetDefault(SoundTrackKey.Volume);
		FloatChannels[1]->SetDefault(SoundTrackKey.Pitch);

		AudioTrack->AddSection( *NewAudioSection );
		bSectionCreated = true;
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpFadeTrack( UInterpTrackFade* MatineeFadeTrack, UMovieSceneFadeTrack* FadeTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeFadeTrack", "Paste Matinee Fade Track" ) );
	bool bSectionCreated = false;

	FadeTrack->Modify();

	FFrameRate   FrameRate    = FadeTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber FirstKeyTime = (MatineeFadeTrack->GetKeyframeTime( 0 ) * FrameRate).RoundToFrame();

	UMovieSceneFadeSection* Section = Cast<UMovieSceneFadeSection>( MovieSceneHelpers::FindSectionAtTime( FadeTrack->GetAllSections(), FirstKeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneFadeSection>( FadeTrack->CreateNewSection() );
		FadeTrack->AddSection( *Section );
		bSectionCreated = true;
	}
	if (Section->TryModify())
	{
		TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

		FMovieSceneFloatChannel* FadeChannel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
		check(FadeChannel);
		TMovieSceneChannelData<FMovieSceneFloatValue> FadeInterface = FadeChannel->GetData();
		for ( const auto& Point : MatineeFadeTrack->FloatTrack.Points )
		{
			FFrameNumber KeyTime = (Point.InVal * FrameRate).RoundToFrame();

			FMatineeImportTools::SetOrAddKey( FadeInterface, KeyTime, Point.OutVal, Point.ArriveTangent, Point.LeaveTangent, Point.InterpMode );

			KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
		}

		if (!KeyRange.IsEmpty())
		{
			Section->SetRange( KeyRange );
		}

		Section->FadeColor = MatineeFadeTrack->FadeColor;
		Section->bFadeAudio = MatineeFadeTrack->bFadeAudio;
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpDirectorTrack( UInterpTrackDirector* DirectorTrack, UMovieSceneCameraCutTrack* CameraCutTrack, AMatineeActor* MatineeActor, IMovieScenePlayer& Player )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeDirectorTrack", "Paste Matinee Director Track" ) );
	bool bCutsAdded = false;

	FFrameRate FrameRate = CameraCutTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();

	CameraCutTrack->Modify();
	
	for (FDirectorTrackCut TrackCut : DirectorTrack->CutTrack)
	{
		int32 GroupIndex = MatineeActor->MatineeData->FindGroupByName(TrackCut.TargetCamGroup);
		
		UInterpGroupInst* ViewGroupInst = (GroupIndex != INDEX_NONE) ? MatineeActor->FindFirstGroupInstByName( TrackCut.TargetCamGroup.ToString() ) : NULL;
		if ( GroupIndex != INDEX_NONE && ViewGroupInst )
		{
			// Find a valid move track for this cut.
			UInterpGroup* Group = MatineeActor->MatineeData->InterpGroups[GroupIndex];
			if (Group)
			{
				AActor* CameraActor = ViewGroupInst->GetGroupActor();
		
				FGuid CameraHandle = Player.FindObjectId(*CameraActor, MovieSceneSequenceID::Root);
				if (CameraHandle.IsValid())
				{
					FMovieSceneObjectBindingID CameraBindingID(CameraHandle, MovieSceneSequenceID::Root);
					CameraCutTrack->AddNewCameraCut(CameraBindingID, (TrackCut.Time * FrameRate).RoundToFrame());
					bCutsAdded = true;
				}
			}
		}
	}

	return bCutsAdded;
}

bool FMatineeImportTools::CopyInterpEventTrack( UInterpTrackEvent* MatineeEventTrack, UMovieSceneEventTrack* EventTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeEventTrack", "Paste Matinee Event Track" ) );
	bool bSectionCreated = false;

	EventTrack->Modify();

	if (MatineeEventTrack->EventTrack.Num())
	{
		FFrameRate   FrameRate    = EventTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber FirstKeyTime = (MatineeEventTrack->EventTrack[0].Time * FrameRate).RoundToFrame();

		UMovieSceneEventSection* Section = Cast<UMovieSceneEventSection>( MovieSceneHelpers::FindSectionAtTime( EventTrack->GetAllSections(), FirstKeyTime ) );
		if ( Section == nullptr )
		{
			Section = Cast<UMovieSceneEventSection>( EventTrack->CreateNewSection() );
			EventTrack->AddSection( *Section );
			bSectionCreated = true;
		}
		if (Section->TryModify())
		{
			TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

			FMovieSceneEventSectionData* EventChannel = Section->GetChannelProxy().GetChannel<FMovieSceneEventSectionData>(0);
			check(EventChannel);
			TMovieSceneChannelData<FEventPayload> ChannelData = EventChannel->GetData();

			for (FEventTrackKey EventTrackKey : MatineeEventTrack->EventTrack)
			{
				FFrameNumber KeyTime = (EventTrackKey.Time * FrameRate).RoundToFrame();

				ChannelData.UpdateOrAddKey(KeyTime, FEventPayload(EventTrackKey.EventName));

				KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
			}

			if (!KeyRange.IsEmpty())
			{
				Section->SetRange( KeyRange );
			}
		}
	}	
	
	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpVisibilityTrack( UInterpTrackVisibility* MatineeVisibilityTrack, UMovieSceneVisibilityTrack* VisibilityTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeVisibilityTrack", "Paste Matinee Visibility track" ) );
	bool bSectionCreated = false;

	VisibilityTrack->Modify();

	if (MatineeVisibilityTrack->VisibilityTrack.Num())
	{
		FFrameRate   FrameRate    = VisibilityTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber FirstKeyTime = (MatineeVisibilityTrack->GetKeyframeTime( 0 ) * FrameRate).RoundToFrame();

		UMovieSceneBoolSection* Section = Cast<UMovieSceneBoolSection>( MovieSceneHelpers::FindSectionAtTime( VisibilityTrack->GetAllSections(), FirstKeyTime ) );
		if ( Section == nullptr )
		{
			Section = Cast<UMovieSceneBoolSection>( VisibilityTrack->CreateNewSection() );
			VisibilityTrack->AddSection( *Section );
			bSectionCreated = true;
		}
		if (Section->TryModify())
		{
			TRange<FFrameNumber> KeyRange = TRange<FFrameNumber>::Empty();

			bool bVisible = true;

			FMovieSceneBoolChannel* VisibilityChannel = Section->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);
			check(VisibilityChannel);
			TMovieSceneChannelData<bool> ChannelData = VisibilityChannel->GetData();

			for (FVisibilityTrackKey VisibilityTrackKey : MatineeVisibilityTrack->VisibilityTrack)
			{
				if (VisibilityTrackKey.Action == EVisibilityTrackAction::EVTA_Hide)
				{
					bVisible = false;
				}
				else if (VisibilityTrackKey.Action == EVisibilityTrackAction::EVTA_Show)
				{
					bVisible = true;
				}
				else if (VisibilityTrackKey.Action == EVisibilityTrackAction::EVTA_Toggle)
				{
					bVisible = !bVisible;
				}

				FFrameNumber KeyTime = (VisibilityTrackKey.Time * FrameRate).RoundToFrame();

				ChannelData.UpdateOrAddKey(KeyTime, bVisible);

				KeyRange = TRange<FFrameNumber>::Hull(KeyRange, TRange<FFrameNumber>(KeyTime));
			}

			if (!KeyRange.IsEmpty())
			{
				Section->SetRange( KeyRange );
			}
		}
	}	
	
	return bSectionCreated;
}
