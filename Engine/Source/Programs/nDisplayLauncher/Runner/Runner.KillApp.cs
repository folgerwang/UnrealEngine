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
		private void ProcessCommandKillApp(List<ClusterNode> ClusterNodes)
		{
			string commandCmd = CommandKillApp;
			HashSet<string> NodesSent = new HashSet<string>();

			foreach (ClusterNode Node in ClusterNodes)
			{
				if (!NodesSent.Contains(Node.address))
				{
					SendDaemonCommand(Node.address, commandCmd);
					NodesSent.Add(Node.address);
				}
			}
		}
	}
}
