// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Web.Http;
using UnrealGameSyncMetadataServer.Models;
using UnrealGameSyncMetadataServer.Connectors;

namespace UnrealGameSyncMetadataServer.Controllers
{
	public class EventController : ApiController
	{
		public List<EventData> Get(string Project, long LastEventId)
		{
			return SqlConnector.GetUserVotes(Project, LastEventId);
		}

		public void Post([FromBody] EventData Event)
		{
			SqlConnector.PostEvent(Event);
		}
	}
}