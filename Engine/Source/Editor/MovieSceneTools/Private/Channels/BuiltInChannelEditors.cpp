// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BuiltInChannelEditors.h"
#include "ISequencerChannelInterface.h"
#include "Widgets/SNullWidget.h"
#include "ISequencer.h"
#include "MovieSceneCommonHelpers.h"
#include "GameFramework/Actor.h"
#include "EditorStyleSet.h"
#include "CurveKeyEditors/SNumericKeyEditor.h"
#include "CurveKeyEditors/SBoolCurveKeyEditor.h"
#include "CurveKeyEditors/SStringCurveKeyEditor.h"
#include "CurveKeyEditors/SEnumKeyEditor.h"
#include "UObject/StructOnScope.h"
#include "KeyDrawParams.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneChannelEditorData.h"
#include "FloatChannelCurveModel.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "BuiltInChannelEditors"


FKeyHandle AddOrUpdateKey(FMovieSceneFloatChannel* Channel, UMovieSceneSection* SectionToKey, const TMovieSceneExternalValue<float>& ExternalValue, FFrameNumber InTime, ISequencer& Sequencer, const FGuid& InObjectBindingID, FTrackInstancePropertyBindings* PropertyBindings)
{
	TOptional<float> Value;
	float CurrentValue = 0.0f, CurrentWeight = 1.0f;
	if ((ExternalValue.OnGetExternalValue && InObjectBindingID.IsValid()))
	{
		for (TWeakObjectPtr<> WeakObject : Sequencer.FindBoundObjects(InObjectBindingID, Sequencer.GetFocusedTemplateID()))
		{
			if (UObject* Object = WeakObject.Get())
			{
				Value = ExternalValue.OnGetExternalValue(*Object, PropertyBindings);
				if (Value.IsSet() && ExternalValue.OnGetCurrentValueAndWeight && SectionToKey)
				{
					CurrentValue = Value.Get(0.0f);
					ExternalValue.OnGetCurrentValueAndWeight(Object, SectionToKey, InTime, Sequencer.GetFocusedTickResolution(), Sequencer.GetEvaluationTemplate(),CurrentValue,CurrentWeight);
				}
				break;
			}
		}
	}

	float NewValue = Channel->GetDefault().Get(0.f);
	bool bWasEvaluated = Channel->Evaluate(InTime, NewValue);
	if (Value.IsSet()) //need to get the diff between Value(Global) and CurrentValue and apply that to the local
	{
		if (bWasEvaluated)
		{
			float CurrentGlobalValue = Value.GetValue();
			NewValue = (Value.Get(0.0f) - CurrentValue) * CurrentWeight + NewValue;
		}
		else //Nothing set (key or default) on channel so use external value
		{
			NewValue = Value.Get(0.0f);
		}
	}

	using namespace MovieScene;
	return AddKeyToChannel(Channel, InTime, NewValue, Sequencer.GetKeyInterpolation());
}

FKeyHandle AddOrUpdateKey(FMovieSceneActorReferenceData* Channel, UMovieSceneSection* SectionToKey, FFrameNumber InTime, ISequencer& Sequencer, const FGuid& InObjectBindingID, FTrackInstancePropertyBindings* PropertyBindings)
{
	AActor* CurrentActor = nullptr;

	if (PropertyBindings && InObjectBindingID.IsValid())
	{
		for (TWeakObjectPtr<> WeakObject : Sequencer.FindBoundObjects(InObjectBindingID, Sequencer.GetFocusedTemplateID()))
		{
			if (UObject* Object = WeakObject.Get())
			{
				CurrentActor = PropertyBindings->GetCurrentValue<AActor*>(*Object);
				break;
			}
		}
	}

	FGuid ThisGuid = CurrentActor ? Sequencer.FindObjectId(*CurrentActor, Sequencer.GetFocusedTemplateID()) : FGuid();

	FMovieSceneObjectBindingID NewValue(ThisGuid, MovieSceneSequenceID::Root, EMovieSceneObjectBindingSpace::Local);
	int32 NewIndex = Channel->GetData().AddKey(InTime, NewValue);
	return Channel->GetData().GetHandle(NewIndex);
}

