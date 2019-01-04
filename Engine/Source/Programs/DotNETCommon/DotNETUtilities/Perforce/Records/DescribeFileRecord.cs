// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Contains information about an individual file in a returned DescribeRecord
	/// </summary>
	public class DescribeFileRecord
	{
		/// <summary>
		/// Path to the modified file in depot syntax
		/// </summary>
		[PerforceTag("depotFile")]
		public string DepotFile;

		/// <summary>
		/// The action performed on this file
		/// </summary>
		[PerforceTag("action")]
		public FileAction Action;

		/// <summary>
		/// The file type after this change
		/// </summary>
		[PerforceTag("type")]
		public string Type;

		/// <summary>
		/// The revision number for this file
		/// </summary>
		[PerforceTag("rev")]
		public int Revision;

		/// <summary>
		/// Size of the file, or -1 if not specified
		/// </summary>
		[PerforceTag("fileSize", Optional = true)]
		public readonly int FileSize;

		/// <summary>
		/// Digest of the file, or null if not specified
		/// </summary>
		[PerforceTag("digest", Optional = true)]
		public readonly string Digest;

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
