// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.Web.Http;
using UnrealGameSyncWebAPI.Connectors;
using UnrealGameSyncWebAPI.Models;

namespace UnrealGameSyncWebAPI.Controllers
{
    public class TelemetryController : ApiController
    {
		public void Post([FromBody] TelemetryTimingData Data, string Version, string IpAddress)
		{
			SqlConnector.PostTelemetryData(Data, Version, IpAddress);
		}
    }
}