bool CanCreateKeyEditor(const FMovieSceneBoolChannel*    Channel)
{
	return true;
}
bool CanCreateKeyEditor(const FMovieSceneByteChannel*    Channel)
{
	return true;
}
bool CanCreateKeyEditor(const FMovieSceneIntegerChannel* Channel)
{
	return true;
}
bool CanCreateKeyEditor(const FMovieSceneFloatChannel*   Channel)
{
	return true;
}
bool CanCreateKeyEditor(const FMovieSceneStringChannel*  Channel)
{
	return true;
}
bool CanCreateKeyEditor(const FMovieSceneObjectPathChannel* Channel)
{
	return true;
}

TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneBoolChannel>&    Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer)
{
	const TMovieSceneExternalValue<bool>* ExternalValue = Channel.GetExtendedEditorData();
	if (!ExternalValue)
	{
		return SNullWidget::NullWidget;
	}

	TSequencerKeyEditor<FMovieSceneBoolChannel, bool> KeyEditor(
		InObjectBindingID, Channel,
		Section, InSequencer, PropertyBindings, ExternalValue->OnGetExternalValue
		);

	return SNew(SBoolCurveKeyEditor, KeyEditor);
}


TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneIntegerChannel>& Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer)
{
	const TMovieSceneExternalValue<int32>* ExternalValue = Channel.GetExtendedEditorData();
	if (!ExternalValue)
	{
		return SNullWidget::NullWidget;
	}

	TSequencerKeyEditor<FMovieSceneIntegerChannel, int32> KeyEditor(
		InObjectBindingID, Channel,
		Section, InSequencer, PropertyBindings, ExternalValue->OnGetExternalValue
		);

	typedef SNumericKeyEditor<FMovieSceneIntegerChannel, int32> KeyEditorType;
	return SNew(KeyEditorType, KeyEditor);
}


TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneFloatChannel>&   Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer)
{
	const TMovieSceneExternalValue<float>* ExternalValue = Channel.GetExtendedEditorData();
	if (!ExternalValue)
	{
		return SNullWidget::NullWidget;
	}

	TSequencerKeyEditor<FMovieSceneFloatChannel, float> KeyEditor(
		InObjectBindingID, Channel,
		Section, InSequencer, PropertyBindings, ExternalValue->OnGetExternalValue
		);

	typedef SNumericKeyEditor<FMovieSceneFloatChannel, float> KeyEditorType;
	return SNew(KeyEditorType, KeyEditor);
}


TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneStringChannel>&  Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer)
{
	const TMovieSceneExternalValue<FString>* ExternalValue = Channel.GetExtendedEditorData();
	if (!ExternalValue)
	{
		return SNullWidget::NullWidget;
	}

	TSequencerKeyEditor<FMovieSceneStringChannel, FString> KeyEditor(
		InObjectBindingID, Channel,
		Section, InSequencer, PropertyBindings, ExternalValue->OnGetExternalValue
		);

	return SNew(SStringCurveKeyEditor, KeyEditor);
}


TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneByteChannel>&    Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer)
{
	const TMovieSceneExternalValue<uint8>* ExternalValue = Channel.GetExtendedEditorData();
	const FMovieSceneByteChannel* RawChannel = Channel.Get();
	if (!ExternalValue || !RawChannel)
	{
		return SNullWidget::NullWidget;
	}

	TSequencerKeyEditor<FMovieSceneByteChannel, uint8> KeyEditor(
		InObjectBindingID, Channel,
		Section, InSequencer, PropertyBindings, ExternalValue->OnGetExternalValue
		);

	if (UEnum* Enum = RawChannel->GetEnum())
	{
		return SNew(SEnumCurveKeyEditor, KeyEditor, Enum);
	}
	else
	{
		typedef SNumericKeyEditor<FMovieSceneByteChannel, uint8> KeyEditorType;
		return SNew(KeyEditorType, KeyEditor);
	}
}

TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneObjectPathChannel>& Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer)
{
	const TMovieSceneExternalValue<UObject*>* ExternalValue = Channel.GetExtendedEditorData();
	const FMovieSceneObjectPathChannel*       RawChannel    = Channel.Get();
	if (ExternalValue && RawChannel)
	{
		TSequencerKeyEditor<FMovieSceneObjectPathChannel, UObject*> KeyEditor(InObjectBindingID, Channel, Section, InSequencer, PropertyBindings, ExternalValue->OnGetExternalValue);

		auto OnSetObjectLambda = [KeyEditor](const FAssetData& Asset) mutable
		{
			FScopedTransaction Transaction(LOCTEXT("SetKey", "Set Enum Key Value"));
			KeyEditor.SetValueWithNotify(Asset.GetAsset(), EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
		};

		auto GetObjectPathLambda = [KeyEditor]() -> FString
		{
			UObject* Obj = KeyEditor.GetCurrentValue();
			return Obj ? Obj->GetPathName() : FString();
		};

		return SNew(SObjectPropertyEntryBox)
		.DisplayBrowse(false)
		.DisplayUseSelected(false)
		.ObjectPath_Lambda(GetObjectPathLambda)
		.AllowedClass(RawChannel->GetPropertyClass())
		.OnObjectChanged_Lambda(OnSetObjectLambda);
	}

	return SNullWidget::NullWidget;
}

UMovieSceneKeyStructType* InstanceGeneratedStruct(FMovieSceneByteChannel* Channel, FSequencerKeyStructGenerator* Generator)
{
	UEnum* ByteEnum = Channel->GetEnum();
	if (!ByteEnum)
	{
		// No enum so just use the default (which will create a generated struct with a byte property)
		return Generator->DefaultInstanceGeneratedStruct(FMovieSceneByteChannel::StaticStruct());
	}

	FName GeneratedTypeName = *FString::Printf(TEXT("MovieSceneByteChannel_%s"), *ByteEnum->GetName());

	UMovieSceneKeyStructType* Existing = Generator->FindGeneratedStruct(GeneratedTypeName);
	if (Existing)
	{
		return Existing;
	}

	UMovieSceneKeyStructType* NewStruct = FSequencerKeyStructGenerator::AllocateNewKeyStruct(FMovieSceneByteChannel::StaticStruct());
	if (!NewStruct)
	{
		return nullptr;
	}

	UByteProperty* NewValueProperty = NewObject<UByteProperty>(NewStruct, "Value");
	NewValueProperty->SetPropertyFlags(CPF_Edit);
	NewValueProperty->SetMetaData("Category", TEXT("Key"));
	NewValueProperty->ArrayDim = 1;
	NewValueProperty->Enum = ByteEnum;

	NewStruct->AddCppProperty(NewValueProperty);
	NewStruct->DestValueProperty = NewValueProperty;

	FSequencerKeyStructGenerator::FinalizeNewKeyStruct(NewStruct);

	Generator->AddGeneratedStruct(GeneratedTypeName, NewStruct);
	return NewStruct;
}

UMovieSceneKeyStructType* InstanceGeneratedStruct(FMovieSceneObjectPathChannel* Channel, FSequencerKeyStructGenerator* Generator)
{
	UClass* PropertyClass = Channel->GetPropertyClass();
	if (!PropertyClass)
	{
		// No specific property class so just use the default (which will create a generated struct with an object property)
		return Generator->DefaultInstanceGeneratedStruct(FMovieSceneObjectPathChannel::StaticStruct());
	}

	FName GeneratedTypeName = *FString::Printf(TEXT("MovieSceneObjectPathChannel_%s"), *PropertyClass->GetName());

	UMovieSceneKeyStructType* Existing = Generator->FindGeneratedStruct(GeneratedTypeName);
	if (Existing)
	{
		return Existing;
	}

	UMovieSceneKeyStructType* NewStruct = FSequencerKeyStructGenerator::AllocateNewKeyStruct(FMovieSceneObjectPathChannel::StaticStruct());
	if (!NewStruct)
	{
		return nullptr;
	}

	USoftObjectProperty* NewValueProperty = NewObject<USoftObjectProperty>(NewStruct, "Value");
	NewValueProperty->SetPropertyFlags(CPF_Edit);
	NewValueProperty->SetMetaData("Category", TEXT("Key"));
	NewValueProperty->PropertyClass = PropertyClass;
	NewValueProperty->ArrayDim = 1;

	NewStruct->AddCppProperty(NewValueProperty);
	NewStruct->DestValueProperty = NewValueProperty;

	FSequencerKeyStructGenerator::FinalizeNewKeyStruct(NewStruct);

	Generator->AddGeneratedStruct(GeneratedTypeName, NewStruct);
	return NewStruct;
}

void DrawKeys(FMovieSceneFloatChannel* Channel, TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams)
{
	static const FName CircleKeyBrushName("Sequencer.KeyCircle");
	static const FName DiamondKeyBrushName("Sequencer.KeyDiamond");
	static const FName SquareKeyBrushName("Sequencer.KeySquare");
	static const FName TriangleKeyBrushName("Sequencer.KeyTriangle");

	const FSlateBrush* CircleKeyBrush = FEditorStyle::GetBrush(CircleKeyBrushName);
	const FSlateBrush* DiamondKeyBrush = FEditorStyle::GetBrush(DiamondKeyBrushName);
	const FSlateBrush* SquareKeyBrush = FEditorStyle::GetBrush(SquareKeyBrushName);
	const FSlateBrush* TriangleKeyBrush = FEditorStyle::GetBrush(TriangleKeyBrushName);

	TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = Channel->GetData();
	TArrayView<const FMovieSceneFloatValue> Values = ChannelData.GetValues();

	FKeyDrawParams TempParams;
	TempParams.BorderBrush = TempParams.FillBrush = DiamondKeyBrush;

	for (int32 Index = 0; Index < InKeyHandles.Num(); ++Index)
	{
		FKeyHandle Handle = InKeyHandles[Index];

		const int32 KeyIndex = ChannelData.GetIndex(Handle);

		ERichCurveInterpMode InterpMode   = KeyIndex == INDEX_NONE ? RCIM_None : Values[KeyIndex].InterpMode.GetValue();
		ERichCurveTangentMode TangentMode = KeyIndex == INDEX_NONE ? RCTM_None : Values[KeyIndex].TangentMode.GetValue();

		TempParams.FillOffset = FVector2D(0.f, 0.f);

		switch (InterpMode)
		{
		case RCIM_Linear:
			TempParams.BorderBrush = TempParams.FillBrush = TriangleKeyBrush;
			TempParams.FillTint = FLinearColor(0.0f, 0.617f, 0.449f, 1.0f); // blueish green
			TempParams.FillOffset = FVector2D(0.0f, 1.0f);
			break;

		case RCIM_Constant:
			TempParams.BorderBrush = TempParams.FillBrush = SquareKeyBrush;
			TempParams.FillTint = FLinearColor(0.0f, 0.445f, 0.695f, 1.0f); // blue
			break;

		case RCIM_Cubic:
			TempParams.BorderBrush = TempParams.FillBrush = CircleKeyBrush;

			switch (TangentMode)
			{
			case RCTM_Auto:  TempParams.FillTint = FLinearColor(0.972f, 0.2f, 0.2f, 1.0f);     break; // vermillion
			case RCTM_Break: TempParams.FillTint = FLinearColor(0.336f, 0.703f, 0.5f, 0.91f);  break; // sky blue
			case RCTM_User:  TempParams.FillTint = FLinearColor(0.797f, 0.473f, 0.5f, 0.652f); break; // reddish purple
			default:         TempParams.FillTint = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);    break; // light gray
			}
			break;

		default:
			TempParams.BorderBrush = TempParams.FillBrush = DiamondKeyBrush;
			TempParams.FillTint   = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // white
			break;
		}

		OutKeyDrawParams[Index] = TempParams;
	}
}

