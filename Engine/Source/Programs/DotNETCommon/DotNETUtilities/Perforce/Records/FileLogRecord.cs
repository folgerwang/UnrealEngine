// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Record output by the filelog command
	/// </summary>
	public class FileLogRecord
	{
		/// <summary>
		/// Path to the file in the depot
		/// </summary>
		[PerforceTag("depotFile")]
		public string DepotPath;

		/// <summary>
		/// Revisions of this file
		/// </summary>
		[PerforceRecordList]
		public List<RevisionRecord> Revisions = new List<RevisionRecord>();

		/// <summary>
		/// Format this record for display in the debugger
		/// </summary>
		/// <returns>Summary of this revision</returns>
		public override string ToString()
		{
			return DepotPath;
		}
	}
}
