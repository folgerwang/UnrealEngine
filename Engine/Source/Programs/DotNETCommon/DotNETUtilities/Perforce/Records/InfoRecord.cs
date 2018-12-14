// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Contains information about the current client and server configuration
	/// </summary>
	public class InfoRecord
	{
		/// <summary>
		/// The current user according to the Perforce environment
		/// </summary>
		[PerforceTag("userName", Optional = true)]
		public string UserName;

		/// <summary>
		/// The current client
		/// </summary>
		[PerforceTag("clientName", Optional = true)]
		public string ClientName;

		/// <summary>
		/// The current host
		/// </summary>
		[PerforceTag("clientHost", Optional = true)]
		public string ClientHost;

		/// <summary>
		/// Root directory for the current client
		/// </summary>
		[PerforceTag("clientRoot", Optional = true)]
		public string ClientRoot;

		/// <summary>
		/// Selected stream in the current client
		/// </summary>
		[PerforceTag("clientStream", Optional = true)]
		public string ClientStream;

		/// <summary>
		/// Address of the Perforce server
		/// </summary>
		[PerforceTag("serverAddress", Optional = true)]
		public string ServerAddress;

		/// <summary>
		/// Case handling setting on the server
		/// </summary>
		[PerforceTag("caseHandling", Optional = true)]
		public string CaseHandling;
	}
}
