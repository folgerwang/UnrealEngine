// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UObject/Package.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "GenericPlatform/IInputInterface.h"
#include "IMagicLeapInputDevice.h"
#include "IHapticDevice.h"
#include "IMotionController.h"
#include "MagicLeapControllerKeys.h"
#include "MagicLeapControllerFunctionLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_input.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapControllerTest, Display, All);

//function to force the linker to include this cpp
void MagicLeapTestReferenceFunction()
{

}

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_LUMIN

/**
* Play Pattern Haptic Effect
*/
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FPlayPatternHapticLatentCommand, EControllerHand, Hand, EMLControllerHapticPattern, Pattern, EMLControllerHapticIntensity, Intensity);
bool FPlayPatternHapticLatentCommand::Update()
{
	FString PatternName;
	UEnum* PatternEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMLControllerHapticPattern"), true);
	if (PatternEnum != nullptr)
	{
		PatternName = PatternEnum->GetNameByValue((int32)Pattern).ToString();
	}

	UE_LOG(LogCore, Log, TEXT("FPlayPatternHapticLatentCommand %d, %s"), (int32)Hand, *PatternName);

	UMagicLeapControllerFunctionLibrary::PlayControllerHapticFeedback(Hand, Pattern, Intensity);

	return true;
}


//@TODO - remove editor
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMagicLeapControllerHapticTest, "System.VR.MagicLeap.Haptics.Pattern", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMagicLeapControllerHapticTest::RunTest(const FString&)
{
	UEnum* PatternEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMLControllerHapticPattern"), true);
	UEnum* IntensityEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMLControllerHapticIntensity"), true);

	float ActiveDuration = 1.0f;
	//for (uint32 HandIndex = 0; HandIndex < MLInput_MaxControllers; ++HandIndex)
	//for now only right hand matters
	uint32 HandIndex = (uint32)EControllerHand::Right;
	{
		for (uint32 PatternIndex = 0; PatternIndex < PatternEnum->GetMaxEnumValue(); ++PatternIndex)
		{
			for (uint32 IntensityIndex = 0; IntensityIndex < IntensityEnum->GetMaxEnumValue(); ++IntensityIndex)
			{
				//Turn on haptics
				ADD_LATENT_AUTOMATION_COMMAND(FPlayPatternHapticLatentCommand(
					(EControllerHand)HandIndex,
					(EMLControllerHapticPattern)PatternIndex,
					(EMLControllerHapticIntensity)IntensityIndex));
				//Give the command a chance to finish
				ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(ActiveDuration));
			}
		}
	}

	return true;
}


/**
* Play LED Effect
*/
DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(FPlayLEDPatternLatentCommand, EControllerHand, Hand, EMLControllerLEDPattern, Pattern, EMLControllerLEDColor, Color, float, Duration);
bool FPlayLEDPatternLatentCommand::Update()
{
	FString PatternName;
	UEnum* PatternEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMLControllerLEDPattern"), true);
	if (PatternEnum != nullptr)
	{
		PatternName = PatternEnum->GetNameByValue((int32)Pattern).ToString();
	}

	FString ColorName;
	UEnum* ColorEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMLControllerLEDColor"), true);
	if (ColorEnum != nullptr)
	{
		ColorName = ColorEnum->GetNameByValue((int32)Color).ToString();
	}

	UE_LOG(LogCore, Log, TEXT("FPlayLEDPatternLatentCommand %d Hand, %s %s"), (int32)Hand, *PatternName, *ColorName);

	UMagicLeapControllerFunctionLibrary::PlayControllerLED(Hand, Pattern, Color, Duration);

	return true;
}


//@TODO - remove editor
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMagicLeapControllerLEDTest, "System.VR.MagicLeap.LED", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMagicLeapControllerLEDTest::RunTest(const FString&)
{
	UEnum* PatternEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMLControllerLEDPattern"), true);
	UEnum* ColorEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMLControllerLEDColor"), true);

	float ActiveDuration = 1.0f;
	float InactiveDuration = 0.5f;
	//run through each pattern and run the pattern, wait for it to finish, turn off, wait
	//for (uint32 HandIndex = 0; HandIndex < MLInput_MaxControllers; ++HandIndex)
	//for now only right hand matters
	uint32 HandIndex = (uint32)EControllerHand::Right;
	{
		for (uint32 PatternIndex = 0; PatternIndex < PatternEnum->GetMaxEnumValue(); ++PatternIndex)
		{
			for (uint32 ColorIndex = 0; ColorIndex < ColorEnum->GetMaxEnumValue(); ++ColorIndex)
			{
				//Turn LED pattern on
				ADD_LATENT_AUTOMATION_COMMAND(FPlayLEDPatternLatentCommand((EControllerHand)HandIndex, (EMLControllerLEDPattern)PatternIndex, (EMLControllerLEDColor)ColorIndex, ActiveDuration));
				//Give the command a chance to finish
				ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(ActiveDuration));
				//Add a second to delimit between patterns
				ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(InactiveDuration));
			}
		}
	}

	return true;
}

#endif