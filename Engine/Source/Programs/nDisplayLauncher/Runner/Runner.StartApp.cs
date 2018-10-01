// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using nDisplayLauncher.Config;
using nDisplayLauncher.Settings;


namespace nDisplayLauncher
{
	public partial class Runner
	{
		private void ProcessCommandStartApp(List<ClusterNode> ClusterNodes)
		{
			if (!File.Exists(SelectedConfig))
			{
				AppLogger.Add("No config file found: " + SelectedConfig);
				return;
			}

			if (!File.Exists(SelectedApplication))
			{
				AppLogger.Add("No application found: " + SelectedApplication);
				return;
			}

			// Update config files before application start
			HashSet<string> NodesSent = new HashSet<string>();
			foreach (ClusterNode Node in ClusterNodes)
			{
				if (!NodesSent.Contains(Node.address))
				{
					NodesSent.Add(Node.address);
					DeployFile(Path.GetFullPath(SelectedConfig), Path.GetDirectoryName(SelectedApplication), Node);
				}
			}

			// Send start command to the listeners
			foreach (ClusterNode Node in ClusterNodes)
			{
				string cmd = GenerateStartCommand(Node);
				SendDaemonCommand(Node.address, cmd);
			}
		}

		private string GenerateStartCommand(ClusterNode Node)
		{
			string commandCmd;

			// Executable
			commandCmd = string.Format("{0} \"{1}\"", CommandStartApp, SelectedApplication);
			// Custom common arguments
			if (!string.IsNullOrWhiteSpace(CustomCommonParams))
			{
				commandCmd = string.Format("{0} {1}", commandCmd, CustomCommonParams.Trim());
			}
			// Mandatory arguments
			commandCmd = string.Format("{0} {1}", commandCmd, ArgMandatory);
			// Config file
			commandCmd = string.Format("{0} {1}=\"{2}\"", commandCmd, ArgConfig, SelectedConfig);
			// Render API and mode
			commandCmd = string.Format("{0} {1} {2}", commandCmd, SelectedRenderApiParam.Value, SelectedRenderModeParam.Value);
			// Camera
			if (!string.IsNullOrWhiteSpace(Node.camera))
			{
				commandCmd = string.Format("{0} {1}={2}", commandCmd, ArgCamera, Node.camera);
			}

			// No texture streaming
			if (IsNotextureStreaming)
			{
				commandCmd = string.Format("{0} {1}", commandCmd, ArgNoTextureStreaming);
			}
			// Use all available cores
			if (IsUseAllCores)
			{
				commandCmd = string.Format("{0} {1}", commandCmd, ArgUseAllAvailableCores);
			}
			// Fullscreen/windowed
			commandCmd = string.Format("{0} {1}", commandCmd, Node.isWindowed ? ArgWindowed : ArgFullscreen);
			// Window mode, location and size
			commandCmd = string.Format("{0} {1} {2} {3} {4}", 
				commandCmd, 
				string.IsNullOrEmpty(Node.winX) ? string.Empty : string.Format("WinX={0}", Node.winX),
				string.IsNullOrEmpty(Node.winY) ? string.Empty : string.Format("WinY={0}", Node.winY),
				string.IsNullOrEmpty(Node.resX) ? string.Empty : string.Format("ResX={0}", Node.resX),
				string.IsNullOrEmpty(Node.resY) ? string.Empty : string.Format("ResY={0}", Node.resY));
			// Node ID
			commandCmd = string.Format("{0} {1}={2}", commandCmd, ArgNode, Node.id);

			return commandCmd;
		}
	}
}
