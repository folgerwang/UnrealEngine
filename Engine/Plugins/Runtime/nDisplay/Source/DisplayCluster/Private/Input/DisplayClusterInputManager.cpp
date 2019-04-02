// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Input/DisplayClusterInputManager.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Config/IPDisplayClusterConfigManager.h"

#include "Devices/VRPN/Analog/DisplayClusterVrpnAnalogInputDevice.h"
#include "Devices/VRPN/Button/DisplayClusterVrpnButtonInputDevice.h"
#include "Devices/VRPN/Tracker/DisplayClusterVrpnTrackerInputDevice.h"
#include "Devices/VRPN/Keyboard/DisplayClusterVrpnKeyboardInputDevice.h"

#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterGameMode.h"
#include "DisplayClusterGlobals.h"


FDisplayClusterInputManager::FDisplayClusterInputManager()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);
}

FDisplayClusterInputManager::~FDisplayClusterInputManager()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterInputManager::Init(EDisplayClusterOperationMode OperationMode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);

	return true;
}

void FDisplayClusterInputManager::Release()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);
}

bool FDisplayClusterInputManager::StartSession(const FString& configPath, const FString& nodeId)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);

	ConfigPath = configPath;
	ClusterNodeId = nodeId;

	if (!InitDevices())
	{
		UE_LOG(LogDisplayClusterInput, Error, TEXT("Couldn't initialize input devices"));
		return false;
	}

	return true;
}

void FDisplayClusterInputManager::EndSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);

	ReleaseDevices();
}

bool FDisplayClusterInputManager::StartScene(UWorld* pWorld)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);

	check(pWorld);
	CurrentWorld = pWorld;

	return true;
}

void FDisplayClusterInputManager::EndScene()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);
}

void FDisplayClusterInputManager::PreTick(float DeltaSeconds)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterInputManager
//////////////////////////////////////////////////////////////////////////////////////////////
// Device API
const IDisplayClusterInputDevice* FDisplayClusterInputManager::GetDevice(EDisplayClusterInputDeviceType DeviceType, const FString& DeviceID) const
{
	if (Devices.Contains(DeviceType))
	{
		if (Devices[DeviceType].Contains(DeviceID))
		{
			return Devices[DeviceType][DeviceID].Get();
		}
	}

	return nullptr;
}

// Basic functionality (device amount)
uint32 FDisplayClusterInputManager::GetAxisDeviceAmount() const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetDeviceAmount_impl<EDisplayClusterInputDeviceType::VrpnAnalog>();
}

uint32 FDisplayClusterInputManager::GetButtonDeviceAmount() const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetDeviceAmount_impl<EDisplayClusterInputDeviceType::VrpnButton>();
}

uint32 FDisplayClusterInputManager::GetKeyboardDeviceAmount() const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetDeviceAmount_impl<EDisplayClusterInputDeviceType::VrpnKeyboard>();
}


uint32 FDisplayClusterInputManager::GetTrackerDeviceAmount() const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetDeviceAmount_impl<EDisplayClusterInputDeviceType::VrpnTracker>();
}


// Access to the device lists
bool FDisplayClusterInputManager::GetAxisDeviceIds(TArray<FString>& ids) const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetDeviceIds_impl<EDisplayClusterInputDeviceType::VrpnAnalog>(ids);
}

bool FDisplayClusterInputManager::GetButtonDeviceIds(TArray<FString>& ids) const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetDeviceIds_impl<EDisplayClusterInputDeviceType::VrpnButton>(ids);
}

bool FDisplayClusterInputManager::GetKeyboardDeviceIds(TArray<FString>& ids) const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetDeviceIds_impl<EDisplayClusterInputDeviceType::VrpnKeyboard>(ids);
}

bool FDisplayClusterInputManager::GetTrackerDeviceIds(TArray<FString>& ids) const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetDeviceIds_impl<EDisplayClusterInputDeviceType::VrpnTracker>(ids);
}


// Button data access
bool FDisplayClusterInputManager::GetButtonState(const FString& devId, const uint8 btn, bool& curState) const
{
	FDisplayClusterVrpnButtonChannelData data;
	if (GetButtonData(devId, btn, data))
	{
		curState = data.btnStateNew;
		return true;
	}

	return false;
}

bool FDisplayClusterInputManager::IsButtonPressed(const FString& devId, const uint8 btn, bool& curPressed) const
{
	bool btnState;
	if (GetButtonState(devId, btn, btnState))
	{
		curPressed = (btnState == true);
		return true;
	}

	return false;
}

