// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;

namespace Gauntlet
{
	/// <summary>
	/// Base class for a device that is able to run applications
	/// </summary>
	public interface ITargetDevice : IDisposable
	{
		/// <summary>
		/// Name of this device
		/// </summary>
		string Name { get; }

		/// <summary>
		/// Platform of this device
		/// </summary>
		UnrealTargetPlatform Platform { get; }

		/// <summary>
		/// Options used when running processes
		/// </summary>
		CommandUtils.ERunOptions RunOptions { get; set; }

		/// <summary>
		/// Is the device available for use?
		/// Note: If we are the process holding a connection then this should still return true
		/// </summary>
		bool IsAvailable { get; }

		/// <summary>
		/// Are we connected to this device?
		/// </summary>
		bool IsConnected { get; }

		/// <summary>
		/// Connect and reserve the device
		/// </summary>
		/// <returns></returns>
		bool Connect();

		/// <summary>
		/// Disconnect from the device
		/// </summary>
		/// <returns></returns>
		bool Disconnect();

		/// <summary>
		/// Is the device powered on?
		/// TODO - Do we need to have a way of expressing whether power changes are supported
		/// </summary>
		bool IsOn { get; }

		/// <summary>
		/// Request the device power on. Should block until the change succeeds (or fails)
		/// </summary>
		/// <returns></returns>
		bool PowerOn();

		/// <summary>
		/// Request the device power on. Should block until the change succeeds (or fails)
		/// </summary>
		/// <returns></returns>
		bool PowerOff();

		/// <summary>
		/// Request the device power on. Should block until the change succeeds (or fails)
		/// </summary>
		/// <returns></returns>
		bool Reboot();

		/// <summary>
		/// Returns a Dictionary of EIntendedBaseCopyDirectory keys and their corresponding file path string values.
		/// If a platform has not set up these mappings, returns an empty Dictionary and warns.
		/// </summary>
		/// <returns></returns>
		Dictionary<EIntendedBaseCopyDirectory, string> GetPlatformDirectoryMappings();

		IAppInstall InstallApplication(UnrealAppConfig AppConfiguration);

		IAppInstance Run(IAppInstall App);

	};


	/// <summary>
	/// Represents a class able to provide devices
	/// </summary>
	public interface IDeviceSource
	{
		bool CanSupportPlatform(UnrealTargetPlatform Platform);
	}

	/// <summary>
	/// Represents a class that provides locally available devices
	/// </summary>
	public interface IDefaultDeviceSource : IDeviceSource
	{
		ITargetDevice[] GetDefaultDevices();
	}

	/// <summary>
	/// Represents a class capable of creating devices of a specific type
	/// </summary>
	public interface IDeviceFactory : IDeviceSource
	{
		ITargetDevice CreateDevice(string InRef, string InParam);
	}
}