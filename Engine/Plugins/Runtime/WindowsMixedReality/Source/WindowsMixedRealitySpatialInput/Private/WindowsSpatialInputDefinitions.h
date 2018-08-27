// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "InputCoreTypes.h"

/*
* Controller button name definitions
*/
#define LeftMenuName "WindowsSpatialInput_LeftMenu"
#define LeftMenuFriendlyName "Windows Spatial Input (L) Menu"
#define RightMenuName "WindowsSpatialInput_RightMenu"
#define RightMenuFriendlyName "Windows Spatial Input (R) Menu"

#define LeftTouchpadPressName "WindowsSpatialInput_LeftTouchpad"
#define LeftTouchpadPressFriendlyName "Windows Spatial Input (L) Touchpad"
#define RightTouchpadPressName "WindowsSpatialInput_RightTouchpad"
#define RightTouchpadPressFriendlyName "Windows Spatial Input (R) Touchpad"

#define LeftTouchpadIsTouchedName "WindowsSpatialInput_LeftTouchpadIsTouched"
#define LeftTouchpadIsTouchedFriendlyName "Windows Spatial Input (L) Touchpad Is Touched"
#define RightTouchpadIsTouchedName "WindowsSpatialInput_RightTouchpadIsTouched"	
#define RightTouchpadIsTouchedFriendlyName "Windows Spatial Input (R) Touchpad Is Touched"

/*
* Controller axis name definitions
*/

#define LeftTouchpadXName "WindowsSpatialInput_LeftTouchpad_X"
#define LeftTouchpadXFriendlyName "Windows Spatial Input (L) Touchpad X"
#define RightTouchpadXName "WindowsSpatialInput_RightTouchpad_X"
#define RightTouchpadXFriendlyName "Windows Spatial Input (R) Touchpad X"

#define LeftTouchpadYName "WindowsSpatialInput_LeftTouchpad_Y"
#define LeftTouchpadYFriendlyName "Windows Spatial Input (L) Touchpad Y"
#define RightTouchpadYName "WindowsSpatialInput_RightTouchpad_Y"
#define RightTouchpadYFriendlyName "Windows Spatial Input (R) Touchpad Y"

/*
* Keys struct
*/

struct FSpatialInputKeys
{
	static const FKey LeftGrasp;
	static const FKey RightGrasp;

	static const FKey LeftMenu;
	static const FKey RightMenu;

	static const FKey LeftTouchpadPress;
	static const FKey RightTouchpadPress;

	static const FKey LeftTouchpadIsTouched;
	static const FKey RightTouchpadIsTouched;

	static const FKey LeftTouchpadX;
	static const FKey RightTouchpadX;

	static const FKey LeftTouchpadY;
	static const FKey RightTouchpadY;
};
