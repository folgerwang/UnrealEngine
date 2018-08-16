// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Web.Http;
using UnrealGameSyncMetadataServer.Connectors;
using UnrealGameSyncMetadataServer.Models;

namespace UnrealGameSyncMetadataServer.Controllers
{
    public class BuildController : ApiController
    {
		public List<BuildData> Get(string Project, long LastBuildId)
		{
			return SqlConnector.GetBuilds(Project, LastBuildId);
		}
		public void Post([FromBody]BuildData Build)
		{
			SqlConnector.PostBuild(Build);
		}
	}
}
