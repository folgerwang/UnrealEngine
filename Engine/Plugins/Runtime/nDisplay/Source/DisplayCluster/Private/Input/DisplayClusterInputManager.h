// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/IPDisplayClusterInputManager.h"
#include "Devices/DisplayClusterInputDeviceTraits.h"
#include "Network/DisplayClusterMessage.h"

class IDisplayClusterInputDevice;
struct FDisplayClusterVrpnAnalogChannelData;
struct FDisplayClusterVrpnButtonChannelData;
struct FDisplayClusterVrpnKeyboardChannelData;
struct FDisplayClusterVrpnTrackerChannelData;


/**
 * Input manager. Implements everything related to VR input devices (VRPN, etc.)
 */
class FDisplayClusterInputManager
	: public IPDisplayClusterInputManager
{
public:
	FDisplayClusterInputManager();
	virtual ~FDisplayClusterInputManager();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Init(EDisplayClusterOperationMode OperationMode) override;
	virtual void Release() override;
	virtual bool StartSession(const FString& configPath, const FString& nodeId) override;
	virtual void EndSession() override;
	virtual bool StartScene(UWorld* pWorld) override;
	virtual void EndScene() override;
	virtual void PreTick(float DeltaSeconds);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterInputManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Device API
	virtual const IDisplayClusterInputDevice* GetDevice(EDisplayClusterInputDeviceType DeviceType, const FString& DeviceID) const override;

	// Device amount
	virtual uint32 GetAxisDeviceAmount()     const override;
	virtual uint32 GetButtonDeviceAmount()   const override;
	virtual uint32 GetKeyboardDeviceAmount() const override;
	virtual uint32 GetTrackerDeviceAmount()  const override;

	// Device IDs
	virtual bool GetAxisDeviceIds    (TArray<FString>& ids) const override;
	virtual bool GetButtonDeviceIds  (TArray<FString>& ids) const override;
	virtual bool GetKeyboardDeviceIds(TArray<FString>& ids) const override;
	virtual bool GetTrackerDeviceIds (TArray<FString>& ids) const override;

	// Axes data access
	virtual bool GetAxis(const FString& devId, const uint8 axis, float& value) const override;

	// Button data access
	virtual bool GetButtonState   (const FString& devId, const uint8 btn, bool& curState)    const override;
	virtual bool IsButtonPressed  (const FString& devId, const uint8 btn, bool& curPressed)  const override;
	virtual bool IsButtonReleased (const FString& devId, const uint8 btn, bool& curReleased) const override;
	virtual bool WasButtonPressed (const FString& devId, const uint8 btn, bool& wasPressed)  const override;
	virtual bool WasButtonReleased(const FString& devId, const uint8 btn, bool& wasReleased) const override;

	// Keyboard data access
	virtual bool GetKeyboardState   (const FString& devId, const uint8 btn, bool& curState)    const override;
	virtual bool IsKeyboardPressed  (const FString& devId, const uint8 btn, bool& curPressed)  const override;
	virtual bool IsKeyboardReleased (const FString& devId, const uint8 btn, bool& curReleased) const override;
	virtual bool WasKeyboardPressed (const FString& devId, const uint8 btn, bool& wasPressed)  const override;
	virtual bool WasKeyboardReleased(const FString& devId, const uint8 btn, bool& wasReleased) const override;

	// Tracking data access
	virtual bool GetTrackerLocation(const FString& devId, const uint8 tr, FVector& location) const override;
	virtual bool GetTrackerQuat(const FString& devId, const uint8 tr, FQuat& rotation) const override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterInputManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void Update() override;

	virtual void ExportInputData(FDisplayClusterMessage::DataType& data) const override;
	virtual void ImportInputData(const FDisplayClusterMessage::DataType& data) override;

private:
	typedef TUniquePtr<IDisplayClusterInputDevice>    TDevice;
	typedef TMap<FString, TDevice>         TDeviceClassMap;
	typedef TMap<int, TDeviceClassMap>     TDeviceMap;

	bool InitDevices();
	void ReleaseDevices();
	void UpdateInputDataCache();

	// Device data
	bool GetAxisData   (const FString& devId, const uint8 channel, FDisplayClusterVrpnAnalogChannelData&  data) const;
	bool GetButtonData (const FString& devId, const uint8 channel, FDisplayClusterVrpnButtonChannelData&  data) const;
	bool GetKeyboardData(const FString& devId, const uint8 channel, FDisplayClusterVrpnKeyboardChannelData&  data) const;
	bool GetTrackerData(const FString& devId, const uint8 channel, FDisplayClusterVrpnTrackerChannelData& data) const;

private:
	// Input devices
	TDeviceMap Devices;
	// Input state data cache
	FDisplayClusterMessage::DataType PackedTransferData;
	// Current config path
	FString ConfigPath;
	// Current cluster node ID
	FString ClusterNodeId;
	// Current world
	UWorld* CurrentWorld;

	mutable FCriticalSection InternalsSyncScope;

private:
	template<int DevTypeID>
	uint32 GetDeviceAmount_impl() const;

	template<int DevTypeID>
	bool GetDeviceIds_impl(TArray<FString>& ids) const;

	template<int DevTypeID>
	bool GetChannelData_impl(const FString& devId, const uint8 channel, typename display_cluster_input_device_traits<DevTypeID>::dev_channel_data_type& data) const;

private:
	static constexpr auto SerializationDeviceTypeNameDelimiter = TEXT(" ");
};

