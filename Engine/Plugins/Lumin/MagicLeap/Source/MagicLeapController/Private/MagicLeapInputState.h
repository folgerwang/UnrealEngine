// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#pragma once

//-------------------------------------------------------------------------------------------------
// FMagicLeapControllerState - Input state for the magic leap motion controller
//-------------------------------------------------------------------------------------------------

struct FMagicLeapControllerState
{
	/** True if the device is connected, otherwise false */
	bool bIsConnected;

	/** True if position is being tracked, otherwise false */
	bool bIsPositionTracked;

	/** True if orientation is being tracked, otherwise false */
	bool bIsOrientationTracked;

	/** Whether or not we're playing a haptic effect.  If true, force feedback calls will be early-outed in favor of the haptic effect */
	bool bPlayingHapticEffect;
	/** Haptic frequency (zero to disable) */
	float HapticFrequency;
	/** Haptic amplitude (zero to disable) */
	float HapticAmplitude;

	/** Explicit constructor sets up sensible defaults */
	FMagicLeapControllerState(const EControllerHand Hand)
		: bIsConnected(false),
		bIsPositionTracked(false),
		bIsOrientationTracked(false),
		bPlayingHapticEffect(false),
		HapticFrequency(0.0f),
		HapticAmplitude(0.0f)
	{
	}

	/** Default constructor does nothing.  Don't use it.  This only exists because we cannot initialize an array of objects with no default constructor on non-C++ 11 compliant compilers (VS 2013) */
	FMagicLeapControllerState()
	{
	}
};