void DrawKeys(FMovieSceneParticleChannel* Channel, TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams)
{
	static const FName KeyLeftBrushName("Sequencer.KeyLeft");
	static const FName KeyRightBrushName("Sequencer.KeyRight");
	static const FName KeyDiamondBrushName("Sequencer.KeyDiamond");

	const FSlateBrush* LeftKeyBrush = FEditorStyle::GetBrush(KeyLeftBrushName);
	const FSlateBrush* RightKeyBrush = FEditorStyle::GetBrush(KeyRightBrushName);
	const FSlateBrush* DiamondBrush = FEditorStyle::GetBrush(KeyDiamondBrushName);

	TMovieSceneChannelData<uint8> ChannelData = Channel->GetData();

	for (int32 Index = 0; Index < InKeyHandles.Num(); ++Index)
	{
		FKeyHandle Handle = InKeyHandles[Index];

		FKeyDrawParams Params;
		Params.BorderBrush = Params.FillBrush = DiamondBrush;

		const int32 KeyIndex = ChannelData.GetIndex(Handle);
		if ( KeyIndex != INDEX_NONE )
		{
			const EParticleKey Value = (EParticleKey)ChannelData.GetValues()[KeyIndex];
			if ( Value == EParticleKey::Activate )
			{
				Params.BorderBrush = Params.FillBrush = LeftKeyBrush;
				Params.FillOffset = FVector2D(-1.0f, 1.0f);
			}
			else if ( Value == EParticleKey::Deactivate )
			{
				Params.BorderBrush = Params.FillBrush = RightKeyBrush;
				Params.FillOffset = FVector2D(1.0f, 1.0f);
			}
		}

		OutKeyDrawParams[Index] = Params;
	}
}

