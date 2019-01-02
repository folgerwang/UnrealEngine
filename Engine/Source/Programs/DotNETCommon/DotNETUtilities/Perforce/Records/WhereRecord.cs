// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Contains information about a file's location in the workspace
	/// </summary>
	public class WhereRecord
	{
		/// <summary>
		/// Path to the file in the depot
		/// </summary>
		[PerforceTag("depotFile")]
		public string DepotFile;

		/// <summary>
		/// Path to the file in client syntax
		/// </summary>
		[PerforceTag("clientFile")]
		public string ClientFile;

		/// <summary>
		/// Path to the file on dist
		/// </summary>
		[PerforceTag("path")]
		public string Path;

		/// <summary>
		/// Indicates that the given file or path is being unmapped from the workspace
		/// </summary>
		[PerforceTag("unmap", Optional = true)]
		public bool Unmap;

		/// <summary>
		/// Summarize this record for display in the debugger
		/// </summary>
		/// <returns>Formatted record</returns>
		public override string ToString()
		{
			return DepotFile;
		}
	}
}
