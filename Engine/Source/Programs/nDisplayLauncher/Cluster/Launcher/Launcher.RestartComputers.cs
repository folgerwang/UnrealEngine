// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

using nDisplayLauncher.Cluster.Config;
using nDisplayLauncher.Cluster.Config.Entity;


namespace nDisplayLauncher.Cluster
{
	public partial class Launcher
	{
		private void ProcessCommandRestartComputers(Configuration Config)
		{
			HashSet<string> NodesSent = new HashSet<string>();

			foreach (EntityClusterNode Node in Config.ClusterNodes.Values)
			{
				if (!NodesSent.Contains(Node.Addr))
				{
					NodesSent.Add(Node.Addr);
					SendDaemonCommand(Node.Addr, DefaultListenerPort, CommandRestart);
				}
			}
		}
	}
}
