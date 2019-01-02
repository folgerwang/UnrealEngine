// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Information about a synced file
	/// </summary>
	public class SyncRecord
	{
		/// <summary>
		/// Path to the file in the depot
		/// </summary>
		[PerforceTag("depotFile")]
		public string DepotFile;

		/// <summary>
		/// Path to the file in the workspace
		/// </summary>
		[PerforceTag("clientFile")]
		public string ClientFile;

		/// <summary>
		/// The revision number of the file that was synced
		/// </summary>
		[PerforceTag("rev")]
		public int Revision;

		/// <summary>
		/// Action taken when syncing the file
		/// </summary>
		[PerforceTag("action")]
		public string Action;

		/// <summary>
		/// Size of the file
		/// </summary>
		[PerforceTag("fileSize", Optional = true)]
		public long FileSize;

		/// <summary>
		/// Stores the total size of all files that are being synced (only set for the first sync record)
		/// </summary>
		[PerforceTag("totalFileSize", Optional = true)]
		public long TotalFileSize;

		/// <summary>
		/// Stores the total number of files that will be synced (only set for the first sync record)
		/// </summary>
		[PerforceTag("totalFileCount", Optional = true)]
		public long TotalFileCount;

		/// <summary>
		/// Change that modified the file
		/// </summary>
		[PerforceTag("change", Optional = true)]
		public int Change;

		/// <summary>
		/// Format this record for display in the debugger
		/// </summary>
		/// <returns>Summary of this revision</returns>
		public override string ToString()
		{
			return DepotFile;
		}
	}
}