void DrawKeys(FMovieSceneEventChannel* Channel, TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams)
{
	FKeyDrawParams ValidEventParams, InvalidEventParams;

	ValidEventParams.BorderBrush   = ValidEventParams.FillBrush   = FEditorStyle::Get().GetBrush("Sequencer.KeyDiamond");

	InvalidEventParams.FillBrush   = FEditorStyle::Get().GetBrush("Sequencer.KeyDiamond");
	InvalidEventParams.BorderBrush = FEditorStyle::Get().GetBrush("Sequencer.KeyDiamondBorder");
	InvalidEventParams.FillTint    = FLinearColor(1.f,1.f,1.f,.2f);

	TMovieSceneChannelData<FMovieSceneEvent> ChannelData = Channel->GetData();
	TArrayView<const FMovieSceneEvent>       Events = ChannelData.GetValues();

	for (int32 Index = 0; Index < InKeyHandles.Num(); ++Index)
	{
		int32 KeyIndex = ChannelData.GetIndex(InKeyHandles[Index]);
		if (KeyIndex != INDEX_NONE && Events[KeyIndex].IsBoundToBlueprint())
		{
			OutKeyDrawParams[Index] = ValidEventParams;
		}
		else
		{
			OutKeyDrawParams[Index] = InvalidEventParams;
		}
	}
}

struct FFloatChannelKeyMenuExtension : FExtender, TSharedFromThis<FFloatChannelKeyMenuExtension>
{
	FFloatChannelKeyMenuExtension(TWeakPtr<ISequencer> InSequencer, TArray<TExtendKeyMenuParams<FMovieSceneFloatChannel>>&& InChannels)
		: WeakSequencer(InSequencer)
		, ChannelAndHandles(MoveTemp(InChannels))
	{}

