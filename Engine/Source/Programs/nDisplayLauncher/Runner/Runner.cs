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
	public partial class Runner : INotifyPropertyChanged
	{
		// net
		public static int DefaultListenerPort = 41000;
		public const string ArgListenerPort   = "listener_port=";
		private const string ListenerAppName  = "nDisplayListener.exe";

		// cluster commands
		private const string CommandStartApp = "start";
		private const string CommandKillApp  = "kill";
		private const string CommandStatus   = "status";

		// run application params\keys
		private const string ArgMandatory = "-dc_cluster -nosplash -fixedseed";

		private const string ArgConfig = "dc_cfg";
		private const string ArgCamera = "dc_camera";
		private const string ArgNode   = "dc_node";

		// switches
		private const string ArgFullscreen           = "-fullscreen";
		private const string ArgWindowed             = "-windowed";
		private const string ArgNoTextureStreaming   = "-notexturestreaming";
		private const string ArgUseAllAvailableCores = "-useallavailablecores";

		public enum ClusterCommandType
		{
			RunApp,
			KillApp,
			ListenersStatus,
			StartListeners,
			StopListeners,
			DeployApp
		}


		#region Properties
		private Dictionary<string, string> _RenderApiParams = new Dictionary<string, string>
		{
			{"OpenGL3",    "-opengl3" },
			{"OpenGL4",    "-opengl4" },
			{"DirectX 11", "-dx11" },
			{"DirectX 12", "-dx12" }
		};
		public Dictionary<string, string> RenderApiParams
		{
			get { return _RenderApiParams; }
			set { Set(ref _RenderApiParams, value, "renderApiParams"); }
		}

		//Selected OpenGL parameter
		private KeyValuePair<string, string> _SelectedRenderApiParam;
		public KeyValuePair<string, string> SelectedRenderApiParam
		{
			get { return _SelectedRenderApiParam; }
			set
			{
				Set(ref _SelectedRenderApiParam, value, "selectedRenderApiParam");
				RegistrySaver.UpdateRegistry(RegistrySaver.RegParamsList, RegistrySaver.RegRenderApiName, value.Key);
			}
		}

		private Dictionary<string, string> _RenderModeParams = new Dictionary<string, string>
		{
			{"Mono",             "-dc_dev_mono" },
			{"Frame sequential", "-quad_buffer_stereo" },
			{"Side-by-side",     "-dc_dev_side_by_side" },
			{"Top-bottom",       "-dc_dev_top_bottom" }
		};
		public Dictionary<string, string> RenderModeParams
		{
			get { return _RenderModeParams; }
			set { Set(ref _RenderModeParams, value, "renderModeParams"); }
		}

		private KeyValuePair<string, string> _SelectedRenderModeParam;
		public KeyValuePair<string, string> SelectedRenderModeParam
		{
			get { return _SelectedRenderModeParam; }
			set
			{
				Set(ref _SelectedRenderModeParam, value, "selectedRenderModeParam");
				RegistrySaver.UpdateRegistry(RegistrySaver.RegParamsList, RegistrySaver.RegRenderModeName, value.Key);
			}
		}

		private bool _IsUseAllCores;
		public bool IsUseAllCores
		{
			get { return _IsUseAllCores; }
			set
			{
				Set(ref _IsUseAllCores, value, "isUseAllCores");
				RegistrySaver.UpdateRegistry(RegistrySaver.RegParamsList, RegistrySaver.RegIsAllCoresName, value);
			}
		}

		private bool _IsNotextureStreaming;
		public bool IsNotextureStreaming
		{
			get { return _IsNotextureStreaming; }
			set
			{
				Set(ref _IsNotextureStreaming, value, "isNotextureStreaming");
				RegistrySaver.UpdateRegistry(RegistrySaver.RegParamsList, RegistrySaver.RegIsNoTextureStreamingName, value);
			}
		}

		//Applications list
		private List<string> _Applications;
		public List<string> Applications
		{
			get { return _Applications; }
			set { Set(ref _Applications, value, "applications"); }
		}

		//Configs list
		private List<string> _Configs;
		public List<string> Configs
		{
			get { return _Configs; }
			set { Set(ref _Configs, value, "configs"); }
		}

		//Cameras list
		private List<string> _Cameras = new List<string>()
		{
			"camera_static",
			"camera_dynamic",
			"camera_custom_1",
			"camera_custom_2",
			"camera_custom_3",
			"camera_custom_4",
			"camera_custom_5",
			"camera_custom_6",
			"camera_custom_7",
			"camera_custom_8",
			"camera_custom_9"
		};
		public List<string> Cameras
		{
			get { return _Cameras; }
			set { Set(ref _Cameras, value, "cameras"); }
		}

		//Additional command line params
		private string _CustomCommonParams;
		public string CustomCommonParams
		{
			get { return _CustomCommonParams; }
			set
			{
				Set(ref _CustomCommonParams, value, "additionalParams");
				RegistrySaver.UpdateRegistry(RegistrySaver.RegParamsList, RegistrySaver.RegAdditionalParamsName, value);
			}
		}

		//Command line string with app path
		private string _CommandLine;
		public string CommandLine
		{
			get { return _CommandLine; }
			set { Set(ref _CommandLine, value, "commandLine"); }
		}

		//Selected Application
		private string _SelectedApplication;
		public string SelectedApplication
		{
			get { return _SelectedApplication; }
			set
			{
				Set(ref _SelectedApplication, value, "selectedApplication");
			}
		}

		//Selected Config file
		private string _SelectedConfig;
		public string SelectedConfig
		{
			get { return _SelectedConfig; }
			set
			{
				Set(ref _SelectedConfig, value, "selectedConfig");
				//runningConfig = new Config();
				//Parser.Parse(_selectedConfig, runningConfig);
				//SetSelectedConfig();
			}
		}

#endregion

		public Runner()
		{
			InitializeOptions();
			InitializeConfigLists();
		}

		//Implementation of INotifyPropertyChanged method for TwoWay binding
		public event PropertyChangedEventHandler PropertyChanged;

		protected void OnNotifyPropertyChanged(string propertyName)
		{
			if (PropertyChanged != null)
				PropertyChanged(this, new PropertyChangedEventArgs(propertyName));
		}

		//Set property with OnNotifyPropertyChanged call
		protected void Set<T>(ref T field, T newValue, string propertyName)
		{
			field = newValue;
			OnNotifyPropertyChanged(propertyName);
		}

		//Reloading all config lists
		private void InitializeConfigLists()
		{
			Applications = RegistrySaver.ReadStringsFromRegistry(RegistrySaver.RegAppList);
			SetSelectedApp();
			AppLogger.Add("Applications loaded successfully");
			Configs = RegistrySaver.ReadStringsFromRegistry(RegistrySaver.RegConfigList);
			SetSelectedConfig();
			AppLogger.Add("Configs loaded successfully");
			AppLogger.Add("List of Active nodes loaded successfully");
		}

		private void SetSelectedApp()
		{
			SelectedApplication = string.Empty;
			string selected = RegistrySaver.FindSelectedRegValue(RegistrySaver.RegAppList);
			if (!string.IsNullOrEmpty(selected))
			{
				SelectedApplication = Applications.Find(x => x == selected);
			}
		}

		public void SetSelectedConfig()
		{
			SelectedConfig = string.Empty;
			string selected = RegistrySaver.FindSelectedRegValue(RegistrySaver.RegConfigList);
			if (!string.IsNullOrEmpty(selected))
			{
				SelectedConfig = Configs.Find(x => x == selected);
			}
		}

		private void InitializeOptions()
		{
			try
			{
				SelectedRenderApiParam = RenderApiParams.First(x => x.Key == RegistrySaver.ReadStringValue(RegistrySaver.RegParamsList, RegistrySaver.RegRenderApiName));
			}
			catch (Exception)
			{
				SelectedRenderApiParam = RenderApiParams.SingleOrDefault(x => x.Key == "OpenGL3");
			}

			try
			{
				SelectedRenderModeParam = RenderModeParams.First(x => x.Key == RegistrySaver.ReadStringValue(RegistrySaver.RegParamsList, RegistrySaver.RegRenderModeName));
			}
			catch (Exception)
			{
				SelectedRenderApiParam = RenderModeParams.SingleOrDefault(x => x.Key == "Mono");
			}

			CustomCommonParams = RegistrySaver.ReadStringValue(RegistrySaver.RegParamsList, RegistrySaver.RegAdditionalParamsName);
			IsUseAllCores = RegistrySaver.ReadBoolValue(RegistrySaver.RegParamsList, RegistrySaver.RegIsAllCoresName);
			IsNotextureStreaming = RegistrySaver.ReadBoolValue(RegistrySaver.RegParamsList, RegistrySaver.RegIsNoTextureStreamingName);
			AppLogger.Add("Application Options initialized");
		}

		private List<ClusterNode> GetClusterNodes()
		{
			VRConfig runningConfig = new VRConfig();
			return Parser.Parse(SelectedConfig, runningConfig).clusterNodes;
		}

		public void ProcessCommand(ClusterCommandType Cmd)
		{
			List<ClusterNode> ClusterNodes = GetClusterNodes();

			if (ClusterNodes.Count < 1)
			{
				AppLogger.Add("No cluster nodes found in the config file");
				return;
			}

			switch (Cmd)
			{
				case ClusterCommandType.RunApp:
					ProcessCommandStartApp(ClusterNodes);
					break;

				case ClusterCommandType.KillApp:
					ProcessCommandKillApp(ClusterNodes);
					break;

				case ClusterCommandType.StartListeners:
					ProcessCommandStopListeners(ClusterNodes, true);
					ProcessCommandStartListeners(ClusterNodes);
					break;

				case ClusterCommandType.StopListeners:
					ProcessCommandStopListeners(ClusterNodes);
					break;

				case ClusterCommandType.ListenersStatus:
					ProcessCommandStatusListeners(ClusterNodes);
					break;

				case ClusterCommandType.DeployApp:
					ProcessCommandDeployApp(ClusterNodes);
					break;

				default:
					break;
			}
		}

		private int SendDaemonCommand(string nodeAddress, string cmd, bool bQuiet = false)
		{
			int ResponseCode = 1;
			TcpClient nodeClient = new TcpClient();

			if (!bQuiet)
			{
				AppLogger.Add(string.Format("Sending command {0} to {1}...", cmd, nodeAddress));
			}

			try
			{
				// Connect to the listener
				nodeClient.Connect(nodeAddress, DefaultListenerPort);
				NetworkStream networkStream = nodeClient.GetStream();

				byte[] OutData = System.Text.Encoding.ASCII.GetBytes(cmd);
				networkStream.Write(OutData, 0, OutData.Length);

				byte[] InData = new byte[1024];
				int InBytesCount = networkStream.Read(InData, 0, InData.Length);

				// Receive response
				ResponseCode = BitConverter.ToInt32(InData, 0);
			}
			catch (Exception ex)
			{
				if (!bQuiet)
				{
					AppLogger.Add("An error occurred while sending a command to " + nodeAddress + ". EXCEPTION: " + ex.Message);
				}
			}
			finally
			{
				nodeClient.Close();
			}

			return ResponseCode;
		}

		public void AddApplication(string appPath)
		{
			if (!Applications.Contains(appPath))
			{
				Applications.Add(appPath);
				RegistrySaver.AddRegistryValue(RegistrySaver.RegAppList, appPath);
				AppLogger.Add("Application [" + appPath + "] added to list");
			}
			else
			{
				AppLogger.Add("WARNING! Application [" + appPath + "] is already in the list");
			}

		}

		public void DeleteApplication()
		{
			Applications.Remove(SelectedApplication);
			RegistrySaver.RemoveRegistryValue(RegistrySaver.RegAppList, SelectedApplication);
			AppLogger.Add("Application [" + SelectedApplication + "] deleted");

			SelectedApplication = null;
		}

		public void ChangeConfigSelection(string configPath)
		{
			try
			{
				foreach (string config in Configs)
				{
					if (config != configPath)
					{
						RegistrySaver.UpdateRegistry(RegistrySaver.RegConfigList, config, false);
					}
					else
					{
						RegistrySaver.UpdateRegistry(RegistrySaver.RegConfigList, config, true);
					}
				}
			}
			catch (Exception exception)
			{
				AppLogger.Add("ERROR while changing config selection. EXCEPTION: " + exception.Message);
			}
		}

		public void AddConfig(string configPath)
		{
			try
			{
				Configs.Add(configPath);
				SelectedConfig = Configs.Find(x => x == configPath);
				RegistrySaver.AddRegistryValue(RegistrySaver.RegConfigList, configPath);
				ChangeConfigSelection(configPath);
				AppLogger.Add("Configuration file [" + configPath + "] added to list");
			}
			catch (Exception)
			{
				AppLogger.Add("ERROR! Can not add configuration file [" + configPath + "] to list");
			}
		}

		public void DeleteConfig()
		{
			Configs.Remove(SelectedConfig);
			RegistrySaver.RemoveRegistryValue(RegistrySaver.RegConfigList, SelectedConfig);
			AppLogger.Add("Configuration file [" + SelectedConfig + "] deleted");
			SelectedConfig = Configs.FirstOrDefault();
		}
	}
}
