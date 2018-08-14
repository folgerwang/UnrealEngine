// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Web.Http;
using UnrealGameSyncMetadataServer.Connectors;
using UnrealGameSyncMetadataServer.Models;

namespace UnrealGameSyncMetadataServer.Controllers
{
    public class LatestController : ApiController
    {
		public LatestData Get(string Project = null)
		{
			return SqlConnector.GetLastIds(Project);
		}
	}
}
