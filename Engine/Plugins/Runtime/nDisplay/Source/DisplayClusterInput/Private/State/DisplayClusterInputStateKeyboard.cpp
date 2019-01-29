// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterInputStateKeyboard.h"
#include "Misc/DisplayClusterInputLog.h"


/*
 * Define global UE4 names for DisplayCluster vrpn keyboard buttons:
 */

const FKey FKeyboardKey::Zero("nDisplayKeyboardZero");
const FKey FKeyboardKey::One("nDisplayKeyboardOne");
const FKey FKeyboardKey::Two("nDisplayKeyboardTwo");
const FKey FKeyboardKey::Three("nDisplayKeyboardThree");
const FKey FKeyboardKey::Four("nDisplayKeyboardFour");
const FKey FKeyboardKey::Five("nDisplayKeyboardFive");
const FKey FKeyboardKey::Six("nDisplayKeyboardSix");
const FKey FKeyboardKey::Seven("nDisplayKeyboardSeven");
const FKey FKeyboardKey::Eight("nDisplayKeyboardEight");
const FKey FKeyboardKey::Nine("nDisplayKeyboardNine");

const FKey FKeyboardKey::A("nDisplayKeyboardA");
const FKey FKeyboardKey::B("nDisplayKeyboardB");
const FKey FKeyboardKey::C("nDisplayKeyboardC");
const FKey FKeyboardKey::D("nDisplayKeyboardD");
const FKey FKeyboardKey::E("nDisplayKeyboardE");
const FKey FKeyboardKey::F("nDisplayKeyboardF");
const FKey FKeyboardKey::G("nDisplayKeyboardG");
const FKey FKeyboardKey::H("nDisplayKeyboardH");
const FKey FKeyboardKey::I("nDisplayKeyboardI");
const FKey FKeyboardKey::J("nDisplayKeyboardJ");
const FKey FKeyboardKey::K("nDisplayKeyboardK");
const FKey FKeyboardKey::L("nDisplayKeyboardL");
const FKey FKeyboardKey::M("nDisplayKeyboardM");
const FKey FKeyboardKey::N("nDisplayKeyboardN");
const FKey FKeyboardKey::O("nDisplayKeyboardO");
const FKey FKeyboardKey::P("nDisplayKeyboardP");
const FKey FKeyboardKey::Q("nDisplayKeyboardQ");
const FKey FKeyboardKey::R("nDisplayKeyboardR");
const FKey FKeyboardKey::S("nDisplayKeyboardS");
const FKey FKeyboardKey::T("nDisplayKeyboardT");
const FKey FKeyboardKey::U("nDisplayKeyboardU");
const FKey FKeyboardKey::V("nDisplayKeyboardV");
const FKey FKeyboardKey::W("nDisplayKeyboardW");
const FKey FKeyboardKey::X("nDisplayKeyboardX");
const FKey FKeyboardKey::Y("nDisplayKeyboardY");
const FKey FKeyboardKey::Z("nDisplayKeyboardZ");

const FKey FKeyboardKey::Semicolon("nDisplayKeyboardSemicolon");
const FKey FKeyboardKey::Equals("nDisplayKeyboardEquals");
const FKey FKeyboardKey::Comma("nDisplayKeyboardComma");
const FKey FKeyboardKey::Hyphen("nDisplayKeyboardHyphen");
const FKey FKeyboardKey::Period("nDisplayKeyboardPeriod");
const FKey FKeyboardKey::Slash("nDisplayKeyboardSlash");
const FKey FKeyboardKey::Tilde("nDisplayKeyboardTilde");
const FKey FKeyboardKey::LeftBracket("nDisplayKeyboardLeftBracket");
const FKey FKeyboardKey::Backslash("nDisplayKeyboardBackslash");
const FKey FKeyboardKey::RightBracket("nDisplayKeyboardRightBracket");
const FKey FKeyboardKey::Apostrophe("nDisplayKeyboardApostrophe");
const FKey FKeyboardKey::SpaceBar("nDisplayKeyboardSpaceBar");

const FKey FKeyboardKey::Ampersand("nDisplayKeyboardAmpersand");
const FKey FKeyboardKey::Asterix("nDisplayKeyboardAsterix");
const FKey FKeyboardKey::Caret("nDisplayKeyboardCaret");
const FKey FKeyboardKey::Colon("nDisplayKeyboardColon");
const FKey FKeyboardKey::Dollar("nDisplayKeyboardDollar");
const FKey FKeyboardKey::Exclamation("nDisplayKeyboardExclamation");

const FKey FKeyboardKey::LeftParantheses("nDisplayKeyboardLeftParantheses");
const FKey FKeyboardKey::RightParantheses("nDisplayKeyboardRightParantheses");
const FKey FKeyboardKey::Quote("nDisplayKeyboardQuote");
const FKey FKeyboardKey::Underscore("nDisplayKeyboardUnderscore");

