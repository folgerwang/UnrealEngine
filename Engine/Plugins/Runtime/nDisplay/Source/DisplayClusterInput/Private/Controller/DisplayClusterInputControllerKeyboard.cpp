// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterInputControllerKeyboard.h"
#include "IDisplayClusterInputModule.h"

#include "IDisplayCluster.h"
#include "DisplayClusterInputStrings.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "Input/IDisplayClusterInputManager.h"
#include "Misc/DisplayClusterInputHelpers.h"
#include "Misc/DisplayClusterInputLog.h"

#include "IDisplayClusterInputModule.h"

#define LOCTEXT_NAMESPACE "DisplayClusterInput"




void FKeyboardController::Initialize()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputKeyboard);

	static const FName nDisplayKeyboardCategoryName(TEXT("nDisplayKeyboard"));
	EKeys::AddMenuCategoryDisplayInfo(nDisplayKeyboardCategoryName, LOCTEXT("nDisplayKeyboardSubCateogry", "nDisplayKeyboard"), TEXT("GraphEditor.KeyEvent_16x"));

	const int32 DefFlags = 0;
#define NDISLAY_KEYBOARD_CATEGORY(FLAGS) FLAGS,nDisplayKeyboardCategoryName

	EKeys::AddKey(FKeyDetails(FKeyboardKey::Tab,   LOCTEXT("nDisplayTab", "nDisplay Tab"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Enter, LOCTEXT("nDisplayEnter", "nDisplay Enter"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Pause, LOCTEXT("nDisplayPause", "nDisplay Pause"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::CapsLock, LOCTEXT("nDisplayCapsLock", "nDisplay Caps Lock"), LOCTEXT("nDisplayCapsLockShort", "nDisplay Caps"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Escape,   LOCTEXT("nDisplayEscape", "nDisplay Escape"), LOCTEXT("EscapenDisplayShort", "nDisplay Esc"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::SpaceBar, LOCTEXT("nDisplaySpaceBar", "nDisplay Space Bar"), LOCTEXT("SpaceBarnDisplayShort", "nDisplay Space"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::PageUp,   LOCTEXT("nDisplayPageUp", "nDisplay Page Up"), LOCTEXT("PageUpnDisplayShort", "nDisplay PgUp"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::PageDown, LOCTEXT("nDisplayPageDown", "nDisplay Page Down"), LOCTEXT("PageDownnDisplayShort", "nDisplay PgDn"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::End,      LOCTEXT("nDisplayEnd", "nDisplay End"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Home,     LOCTEXT("nDisplayHome", "nDisplay Home"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::Left,  LOCTEXT("nDisplayLeft", "nDisplay Left"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Up,    LOCTEXT("nDisplayUp", "nDisplay Up"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Right, LOCTEXT("nDisplayRight", "nDisplay Right"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Down,  LOCTEXT("nDisplayDown", "nDisplay Down"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::Insert, LOCTEXT("nDisplayInsert", "nDisplay Insert"), LOCTEXT("InsertnDisplayShort", "nDisplay Ins"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::BackSpace, LOCTEXT("nDisplayBackSpace", "nDisplay Backspace"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Delete,    LOCTEXT("nDisplayDelete", "nDisplay Delete"), LOCTEXT("DeletenDisplayShort", "nDisplay Del"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::Zero,  FText::FromString("nDisplay 0"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::One,   FText::FromString("nDisplay 1"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Two,   FText::FromString("nDisplay 2"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Three, FText::FromString("nDisplay 3"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Four,  FText::FromString("nDisplay 4"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Five,  FText::FromString("nDisplay 5"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Six,   FText::FromString("nDisplay 6"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Seven, FText::FromString("nDisplay 7"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Eight, FText::FromString("nDisplay 8"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Nine,  FText::FromString("nDisplay 9"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::A, FText::FromString("nDisplay A"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::B, FText::FromString("nDisplay B"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::C, FText::FromString("nDisplay C"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::D, FText::FromString("nDisplay D"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::E, FText::FromString("nDisplay E"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F, FText::FromString("nDisplay F"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::G, FText::FromString("nDisplay G"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::H, FText::FromString("nDisplay H"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::I, FText::FromString("nDisplay I"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::J, FText::FromString("nDisplay J"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::K, FText::FromString("nDisplay K"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::L, FText::FromString("nDisplay L"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::M, FText::FromString("nDisplay M"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::N, FText::FromString("nDisplay N"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::O, FText::FromString("nDisplay O"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::P, FText::FromString("nDisplay P"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Q, FText::FromString("nDisplay Q"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::R, FText::FromString("nDisplay R"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::S, FText::FromString("nDisplay S"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::T, FText::FromString("nDisplay T"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::U, FText::FromString("nDisplay U"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::V, FText::FromString("nDisplay V"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::W, FText::FromString("nDisplay W"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::X, FText::FromString("nDisplay X"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Y, FText::FromString("nDisplay Y"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Z, FText::FromString("nDisplay Z"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumPadZero,  LOCTEXT("nDisplayNumPadZero", "nDisplay Num 0"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumPadOne,   LOCTEXT("nDisplayNumPadOne", "nDisplay Num 1"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumPadTwo,   LOCTEXT("nDisplayNumPadTwo", "nDisplay Num 2"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumPadThree, LOCTEXT("nDisplayNumPadThree", "nDisplay Num 3"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumPadFour,  LOCTEXT("nDisplayNumPadFour", "nDisplay Num 4"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumPadFive,  LOCTEXT("nDisplayNumPadFive", "nDisplay Num 5"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumPadSix,   LOCTEXT("nDisplayNumPadSix", "nDisplay Num 6"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumPadSeven, LOCTEXT("nDisplayNumPadSeven", "nDisplay Num 7"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumPadEight, LOCTEXT("nDisplayNumPadEight", "nDisplay Num 8"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumPadNine,  LOCTEXT("nDisplayNumPadNine", "nDisplay Num 9"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Multiply,    LOCTEXT("nDisplayMultiply", "nDisplay Num *"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Add,         LOCTEXT("nDisplayAdd", "nDisplay Num +"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Subtract,    LOCTEXT("nDisplaySubtract", "nDisplay Num -"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Decimal,     LOCTEXT("nDisplayDecimal", "nDisplay Num ."), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Divide,      LOCTEXT("nDisplayDivide", "nDisplay Num /"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::F1,  LOCTEXT("nDisplayF1", "nDisplay F1"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F2,  LOCTEXT("nDisplayF2", "nDisplay F2"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F3,  LOCTEXT("nDisplayF3", "nDisplay F3"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F4,  LOCTEXT("nDisplayF4", "nDisplay F4"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F5,  LOCTEXT("nDisplayF5", "nDisplay F5"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F6,  LOCTEXT("nDisplayF6", "nDisplay F6"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F7,  LOCTEXT("nDisplayF7", "nDisplay F7"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F8,  LOCTEXT("nDisplayF8", "nDisplay F8"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F9,  LOCTEXT("nDisplayF9", "nDisplay F9"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F10, LOCTEXT("nDisplayF10", "nDisplay F10"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F11, LOCTEXT("nDisplayF11", "nDisplay F11"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::F12, LOCTEXT("nDisplayF12", "nDisplay F12"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::NumLock,    LOCTEXT("nDisplayNumLock", "nDisplay Num Lock"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::ScrollLock, LOCTEXT("nDisplayScrollLock", "nDisplay Scroll Lock"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::LeftShift,    LOCTEXT("nDisplayLeftShift", "nDisplay Left Shift"), NDISLAY_KEYBOARD_CATEGORY(FKeyDetails::ModifierKey)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::RightShift,   LOCTEXT("nDisplayRightShift", "nDisplay Right Shift"), NDISLAY_KEYBOARD_CATEGORY(FKeyDetails::ModifierKey)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::LeftControl,  LOCTEXT("nDisplayLeftControl", "nDisplay Left Ctrl"), NDISLAY_KEYBOARD_CATEGORY(FKeyDetails::ModifierKey)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::RightControl, LOCTEXT("nDisplayRightControl", "nDisplay Right Ctrl"), NDISLAY_KEYBOARD_CATEGORY(FKeyDetails::ModifierKey)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::LeftAlt,      LOCTEXT("nDisplayLeftAlt", "nDisplay Left Alt"), NDISLAY_KEYBOARD_CATEGORY(FKeyDetails::ModifierKey)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::RightAlt,     LOCTEXT("nDisplayRightAlt", "nDisplay Right Alt"), NDISLAY_KEYBOARD_CATEGORY(FKeyDetails::ModifierKey)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::LeftCommand,  LOCTEXT("nDisplayLeftCommand", "nDisplay Left Cmd"), NDISLAY_KEYBOARD_CATEGORY(FKeyDetails::ModifierKey)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::RightCommand, LOCTEXT("nDisplayRightCommand", "nDisplay Right Cmd"), NDISLAY_KEYBOARD_CATEGORY(FKeyDetails::ModifierKey)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::Semicolon, LOCTEXT("nDisplaySemicolon", "nDisplay Semicolon"), FText::FromString("nDisplay ;"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Equals,    LOCTEXT("nDisplayEquals", "nDisplay Equals"), FText::FromString("nDisplay ="), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Comma,     LOCTEXT("nDisplayComma", "nDisplay Comma"), FText::FromString("nDisplay ,"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Hyphen,    LOCTEXT("nDisplayHyphen", "nDisplay Hyphen"), FText::FromString("nDisplay -"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::Tilde, FText::FromString("nDisplay `"), NDISLAY_KEYBOARD_CATEGORY(DefFlags))); // Yes this is not actually a tilde, it is a long, sad, and old story

	EKeys::AddKey(FKeyDetails(FKeyboardKey::Underscore,   LOCTEXT("nDisplayUnderscore", "nDisplay Underscore"), FText::FromString("nDisplay _"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Period,       LOCTEXT("nDisplayPeriod", "nDisplay Period"), FText::FromString("nDisplay ."), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Slash,        LOCTEXT("nDisplaySlash", "nDisplay Slash"), FText::FromString("nDisplay /"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::LeftBracket,  LOCTEXT("nDisplayLeftBracket", "nDisplay Left Bracket"), FText::FromString("nDisplay ["), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Backslash,    LOCTEXT("nDisplayBackslash", "nDisplay Backslash"), FText::FromString("nDisplay \\"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::RightBracket, LOCTEXT("nDisplayRightBracket", "nDisplay Right Bracket"), FText::FromString("nDisplay ]"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Apostrophe,   LOCTEXT("nDisplayApostrophe", "nDisplay Apostrophe"), FText::FromString("nDisplay '"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Quote,        LOCTEXT("nDisplayQuote", "nDisplay Quote"), FText::FromString("nDisplay \""), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::LeftParantheses,  LOCTEXT("nDisplayLeftParantheses", "nDisplay Left Parantheses"), FText::FromString("nDisplay ("), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::RightParantheses, LOCTEXT("nDisplayRightParantheses", "nDisplay Right Parantheses"), FText::FromString("nDisplay )"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Ampersand,        LOCTEXT("nDisplayAmpersand", "nDisplay Ampersand"), FText::FromString("nDisplay &"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Asterix,          LOCTEXT("nDisplayAsterix", "nDisplay Asterisk"), FText::FromString("nDisplay *"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Caret,            LOCTEXT("nDisplayCaret", "nDisplay Caret"), FText::FromString("nDisplay ^"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Dollar,           LOCTEXT("nDisplayDollar", "nDisplay Dollar"), FText::FromString("nDisplay $"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Exclamation,      LOCTEXT("nDisplayExclamation", "nDisplay Exclamation"), FText::FromString("nDisplay !"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Colon,            LOCTEXT("nDisplayColon", "nDisplay Colon"), FText::FromString("nDisplay :"), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));

	EKeys::AddKey(FKeyDetails(FKeyboardKey::A_AccentGrave, FText::FromString(FString::Printf(TEXT("nDisplay %s"), *FString::Chr(224))), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::E_AccentGrave, FText::FromString(FString::Printf(TEXT("nDisplay %s"), *FString::Chr(232))), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::E_AccentAigu,  FText::FromString(FString::Printf(TEXT("nDisplay %s"), *FString::Chr(233))), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::C_Cedille,     FText::FromString(FString::Printf(TEXT("nDisplay %s"), *FString::Chr(231))), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
	EKeys::AddKey(FKeyDetails(FKeyboardKey::Section,       FText::FromString(FString::Printf(TEXT("nDisplay %s"), *FString::Chr(167))), NDISLAY_KEYBOARD_CATEGORY(DefFlags)));
}

void FKeyboardController::ProcessStartSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputKeyboard);

	// Clear old binds
	ResetAllBindings();
	// Read config from cluster & create user binds:
	IDisplayClusterInputManager& DisplayClusterInputManager = *IDisplayCluster::Get().GetInputMgr();

	// Clear old setup
	IDisplayClusterInputManager&  InputMgr = *IDisplayCluster::Get().GetInputMgr();
	IDisplayClusterConfigManager& ConfigMgr = *IDisplayCluster::Get().GetConfigMgr();

	TArray<FString> DeviceNames;
	InputMgr.GetKeyboardDeviceIds(DeviceNames);
	for (const FString& DeviceName : DeviceNames)
	{
		AddDevice(DeviceName);

		FDisplayClusterConfigInput CfgInput;
		if (ConfigMgr.GetInputDevice(DeviceName, CfgInput))
		{
			FString ReflectionParam;
			if (FParse::Value(*CfgInput.Params, DisplayClusterInputStrings::cfg::input::keyboard::TokenReflect, ReflectionParam))
			{
				const EDisplayClusterInputKeyboardReflectMode ReflectionType = ParseReflectionType(ReflectionParam, EDisplayClusterInputKeyboardReflectMode::Refl_None);
				ReflectKeyboard(DeviceName, ReflectionType);
			}
		}

		TArray<FDisplayClusterConfigInputSetup> Records = ConfigMgr.GetInputSetupRecords();
		for (const FDisplayClusterConfigInputSetup& Record : Records)
		{
			if (DeviceName.Compare(Record.Id, ESearchCase::IgnoreCase) == 0)
			{
				BindChannel(DeviceName, Record.Channel, Record.BindName);
			}
		}
	}
}

void FKeyboardController::ProcessEndSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputKeyboard);

	UE_LOG(LogDisplayClusterInputKeyboard, Verbose, TEXT("Removing all keyboard bindings..."));

	ResetAllBindings();
}

void FKeyboardController::ProcessPreTick()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputKeyboard);

	IDisplayClusterInputManager& InputMgr = *IDisplayCluster::Get().GetInputMgr();
	for (auto& DeviceIt : BindMap)
	{
		for (auto& ChannelIt : DeviceIt.Value)
		{
			bool bIsPressed;
			if (InputMgr.GetKeyboardState(DeviceIt.Key, ChannelIt.Key, bIsPressed))
			{
				UE_LOG(LogDisplayClusterInputKeyboard, Verbose, TEXT("Obtained keyboard data %s:%d => %d"), *DeviceIt.Key, ChannelIt.Key, bIsPressed ? 1 : 0);
				ChannelIt.Value.SetData(bIsPressed);
			}
		}
	}
}

EDisplayClusterInputKeyboardReflectMode FKeyboardController::ParseReflectionType(const FString& Text, EDisplayClusterInputKeyboardReflectMode DefaultValue) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputKeyboard);

	FString CleanStr = Text;
	CleanStr = CleanStr.TrimStartAndEnd().TrimQuotes().TrimStartAndEnd();

	if (CleanStr.Compare(DisplayClusterInputStrings::cfg::input::keyboard::ReflectNdisplay, ESearchCase::IgnoreCase) == 0)
	{
		return EDisplayClusterInputKeyboardReflectMode::Refl_nDisplay;
	}
	else if (CleanStr.Compare(DisplayClusterInputStrings::cfg::input::keyboard::ReflectUE4, ESearchCase::IgnoreCase) == 0)
	{
		return EDisplayClusterInputKeyboardReflectMode::Refl_UECore;
	}
	else if (CleanStr.Compare(DisplayClusterInputStrings::cfg::input::keyboard::ReflectBoth, ESearchCase::IgnoreCase) == 0)
	{
		return EDisplayClusterInputKeyboardReflectMode::Refl_Both;
	}
	else if (CleanStr.Compare(DisplayClusterInputStrings::cfg::input::keyboard::ReflectNone, ESearchCase::IgnoreCase) == 0)
	{
		return EDisplayClusterInputKeyboardReflectMode::Refl_None;
	}

	return DefaultValue;
}

void FKeyboardController::ConnectKey(FChannelBinds& KeyboardData, uint32 VrpnChannel, const TCHAR* KeyName)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputKeyboard);

	// Generate channel for this Key:
	if (!KeyboardData.Contains(VrpnChannel))
	{
		KeyboardData.Add(VrpnChannel, FChannelBindData());
	}

	// Reflect key to user defined targets:
	FName NDisplayTargetName(*FString::Printf(TEXT("nDisplayKeyboard%s"), KeyName));
	if (bReflectToNDisplayCluster)
	{
		// Reflect to nDisplay namespace
		KeyboardData[VrpnChannel].BindTarget(NDisplayTargetName);
	}
	else
	{
		// Remove to nDisplay namespace (runtime reflect purpose)
		KeyboardData[VrpnChannel].UnBindTarget(NDisplayTargetName);
	}

	FName UE4TargetName(*FString::Printf(TEXT("%s"), KeyName));
	if (bReflectToUE4)
	{
		// Reflect link to UE4 keyboard
		KeyboardData[VrpnChannel].BindTarget(UE4TargetName);
	}
	else
	{
		// Remove bind to UE4 keyboard  (runtime reflect purpose)
		KeyboardData[VrpnChannel].UnBindTarget(UE4TargetName);
	}
}

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"

// Support reflect vrpn to ue4\nDisplay:
void FKeyboardController::ReflectKeyboard(const FString& VrpnDeviceId, EDisplayClusterInputKeyboardReflectMode ReflectMode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputKeyboard);

	switch (ReflectMode)
	{
	case EDisplayClusterInputKeyboardReflectMode::Refl_Both:
		bReflectToNDisplayCluster = true;
		bReflectToUE4 = true;
		break;
	
	case EDisplayClusterInputKeyboardReflectMode::Refl_nDisplay:
		bReflectToNDisplayCluster = true;
		break;

	case EDisplayClusterInputKeyboardReflectMode::Refl_UECore:
		bReflectToUE4 = true;
		break;

	case EDisplayClusterInputKeyboardReflectMode::Refl_None:
		bReflectToUE4 = false;
		bReflectToNDisplayCluster = false;
		break;

	default:
		bReflectToUE4 = false;
		bReflectToNDisplayCluster = false;
	}

	// Apply reflect, also remove old binds (support reflect mode runtime changes)
	FChannelBinds& KeyboardDeviceData = AddDevice(VrpnDeviceId);
	for (int ScanCode = 0; ScanCode < 256; ++ScanCode)
	{
		uint32 VirtualKeyCode = MapVirtualKey(ScanCode, MAPVK_VSC_TO_VK);
		uint32 CharCode = MapVirtualKey(VirtualKeyCode, MAPVK_VK_TO_CHAR);
		FKey Key = FInputKeyManager::Get().GetKeyFromCodes(VirtualKeyCode, CharCode);

		if (Key != EKeys::Invalid)
		{
			ConnectKey(KeyboardDeviceData, ScanCode, *Key.GetFName().ToString());
		}
	}
}

#include "Windows/HideWindowsPlatformTypes.h"
#endif

#undef LOCTEXT_NAMESPACE
