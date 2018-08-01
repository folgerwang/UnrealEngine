// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/Devices/VRPN/Analog/DisplayClusterVrpnAnalogInputData.h"
#include "Input/Devices/VRPN/Button/DisplayClusterVrpnButtonInputData.h"
#include "Input/Devices/VRPN/Tracker/DisplayClusterVrpnTrackerInputData.h"


/**
 * Available types of input devices
 */
enum EDisplayClusterInputDevice
{
	VrpnAnalog = 0,
	VrpnButton,
	VrpnTracker
};


/**
 * Input device traits
 */
template<int DevTypeID>
struct display_cluster_input_device_traits { };

/**
 * Specialization for VRPN analog device
 */
template <>
struct display_cluster_input_device_traits<EDisplayClusterInputDevice::VrpnAnalog>
{
	typedef FDisplayClusterVrpnAnalogChannelData           dev_channel_data_type;
};

/**
 * Specialization for VRPN button device
 */
template <>
struct display_cluster_input_device_traits<EDisplayClusterInputDevice::VrpnButton>
{
	typedef FDisplayClusterVrpnButtonChannelData           dev_channel_data_type;
};

/**
 * Specialization for VRPN tracker device
 */
template <>
struct display_cluster_input_device_traits<EDisplayClusterInputDevice::VrpnTracker>
{
	typedef FDisplayClusterVrpnTrackerChannelData          dev_channel_data_type;
};