	void ExtendMenu(FMenuBuilder& MenuBuilder)
	{
		ISequencer* SequencerPtr = WeakSequencer.Pin().Get();
		if (!SequencerPtr)
		{
			return;
		}

		TSharedRef<FFloatChannelKeyMenuExtension> SharedThis = AsShared();

		MenuBuilder.BeginSection("SequencerInterpolation", LOCTEXT("KeyInterpolationMenu", "Key Interpolation"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SetKeyInterpolationAuto", "Cubic (Auto)"),
				LOCTEXT("SetKeyInterpolationAutoTooltip", "Set key interpolation to auto"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.IconKeyAuto"),
				FUIAction(
					FExecuteAction::CreateLambda([SharedThis]{ SharedThis->SetInterpTangentMode(RCIM_Cubic, RCTM_Auto); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([SharedThis]{ return SharedThis->IsInterpTangentModeSelected(RCIM_Cubic, RCTM_Auto); }) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("SetKeyInterpolationUser", "Cubic (User)"),
				LOCTEXT("SetKeyInterpolationUserTooltip", "Set key interpolation to user"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.IconKeyUser"),
				FUIAction(
					FExecuteAction::CreateLambda([SharedThis]{ SharedThis->SetInterpTangentMode(RCIM_Cubic, RCTM_User); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([SharedThis]{ return SharedThis->IsInterpTangentModeSelected(RCIM_Cubic, RCTM_User); }) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("SetKeyInterpolationBreak", "Cubic (Break)"),
				LOCTEXT("SetKeyInterpolationBreakTooltip", "Set key interpolation to break"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.IconKeyBreak"),
				FUIAction(
					FExecuteAction::CreateLambda([SharedThis]{ SharedThis->SetInterpTangentMode(RCIM_Cubic, RCTM_Break); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([SharedThis]{ return SharedThis->IsInterpTangentModeSelected(RCIM_Cubic, RCTM_Break); }) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("SetKeyInterpolationLinear", "Linear"),
				LOCTEXT("SetKeyInterpolationLinearTooltip", "Set key interpolation to linear"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.IconKeyLinear"),
				FUIAction(
					FExecuteAction::CreateLambda([SharedThis]{ SharedThis->SetInterpTangentMode(RCIM_Linear, RCTM_Auto); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([SharedThis]{ return SharedThis->IsInterpTangentModeSelected(RCIM_Linear, RCTM_Auto); }) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("SetKeyInterpolationConstant", "Constant"),
				LOCTEXT("SetKeyInterpolationConstantTooltip", "Set key interpolation to constant"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.IconKeyConstant"),
				FUIAction(
					FExecuteAction::CreateLambda([SharedThis]{ SharedThis->SetInterpTangentMode(RCIM_Constant, RCTM_Auto); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([SharedThis]{ return SharedThis->IsInterpTangentModeSelected(RCIM_Constant, RCTM_Auto); }) ),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
		MenuBuilder.EndSection(); // SequencerInterpolation
	}

	void SetInterpTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode)
	{
		FScopedTransaction SetInterpTangentModeTransaction(NSLOCTEXT("Sequencer", "SetInterpTangentMode_Transaction", "Set Interpolation and Tangent Mode"));
		bool bAnythingChanged = false;

		for (const TExtendKeyMenuParams<FMovieSceneFloatChannel>& Channel : ChannelAndHandles)
		{
			UMovieSceneSection* Section = Channel.Section.Get();
			FMovieSceneFloatChannel* ChannelPtr = Channel.Channel.Get();

			if (Section && ChannelPtr)
			{
				Section->Modify();

				TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = ChannelPtr->GetData();
				TArrayView<FMovieSceneFloatValue> Values = ChannelData.GetValues();

				for (FKeyHandle Handle : Channel.Handles)
				{
					const int32 KeyIndex = ChannelData.GetIndex(Handle);
					if (KeyIndex != INDEX_NONE)
					{
						Values[KeyIndex].InterpMode = InterpMode;
						Values[KeyIndex].TangentMode = TangentMode;
						bAnythingChanged = true;
					}
				}

				ChannelPtr->AutoSetTangents();
			}
		}

		if (bAnythingChanged)
		{
			if (ISequencer* Sequencer = WeakSequencer.Pin().Get())
			{
				Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
			}
		}
	}

	bool IsInterpTangentModeSelected(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const
	{
		for (const TExtendKeyMenuParams<FMovieSceneFloatChannel>& Channel : ChannelAndHandles)
		{
			FMovieSceneFloatChannel* ChannelPtr = Channel.Channel.Get();
			if (ChannelPtr)
			{
				TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = ChannelPtr->GetData();
				TArrayView<FMovieSceneFloatValue> Values = ChannelData.GetValues();

				for (FKeyHandle Handle : Channel.Handles)
				{
					int32 KeyIndex = ChannelData.GetIndex(Handle);
					if (KeyIndex == INDEX_NONE || Values[KeyIndex].InterpMode != InterpMode || Values[KeyIndex].TangentMode != TangentMode)
					{
						return false;
					}
				}
			}
		}
		return true;
	}

private:

	/** Hidden AsShared() methods to prevent CreateSP delegate use since this extender disappears with its menu. */
	using TSharedFromThis::AsShared;

	TWeakPtr<ISequencer> WeakSequencer;
	TArray<TExtendKeyMenuParams<FMovieSceneFloatChannel>> ChannelAndHandles;
};


struct FFloatChannelSectionMenuExtension : FExtender, TSharedFromThis<FFloatChannelSectionMenuExtension>
{
	FFloatChannelSectionMenuExtension(TWeakPtr<ISequencer> InSequencer, TArray<TMovieSceneChannelHandle<FMovieSceneFloatChannel>>&& InChannels, TArrayView<UMovieSceneSection* const> InSections)
		: WeakSequencer(InSequencer)
		, Channels(MoveTemp(InChannels))
	{
		Sections.Reserve(InSections.Num());
		for (UMovieSceneSection* Section : InSections)
		{
			Sections.Add(Section);
		}
	}

	void ExtendMenu(FMenuBuilder& MenuBuilder)
	{
		ISequencer* SequencerPtr = WeakSequencer.Pin().Get();
		if (!SequencerPtr)
		{
			return;
		}

		TSharedRef<FFloatChannelSectionMenuExtension> SharedThis = AsShared();

		MenuBuilder.AddSubMenu(
			LOCTEXT("SetPreInfinityExtrap", "Pre-Infinity"),
			LOCTEXT("SetPreInfinityExtrapTooltip", "Set pre-infinity extrapolation"),
			FNewMenuDelegate::CreateLambda([SharedThis](FMenuBuilder& SubMenuBuilder){ SharedThis->AddExtrapolationMenu(SubMenuBuilder, true); })
			);

		MenuBuilder.AddSubMenu(
			LOCTEXT("SetPostInfinityExtrap", "Post-Infinity"),
			LOCTEXT("SetPostInfinityExtrapTooltip", "Set post-infinity extrapolation"),
			FNewMenuDelegate::CreateLambda([SharedThis](FMenuBuilder& SubMenuBuilder){ SharedThis->AddExtrapolationMenu(SubMenuBuilder, false); })
			);
	}

	void AddExtrapolationMenu(FMenuBuilder& MenuBuilder, bool bPreInfinity)
	{
		TSharedRef<FFloatChannelSectionMenuExtension> SharedThis = AsShared();

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetExtrapCycle", "Cycle"),
			LOCTEXT("SetExtrapCycleTooltip", "Set extrapolation cycle"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SharedThis, bPreInfinity]{ SharedThis->SetExtrapolationMode(RCCE_Cycle, bPreInfinity); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([SharedThis, bPreInfinity]{ return SharedThis->IsExtrapolationModeSelected(RCCE_Cycle, bPreInfinity); })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetExtrapCycleWithOffset", "Cycle with Offset"),
			LOCTEXT("SetExtrapCycleWithOffsetTooltip", "Set extrapolation cycle with offset"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SharedThis, bPreInfinity]{ SharedThis->SetExtrapolationMode(RCCE_CycleWithOffset, bPreInfinity); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([SharedThis, bPreInfinity]{ return SharedThis->IsExtrapolationModeSelected(RCCE_CycleWithOffset, bPreInfinity); })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetExtrapOscillate", "Oscillate"),
			LOCTEXT("SetExtrapOscillateTooltip", "Set extrapolation oscillate"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SharedThis, bPreInfinity]{ SharedThis->SetExtrapolationMode(RCCE_Oscillate, bPreInfinity); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([SharedThis, bPreInfinity]{ return SharedThis->IsExtrapolationModeSelected(RCCE_Oscillate, bPreInfinity); })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetExtrapLinear", "Linear"),
			LOCTEXT("SetExtrapLinearTooltip", "Set extrapolation linear"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SharedThis, bPreInfinity]{ SharedThis->SetExtrapolationMode(RCCE_Linear, bPreInfinity); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([SharedThis, bPreInfinity]{ return SharedThis->IsExtrapolationModeSelected(RCCE_Linear, bPreInfinity); })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetExtrapConstant", "Constant"),
			LOCTEXT("SetExtrapConstantTooltip", "Set extrapolation constant"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SharedThis, bPreInfinity]{ SharedThis->SetExtrapolationMode(RCCE_Constant, bPreInfinity); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([SharedThis, bPreInfinity]{ return SharedThis->IsExtrapolationModeSelected(RCCE_Constant, bPreInfinity); })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}

	void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity)
	{
		FScopedTransaction Transaction(LOCTEXT("SetExtrapolationMode_Transaction", "Set Extrapolation Mode"));

		bool bAnythingChanged = false;

		// Modify all sections
		for (TWeakObjectPtr<UMovieSceneSection> WeakSection : Sections)
		{
			if (UMovieSceneSection* Section = WeakSection.Get())
			{
				Section->Modify();
			}
		}

		// Apply to all channels
		for (const TMovieSceneChannelHandle<FMovieSceneFloatChannel>& Handle : Channels)
		{
			FMovieSceneFloatChannel* Channel = Handle.Get();

			if (Channel)
			{
				TEnumAsByte<ERichCurveExtrapolation>& DestExtrap = bPreInfinity ? Channel->PreInfinityExtrap : Channel->PostInfinityExtrap;
				DestExtrap = ExtrapMode;
				bAnythingChanged = true;
			}
		}

		if (bAnythingChanged)
		{
			if (ISequencer* Sequencer = WeakSequencer.Pin().Get())
			{
				Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
			}
		}
		else
		{
			Transaction.Cancel();
		}
	}


	bool IsExtrapolationModeSelected(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) const
	{
		for (const TMovieSceneChannelHandle<FMovieSceneFloatChannel>& Handle : Channels)
		{
			FMovieSceneFloatChannel* Channel = Handle.Get();

			if (Channel)
			{
				ERichCurveExtrapolation SourceExtrap = bPreInfinity ? Channel->PreInfinityExtrap : Channel->PostInfinityExtrap;
				if (SourceExtrap != ExtrapMode)
				{
					return false;
				}
			}
		}

		return true;
	}

private:

	/** Hidden AsShared() methods to prevent CreateSP delegate use since this extender disappears with its menu. */
	using TSharedFromThis::AsShared;

	TWeakPtr<ISequencer> WeakSequencer;
	TArray<TMovieSceneChannelHandle<FMovieSceneFloatChannel>> Channels;
	TArray<TWeakObjectPtr<UMovieSceneSection>> Sections;
};

void ExtendSectionMenu(FMenuBuilder& OuterMenuBuilder, TArray<TMovieSceneChannelHandle<FMovieSceneFloatChannel>>&& Channels, TArrayView<UMovieSceneSection* const> Sections, TWeakPtr<ISequencer> InSequencer)
{
	TSharedRef<FFloatChannelSectionMenuExtension> Extension = MakeShared<FFloatChannelSectionMenuExtension>(InSequencer, MoveTemp(Channels), Sections);

	Extension->AddMenuExtension("SequencerSections", EExtensionHook::First, nullptr, FMenuExtensionDelegate::CreateLambda([Extension](FMenuBuilder& MenuBuilder) { Extension->ExtendMenu(MenuBuilder); }));

	OuterMenuBuilder.PushExtender(Extension);
}

void ExtendKeyMenu(FMenuBuilder& OuterMenuBuilder, TArray<TExtendKeyMenuParams<FMovieSceneFloatChannel>>&& Channels, TWeakPtr<ISequencer> InSequencer)
{
	TSharedRef<FFloatChannelKeyMenuExtension> Extension = MakeShared<FFloatChannelKeyMenuExtension>(InSequencer, MoveTemp(Channels));

	Extension->AddMenuExtension("SequencerKeyEdit", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda([Extension](FMenuBuilder& MenuBuilder) { Extension->ExtendMenu(MenuBuilder); }));

	OuterMenuBuilder.PushExtender(Extension);
}

TUniquePtr<FCurveModel> CreateCurveEditorModel(const TMovieSceneChannelHandle<FMovieSceneFloatChannel>& FloatChannel, UMovieSceneSection* OwningSection, TSharedRef<ISequencer> InSequencer)
{
	return MakeUnique<FFloatChannelCurveModel>(FloatChannel, OwningSection, InSequencer);
}

#undef LOCTEXT_NAMESPACE
