// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Generic class for parsing "info" responses from Perforce
	/// </summary>
	public class PerforceInfo
	{
		/// <summary>
		/// The severity level
		/// </summary>
		[PerforceTag("level")]
		public int Level;

		/// <summary>
		/// Message data
		/// </summary>
		[PerforceTag("data")]
		public string Data;

		/// <summary>
		/// Formats this error for display in the debugger
		/// </summary>
		/// <returns>String representation of this object</returns>
		public override string ToString()
		{
			return Data;
		}
	}
}