const FKey FKeyboardKey::A_AccentGrave("nDisplayKeyboardA_AccentGrave");
const FKey FKeyboardKey::E_AccentGrave("nDisplayKeyboardE_AccentGrave");
const FKey FKeyboardKey::E_AccentAigu("nDisplayKeyboardE_AccentAigu");
const FKey FKeyboardKey::C_Cedille("nDisplayKeyboardC_Cedille");
const FKey FKeyboardKey::Section("nDisplayKeyboardSection");

const FKey FKeyboardKey::BackSpace("nDisplayKeyboardBackSpace");
const FKey FKeyboardKey::Tab("nDisplayKeyboardTab");
const FKey FKeyboardKey::Enter("nDisplayKeyboardEnter");
const FKey FKeyboardKey::Pause("nDisplayKeyboardPause");

const FKey FKeyboardKey::CapsLock("nDisplayKeyboardCapsLock");
const FKey FKeyboardKey::Escape("nDisplayKeyboardEscape");
const FKey FKeyboardKey::PageUp("nDisplayKeyboardPageUp");
const FKey FKeyboardKey::PageDown("nDisplayKeyboardPageDown");
const FKey FKeyboardKey::End("nDisplayKeyboardEnd");
const FKey FKeyboardKey::Home("nDisplayKeyboardHome");

const FKey FKeyboardKey::Left("nDisplayKeyboardLeft");
const FKey FKeyboardKey::Up("nDisplayKeyboardUp");
const FKey FKeyboardKey::Right("nDisplayKeyboardRight");
const FKey FKeyboardKey::Down("nDisplayKeyboardDown");

const FKey FKeyboardKey::Insert("nDisplayKeyboardInsert");
const FKey FKeyboardKey::Delete("nDisplayKeyboardDelete");


const FKey FKeyboardKey::NumPadZero("nDisplayKeyboardNumPadZero");
const FKey FKeyboardKey::NumPadOne("nDisplayKeyboardNumPadOne");
const FKey FKeyboardKey::NumPadTwo("nDisplayKeyboardNumPadTwo");
const FKey FKeyboardKey::NumPadThree("nDisplayKeyboardNumPadThree");
const FKey FKeyboardKey::NumPadFour("nDisplayKeyboardNumPadFour");
const FKey FKeyboardKey::NumPadFive("nDisplayKeyboardNumPadFive");
const FKey FKeyboardKey::NumPadSix("nDisplayKeyboardNumPadSix");
const FKey FKeyboardKey::NumPadSeven("nDisplayKeyboardNumPadSeven");
const FKey FKeyboardKey::NumPadEight("nDisplayKeyboardNumPadEight");
const FKey FKeyboardKey::NumPadNine("nDisplayKeyboardNumPadNine");


const FKey FKeyboardKey::Multiply("nDisplayKeyboardMultiply");
const FKey FKeyboardKey::Add("nDisplayKeyboardAdd");
const FKey FKeyboardKey::Subtract("nDisplayKeyboardSubtract");
const FKey FKeyboardKey::Decimal("nDisplayKeyboardDecimal");
const FKey FKeyboardKey::Divide("nDisplayKeyboardDivide");

const FKey FKeyboardKey::F1("nDisplayKeyboardF1");
const FKey FKeyboardKey::F2("nDisplayKeyboardF2");
const FKey FKeyboardKey::F3("nDisplayKeyboardF3");
const FKey FKeyboardKey::F4("nDisplayKeyboardF4");
const FKey FKeyboardKey::F5("nDisplayKeyboardF5");
const FKey FKeyboardKey::F6("nDisplayKeyboardF6");
const FKey FKeyboardKey::F7("nDisplayKeyboardF7");
const FKey FKeyboardKey::F8("nDisplayKeyboardF8");
const FKey FKeyboardKey::F9("nDisplayKeyboardF9");
const FKey FKeyboardKey::F10("nDisplayKeyboardF10");
const FKey FKeyboardKey::F11("nDisplayKeyboardF11");
const FKey FKeyboardKey::F12("nDisplayKeyboardF12");

const FKey FKeyboardKey::NumLock("nDisplayKeyboardNumLock");

const FKey FKeyboardKey::ScrollLock("nDisplayKeyboardScrollLock");

const FKey FKeyboardKey::LeftShift("nDisplayKeyboardLeftShift");
const FKey FKeyboardKey::RightShift("nDisplayKeyboardRightShift");
const FKey FKeyboardKey::LeftControl("nDisplayKeyboardLeftControl");
const FKey FKeyboardKey::RightControl("nDisplayKeyboardRightControl");
const FKey FKeyboardKey::LeftAlt("nDisplayKeyboardLeftAlt");
const FKey FKeyboardKey::RightAlt("nDisplayKeyboardRightAlt");
const FKey FKeyboardKey::LeftCommand("nDisplayKeyboardLeftCommand");
const FKey FKeyboardKey::RightCommand("nDisplayKeyboardRightCommand");
