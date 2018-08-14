// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace UnrealGameSyncMetadataServer.Models
{
	public class LatestData
	{
		public long LastEventId { get; set; }
		public long LastCommmentId { get; set; }
		public long LastBuildId { get; set; }
	}
}