bool FDisplayClusterInputManager::IsButtonReleased(const FString& devId, const uint8 btn, bool& curReleased) const
{
	bool btnState;
	if (GetButtonState(devId, btn, btnState))
	{
		curReleased = (btnState == false);
		return true;
	}

	return false;
}

bool FDisplayClusterInputManager::WasButtonPressed(const FString& devId, const uint8 btn, bool& wasPressed) const
{
	FDisplayClusterVrpnButtonChannelData data;
	if (GetButtonData(devId, btn, data))
	{
		wasPressed = (data.btnStateOld == false && data.btnStateNew == true);
		return true;
	}

	return false;
}

bool FDisplayClusterInputManager::WasButtonReleased(const FString& devId, const uint8 btn, bool& wasReleased) const
{
	FDisplayClusterVrpnButtonChannelData data;
	if (GetButtonData(devId, btn, data))
	{
		wasReleased = (data.btnStateOld == true && data.btnStateNew == false);
		return true;
	}

	return false;
}

// Keyboard data access
bool FDisplayClusterInputManager::GetKeyboardState(const FString& devId, const uint8 btn, bool& curState) const
{
	FDisplayClusterVrpnKeyboardChannelData data;
	if (GetKeyboardData(devId, btn, data))
	{
		curState = data.btnStateNew;
		return true;
	}

	return false;
}

bool FDisplayClusterInputManager::IsKeyboardPressed(const FString& devId, const uint8 btn, bool& curPressed) const
{
	bool btnState;
	if (GetKeyboardState(devId, btn, btnState))
	{
		curPressed = (btnState == true);
		return true;
	}

	return false;
}

bool FDisplayClusterInputManager::IsKeyboardReleased(const FString& devId, const uint8 btn, bool& curReleased) const
{
	bool btnState;
	if (GetKeyboardState(devId, btn, btnState))
	{
		curReleased = (btnState == false);
		return true;
	}

	return false;
}

bool FDisplayClusterInputManager::WasKeyboardPressed(const FString& devId, const uint8 btn, bool& wasPressed) const
{
	FDisplayClusterVrpnKeyboardChannelData data;
	if (GetKeyboardData(devId, btn, data))
	{
		wasPressed = (data.btnStateOld == false && data.btnStateNew == true);
		return true;
	}

	return false;
}

bool FDisplayClusterInputManager::WasKeyboardReleased(const FString& devId, const uint8 btn, bool& wasReleased) const
{
	FDisplayClusterVrpnKeyboardChannelData data;
	if (GetKeyboardData(devId, btn, data))
	{
		wasReleased = (data.btnStateOld == true && data.btnStateNew == false);
		return true;
	}

	return false;
}

// Axes data access
bool FDisplayClusterInputManager::GetAxis(const FString& devId, const uint8 axis, float& value) const
{
	FDisplayClusterVrpnAnalogChannelData data;
	if (GetAxisData(devId, axis, data))
	{
		value = data.axisValue;
		return true;
	}

	return false;
}

// Tracking data access
bool FDisplayClusterInputManager::GetTrackerLocation(const FString& devId, const uint8 tr, FVector& location) const
{
	FDisplayClusterVrpnTrackerChannelData data;
	if (GetTrackerData(devId, tr, data))
	{
		location = data.trLoc;
		return true;
	}

	return false;
}

bool FDisplayClusterInputManager::GetTrackerQuat(const FString& devId, const uint8 tr, FQuat& rotation) const
{
	FDisplayClusterVrpnTrackerChannelData data;
	if (GetTrackerData(devId, tr, data))
	{
		rotation = data.trQuat;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterInputManager
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterInputManager::Update()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);

	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return;
	}

	// Perform input update on master only. Slaves' state will be replicated later.
	if (GDisplayCluster->GetPrivateClusterMgr()->IsMaster())
	{
		UE_LOG(LogDisplayClusterInput, Verbose, TEXT("Input update started"));
		{
			FScopeLock ScopeLock(&InternalsSyncScope);

			// Pre-Update
			UE_LOG(LogDisplayClusterInput, Verbose, TEXT("Input pre-update..."));
			for (auto classIt = Devices.CreateIterator(); classIt; ++classIt)
			{
				for (auto devIt = classIt->Value.CreateConstIterator(); devIt; ++devIt)
				{
					devIt->Value->PreUpdate();
				}
			}

			// Update
			UE_LOG(LogDisplayClusterInput, Verbose, TEXT("Input update..."));
			for (auto classIt = Devices.CreateIterator(); classIt; ++classIt)
			{
				for (auto devIt = classIt->Value.CreateConstIterator(); devIt; ++devIt)
				{
					devIt->Value->Update();
				}
			}

			// Post-Update
			for (auto classIt = Devices.CreateIterator(); classIt; ++classIt)
			{
				for (auto devIt = classIt->Value.CreateConstIterator(); devIt; ++devIt)
				{
					devIt->Value->PostUpdate();
				}
			}
		}
		UE_LOG(LogDisplayClusterInput, Verbose, TEXT("Input update finished"));
	
		// Update input data cache for slave nodes
		UpdateInputDataCache();
	}
}

