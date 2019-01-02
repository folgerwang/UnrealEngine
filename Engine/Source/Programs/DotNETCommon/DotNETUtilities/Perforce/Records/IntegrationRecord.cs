// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Stores integration information for a file revision
	/// </summary>
	public class IntegrationRecord
	{
		/// <summary>
		/// The integration action performed for this file
		/// </summary>
		[PerforceTag("how")]
		public IntegrateAction Action;

		/// <summary>
		/// The partner file for this integration
		/// </summary>
		[PerforceTag("file")]
		public string OtherFile;

		/// <summary>
		/// Min revision of the partner file for this integration
		/// </summary>
		[PerforceTag("srev")]
		public int StartRevisionNumber;

		/// <summary>
		/// Max revision of the partner file for this integration
		/// </summary>
		[PerforceTag("erev")]
		public int EndRevisionNumber;

		/// <summary>
		/// Summarize this record for display in the debugger
		/// </summary>
		/// <returns>Formatted integration record</returns>
		public override string ToString()
		{
			if(StartRevisionNumber + 1 == EndRevisionNumber)
			{
				return String.Format("{0} {1}#{2}", Action, OtherFile, EndRevisionNumber);
			}
			else
			{
				return String.Format("{0} {1}#{2},#{3}", Action, OtherFile, StartRevisionNumber + 1, EndRevisionNumber);
			}
		}
	}
}
