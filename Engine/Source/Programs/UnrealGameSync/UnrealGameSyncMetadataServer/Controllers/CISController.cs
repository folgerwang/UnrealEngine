// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Web.Http;
using UnrealGameSyncMetadataServer.Connectors;
using UnrealGameSyncMetadataServer.Models;

namespace UnrealGameSyncMetadataServer.Controllers
{
    public class CISController : ApiController
    {
		public long[] Get()
		{
			return SqlConnector.GetLastIds();
		}
		public List<BuildData> Get(string Project, long LastBuildId)
		{
			return SqlConnector.GetCIS(Project, LastBuildId);
		}
		public void Post([FromBody]BuildData Build)
		{
			SqlConnector.PostCIS(Build);
		}
	}
}
