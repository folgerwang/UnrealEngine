// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
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
		private void ProcessCommandStopListeners(List<ClusterNode> ClusterNodes, bool bQuiet=false)
		{
			HashSet<string> NodesSent = new HashSet<string>();

			foreach (ClusterNode Node in ClusterNodes)
			{
				if (!NodesSent.Contains(Node.address))
				{
					NodesSent.Add(Node.address);

					string appPath = "wmic";
					string argList = string.Format("/node:\"{0}\" process where name=\"{1}\" call terminate", Node.address, ListenerAppName);

					SpawnRemoteProcess("Stop Listener", Node.address, appPath, argList, bQuiet);
				}
			}
		}
	}
}