void FDisplayClusterInputManager::ExportInputData(FDisplayClusterMessage::DataType& data) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);

	FScopeLock ScopeLock(&InternalsSyncScope);

	// Get data from cache
	data = PackedTransferData;
}

void FDisplayClusterInputManager::ImportInputData(const FDisplayClusterMessage::DataType& data)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);

	FScopeLock ScopeLock(&InternalsSyncScope);

	for (auto rec : data)
	{
		FString strClassId;
		FString strDevId;
		if (rec.Key.Split(FString(SerializationDeviceTypeNameDelimiter), &strClassId, &strDevId))
		{
			UE_LOG(LogDisplayClusterInput, VeryVerbose, TEXT("Deserializing input device: <%s, %s>"), *rec.Key, *rec.Value);

			int classId = FCString::Atoi(*strClassId);
			if (Devices.Contains(classId))
			{
				if (Devices[classId].Contains(strDevId))
				{
					Devices[classId][strDevId]->DeserializeFromString(rec.Value);
				}
			}
		}
	}
}


bool FDisplayClusterInputManager::GetAxisData(const FString& devId, const uint8 channel, FDisplayClusterVrpnAnalogChannelData&  data) const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetChannelData_impl<EDisplayClusterInputDeviceType::VrpnAnalog>(devId, channel, data);
}

bool FDisplayClusterInputManager::GetButtonData(const FString& devId, const uint8 channel, FDisplayClusterVrpnButtonChannelData&  data) const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetChannelData_impl<EDisplayClusterInputDeviceType::VrpnButton>(devId, channel, data);
}

bool FDisplayClusterInputManager::GetKeyboardData(const FString& devId, const uint8 channel, FDisplayClusterVrpnKeyboardChannelData&  data) const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetChannelData_impl<EDisplayClusterInputDeviceType::VrpnKeyboard>(devId, channel, data);
}

bool FDisplayClusterInputManager::GetTrackerData(const FString& devId, const uint8 channel, FDisplayClusterVrpnTrackerChannelData& data) const
{
	FScopeLock ScopeLock(&InternalsSyncScope);
	return GetChannelData_impl<EDisplayClusterInputDeviceType::VrpnTracker>(devId, channel, data);
}

template<int DevTypeID>
uint32 FDisplayClusterInputManager::GetDeviceAmount_impl() const
{
	if (!Devices.Contains(DevTypeID))
	{
		return 0;
	}

	return static_cast<uint32>(Devices[DevTypeID].Num());
}

template<int DevTypeID>
bool FDisplayClusterInputManager::GetDeviceIds_impl(TArray<FString>& ids) const
{
	if (!Devices.Contains(DevTypeID))
	{
		return false;
	}

	Devices[DevTypeID].GenerateKeyArray(ids);
	return true;
}

