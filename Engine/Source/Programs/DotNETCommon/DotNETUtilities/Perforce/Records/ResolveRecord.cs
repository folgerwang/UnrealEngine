// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Information about a resolved file
	/// </summary>
	public class ResolveRecord
	{
		/// <summary>
		/// Path to the file in the workspace
		/// </summary>
		[PerforceTag("clientFile")]
		public string ClientFile;

		/// <summary>
		/// Path to the depot file that needs to be resolved
		/// </summary>
		[PerforceTag("fromFile")]
		public string FromFile;

		/// <summary>
		/// Start range of changes to be resolved
		/// </summary>
		[PerforceTag("startFromRev")]
		public int FromRevisionStart;

		/// <summary>
		/// Ending range of changes to be resolved
		/// </summary>
		[PerforceTag("endFromRev")]
		public int FromRevisionEnd;

		/// <summary>
		/// The type of resolve to perform
		/// </summary>
		[PerforceTag("resolveType")]
		public string ResolveType;

		/// <summary>
		/// For content resolves, the type of resolve to be performed
		/// </summary>
		[PerforceTag("contentResolveType", Optional = true)]
		public string ContentResolveType;

		/// <summary>
		/// Format this record for display in the debugger
		/// </summary>
		/// <returns>Summary of this revision</returns>
		public override string ToString()
		{
			return ClientFile;
		}
	}
}
