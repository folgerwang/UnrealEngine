// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using nDisplayLauncher.Settings;
using nDisplayLauncher.Config;

namespace nDisplayLauncher
{
	public static class Parser
	{
		//Config file parser
		public static VRConfig Parse(string filePath, VRConfig currentConfig)
		{
			// refactoring needed
			List<string> inputLines = new List<string>();
			List<string> sceneNodeLines = new List<string>();
			List<string> screenLines = new List<string>();
			List<string> viewportLines = new List<string>();
			List<string> clusterNodeLines = new List<string>();
			List<string> cameraLines = new List<string>();
			List<string> generalLines = new List<string>();
			List<string> stereoLines = new List<string>();
			List<string> debugLines = new List<string>();
			try
			{
				foreach (string line in File.ReadLines(filePath))
				{
					if (line == string.Empty || line.First() == '#')
					{
						//Do nothing
					}
					else
					{
						if (line.ToLower().Contains("[input]"))
						{
							inputLines.Add(line);
						}
						if (line.ToLower().Contains("[scene_node]"))
						{
							sceneNodeLines.Add(line);
						}
						if (line.ToLower().Contains("[screen]"))
						{
							screenLines.Add(line);
						}
						if (line.ToLower().Contains("[viewport]"))
						{
							viewportLines.Add(line);
						}
						if (line.ToLower().Contains("[cluster_node]"))
						{
							clusterNodeLines.Add(line);
						}
						if (line.ToLower().Contains("[camera]"))
						{
							cameraLines.Add(line);
						}
						if (line.ToLower().Contains("[general]"))
						{
							generalLines.Add(line);
						}
						if (line.ToLower().Contains("[stereo]"))
						{
							stereoLines.Add(line);
						}
						if (line.ToLower().Contains("[debug]"))
						{
							debugLines.Add(line);
						}
						if (line.ToLower().Contains("[render]"))
						{
							//todo 
						}
						if (line.ToLower().Contains("[custom]"))
						{
							//todo 
						}
					}
				}
				foreach (string line in viewportLines)
				{
					currentConfig.ViewportParse(line);
				}
				foreach (string line in generalLines)
				{
					currentConfig.GeneralParse(line);
				}
				foreach (string line in stereoLines)
				{
					currentConfig.StereoParse(line);
				}
				foreach (string line in debugLines)
				{
					currentConfig.DebugParse(line);
				}
				foreach (string line in inputLines)
				{
					currentConfig.InputsParse(line);
				}
				foreach (string line in cameraLines)
				{
					currentConfig.CameraParse(line);
				}
				foreach (string line in sceneNodeLines)
				{
					currentConfig.SceneNodeParse(line);
				}
				foreach (string line in screenLines)
				{
					currentConfig.ScreenParse(line);
				}
				foreach (string line in clusterNodeLines)
				{
					currentConfig.ClusterNodeParse(line);
				}

				currentConfig.sceneNodesView = currentConfig.ConvertSceneNodeList(currentConfig.sceneNodes);
				currentConfig.name = Path.GetFileNameWithoutExtension(filePath);
				//AppLogger.Add("Config " + currentConfig.name + " loaded");
				RegistrySaver.AddRegistryValue(RegistrySaver.RegConfigName, filePath);

			}
			catch (FileNotFoundException)
			{
				AppLogger.Add("ERROR! Config " + currentConfig.name + "not found!");
			}
			catch (System.ArgumentException)
			{
				AppLogger.Add("ERROR! Config " + currentConfig.name + "not found!");
			}

			return currentConfig;
		}
	}
}
