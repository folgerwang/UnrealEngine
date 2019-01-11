// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;

using nDisplayLauncher.Cluster.Config.Entity;


namespace nDisplayLauncher.Cluster.Config
{
	public class Configuration
	{
		public Dictionary<string, EntityClusterNode> ClusterNodes { get; set; } = new Dictionary<string, EntityClusterNode>();
		public Dictionary<string, EntityWindow>      Windows { get; set; } = new Dictionary<string, EntityWindow>();


		public Configuration()
		{
		}

		public EntityClusterNode GetMasterNode()
		{
			return ClusterNodes.Values.First(x => x.IsMaster);
		}
	}
}
