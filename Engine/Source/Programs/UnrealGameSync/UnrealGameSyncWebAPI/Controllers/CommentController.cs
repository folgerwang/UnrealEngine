// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Web.Http;
using UnrealGameSyncWebAPI.Connectors;
using UnrealGameSyncWebAPI.Models;

namespace UnrealGameSyncWebAPI.Controllers
{
	public class CommentController : ApiController
	{
		public List<CommentData> Get(string Project, long LastCommentId)
		{
			return SqlConnector.GetComments(Project, LastCommentId);
		}
		public void Post([FromBody] CommentData Comment)
		{
			SqlConnector.PostComment(Comment);
		}
	}
}