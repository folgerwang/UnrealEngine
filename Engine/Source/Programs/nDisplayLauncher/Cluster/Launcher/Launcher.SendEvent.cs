// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;

using nDisplayLauncher.Cluster.Config;
using nDisplayLauncher.Cluster.Config.Entity;
using nDisplayLauncher.Cluster.Events;
using nDisplayLauncher.Log;


namespace nDisplayLauncher.Cluster
{
	public partial class Launcher
	{
		private void ProcessCommandSendClusterEvent(Configuration Config, List<ClusterEvent> Events)
		{
			if (!File.Exists(SelectedConfig))
			{
				AppLogger.Log("No config file found: " + SelectedConfig);
				return;
			}

			EntityClusterNode MasterNode = Config.GetMasterNode();
			if (MasterNode == null)
			{
				AppLogger.Log("Master node not found");
				return;
			}

			if (Events == null || Events.Count == 0)
			{
				AppLogger.Log("Nothing to send");
				return;
			}

			foreach (ClusterEvent Event in Events)
			{
				SendClusterCommand(MasterNode.Addr, MasterNode.PortCE, Event.JsonData);
			}
		}
	}
}
