// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Information about a reverted file
	/// </summary>
	public class RevertRecord
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
		/// The revision number that is now in the workspace
		/// </summary>
		[PerforceTag("haveRev")]
		public int HaveRevision;

		/// <summary>
		/// The previous action that the file was opened as
		/// </summary>
		[PerforceTag("oldAction")]
		public string OldAction;

		/// <summary>
		/// Action taken to revert the file
		/// </summary>
		[PerforceTag("action")]
		public string Action;

		/// <summary>
		/// Format this record for display in the debugger
		/// </summary>
		/// <returns>Summary of this revert record</returns>
		public override string ToString()
		{
			return ClientFile;
		}
	}
}