template<int DevTypeID>
bool FDisplayClusterInputManager::GetChannelData_impl(const FString& devId, const uint8 channel, typename display_cluster_input_device_traits<DevTypeID>::dev_channel_data_type& data) const
{
	if (!Devices.Contains(DevTypeID))
	{
		return false;
	}

	if (!Devices[DevTypeID].Contains(devId))
	{
		return false;
	}

	return static_cast<FDisplayClusterInputDeviceBase<DevTypeID>*>(Devices[DevTypeID][devId].Get())->GetChannelData(channel, data);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterInputManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterInputManager::InitDevices()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);

	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return false;
	}

	FScopeLock ScopeLock(&InternalsSyncScope);

	UE_LOG(LogDisplayClusterInput, Log, TEXT("Initializing input devices..."));

	const TArray<FDisplayClusterConfigInput> cfgInputDevs = GDisplayCluster->GetPrivateConfigMgr()->GetInputDevices();

	for (auto& cfgDev : cfgInputDevs)
	{
		UE_LOG(LogDisplayClusterInput, Verbose, TEXT("Creating input device: %s"), *cfgDev.ToString());

		IDisplayClusterInputDevice* pDev = nullptr;

		if (cfgDev.Type.Compare(FString(DisplayClusterStrings::cfg::data::input::DeviceAnalog), ESearchCase::IgnoreCase) == 0)
		{
			if (GDisplayCluster->GetPrivateClusterMgr()->IsMaster())
			{
				pDev = new FDisplayClusterVrpnAnalogInputDevice(cfgDev);
			}
			else
			{
				pDev = new FDisplayClusterVrpnAnalogInputDataHolder(cfgDev);
			}
		}
		else if (cfgDev.Type.Compare(FString(DisplayClusterStrings::cfg::data::input::DeviceButtons), ESearchCase::IgnoreCase) == 0)
		{
			if (GDisplayCluster->GetPrivateClusterMgr()->IsMaster())
			{
				pDev = new FDisplayClusterVrpnButtonInputDevice(cfgDev);
			}
			else
			{
				pDev = new FDisplayClusterVrpnButtonInputDataHolder(cfgDev);
			}
		}
		else if (cfgDev.Type.Compare(FString(DisplayClusterStrings::cfg::data::input::DeviceTracker), ESearchCase::IgnoreCase) == 0)
		{
			if (GDisplayCluster->GetPrivateClusterMgr()->IsMaster())
			{
				pDev = new FDisplayClusterVrpnTrackerInputDevice(cfgDev);
			}
			else
			{
				pDev = new FDisplayClusterVrpnTrackerInputDataHolder(cfgDev);
			}
		}
		else if (cfgDev.Type.Compare(FString(DisplayClusterStrings::cfg::data::input::DeviceKeyboard), ESearchCase::IgnoreCase) == 0)
		{
			if (GDisplayCluster->GetPrivateClusterMgr()->IsMaster())
			{
				pDev = new FDisplayClusterVrpnKeyboardInputDevice(cfgDev);
			}
			else
			{
				pDev = new FDisplayClusterVrpnKeyboardInputDataHolder(cfgDev);
			}
		}
		else
		{
			UE_LOG(LogDisplayClusterInput, Error, TEXT("Unsupported device type: %s"), *cfgDev.Type);
			continue;
		}

		if (pDev && pDev->Initialize())
		{
			UE_LOG(LogDisplayClusterInput, Log, TEXT("Adding device: %s"), *pDev->ToString());
			
			auto pDevMap = Devices.Find(pDev->GetTypeId());
			if (!pDevMap)
			{
				pDevMap = &Devices.Add(pDev->GetTypeId());
			}

			pDevMap->Add(cfgDev.Id, TDevice(pDev));
		}
		else
		{
			UE_LOG(LogDisplayClusterInput, Warning, TEXT("Neither data holder nor true device was instantiated for item id: %s"), *cfgDev.Id);

			// It's safe to delete nullptr so no checking performed
			delete pDev;

			//@note: Allow other devices to be initialized. User will locate the problem from logs.
			//return false;
		}
	}

	return true;
}

void FDisplayClusterInputManager::ReleaseDevices()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);

	FScopeLock ScopeLock(&InternalsSyncScope);

	UE_LOG(LogDisplayClusterInput, Log, TEXT("Releasing input subsystem..."));

	UE_LOG(LogDisplayClusterInput, Log, TEXT("Releasing input devices..."));
	Devices.Empty();
}

void FDisplayClusterInputManager::UpdateInputDataCache()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInput);

	FScopeLock ScopeLock(&InternalsSyncScope);

	// Clear previously cached data
	PackedTransferData.Empty(PackedTransferData.Num() | 0x07);

	for (auto classIt = Devices.CreateConstIterator(); classIt; ++classIt)
	{
		for (auto devIt = classIt->Value.CreateConstIterator(); devIt; ++devIt)
		{
			const FString key = FString::Printf(TEXT("%d%s%s"), classIt->Key, SerializationDeviceTypeNameDelimiter, *devIt->Key);
			const FString val = devIt->Value->SerializeToString();
			UE_LOG(LogDisplayClusterInput, VeryVerbose, TEXT("Input device %d:%s serialized: <%s, %s>"), classIt->Key, *devIt->Key, *key, *val);
			PackedTransferData.Add(key, val);
		}
	}
}
