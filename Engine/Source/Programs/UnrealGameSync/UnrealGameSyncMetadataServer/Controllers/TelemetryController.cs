// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.Web.Http;
using UnrealGameSyncMetadataServer.Connectors;
using UnrealGameSyncMetadataServer.Models;

namespace UnrealGameSyncMetadataServer.Controllers
{
    public class TelemetryController : ApiController
    {
		public void Post([FromBody] TelemetryTimingData Data, string Version, string IpAddress)
		{
			SqlConnector.PostTelemetryData(Data, Version, IpAddress);
		}
    }
}
