// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Information about a sync operation
	/// </summary>
	public class SyncSummaryRecord
	{
		/// <summary>
		/// The total size of all files synced
		/// </summary>
		[PerforceTag("totalFileSize")]
		public long TotalFileSize;

		/// <summary>
		/// The total number of files synced
		/// </summary>
		[PerforceTag("totalFileCount")]
		public long TotalFileCount;

		/// <summary>
		/// The changelist that was synced to
		/// </summary>
		[PerforceTag("change")]
		public int Change;
	}
}
