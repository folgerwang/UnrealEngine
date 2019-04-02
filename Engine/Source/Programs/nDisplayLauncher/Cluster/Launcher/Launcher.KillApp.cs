// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

using nDisplayLauncher.Cluster.Config;
using nDisplayLauncher.Cluster.Config.Entity;


namespace nDisplayLauncher.Cluster
{
	public partial class Launcher
	{
		private void ProcessCommandKillApp(Configuration Config)
		{
			string commandCmd = CommandKillApp;
			HashSet<string> NodesSent = new HashSet<string>();

			foreach (EntityClusterNode Node in Config.ClusterNodes.Values)
			{
				if (!NodesSent.Contains(Node.Addr))
				{
					SendDaemonCommand(Node.Addr, DefaultListenerPort, commandCmd);
					NodesSent.Add(Node.Addr);
				}
			}
		}
	}
}
