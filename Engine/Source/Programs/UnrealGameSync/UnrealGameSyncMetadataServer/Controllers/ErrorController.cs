// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.Web.Http;
using UnrealGameSyncMetadataServer.Connectors;
using UnrealGameSyncMetadataServer.Models;

namespace UnrealGameSyncMetadataServer.Controllers
{
    public class ErrorController : ApiController
    {
		public void Post([FromBody] TelemetryErrorData Data, string Version, string IpAddress)
		{
			SqlConnector.PostErrorData(Data, Version, IpAddress);
		}
    }
}
