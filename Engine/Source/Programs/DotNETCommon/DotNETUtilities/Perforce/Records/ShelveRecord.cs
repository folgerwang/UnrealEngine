// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Contains information about a shelved file
	/// </summary>
	public class ShelveRecord
	{
		/// <summary>
		/// The changelist containing the shelved file
		/// </summary>
		[PerforceTag("change", Optional = true)]
		public int Change;

		/// <summary>
		/// Path to the file in the depot
		/// </summary>
		[PerforceTag("depotFile")]
		public string DepotFile;

		/// <summary>
		/// The revision number of the file that was shelved
		/// </summary>
		[PerforceTag("rev")]
		public int Revision;

		/// <summary>
		/// The action to be applied to the file
		/// </summary>
		[PerforceTag("action")]
		public string Action;

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
