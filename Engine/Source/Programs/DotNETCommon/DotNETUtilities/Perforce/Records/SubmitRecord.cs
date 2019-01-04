// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Contains information about a submitted changelist
	/// </summary>
	public class SubmitRecord
	{
		/// <summary>
		/// Submitted changelist number
		/// </summary>
		[PerforceTag("change", Optional = true)]
		public int ChangeNumber = -1;

		/// <summary>
		/// Format this record for display in the debugger
		/// </summary>
		/// <returns>Summary of this revision</returns>
		public override string ToString()
		{
			return ChangeNumber.ToString();
		}
	}
}
