// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/IDisplayClusterInputManager.h"
#include "Input/Devices/VRPN/Analog/DisplayClusterVrpnAnalogInputData.h"
#include "Input/Devices/VRPN/Button/DisplayClusterVrpnButtonInputData.h"
#include "Input/Devices/VRPN/Keyboard/DisplayClusterVrpnKeyboardInputData.h"
#include "Input/Devices/VRPN/Tracker/DisplayClusterVrpnTrackerInputData.h"


/**
 * Input device traits
 */
template<int DevTypeID>
struct display_cluster_input_device_traits { };

/**
 * Specialization for VRPN analog device
 */
template <>
struct display_cluster_input_device_traits<EDisplayClusterInputDeviceType::VrpnAnalog>
{
	typedef FDisplayClusterVrpnAnalogChannelData           dev_channel_data_type;
};

/**
 * Specialization for VRPN button device
 */
template <>
struct display_cluster_input_device_traits<EDisplayClusterInputDeviceType::VrpnButton>
{
	typedef FDisplayClusterVrpnButtonChannelData           dev_channel_data_type;
};

/**
 * Specialization for VRPN keyboard device
 */
template <>
struct display_cluster_input_device_traits<EDisplayClusterInputDeviceType::VrpnKeyboard>
{
	typedef FDisplayClusterVrpnKeyboardChannelData         dev_channel_data_type;
};

/**
 * Specialization for VRPN tracker device
 */
template <>
struct display_cluster_input_device_traits<EDisplayClusterInputDeviceType::VrpnTracker>
{
	typedef FDisplayClusterVrpnTrackerChannelData          dev_channel_data_type;
};
