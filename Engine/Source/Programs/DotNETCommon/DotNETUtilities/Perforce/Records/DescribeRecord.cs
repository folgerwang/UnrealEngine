// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Detailed description of an individual changelist
	/// </summary>
	public class DescribeRecord
	{
		/// <summary>
		/// The changelist number
		/// </summary>
		[PerforceTag("change")]
		public int Number;

		/// <summary>
		/// The user that owns or submitted the change
		/// </summary>
		[PerforceTag("user")]
		public string User;

		/// <summary>
		/// The workspace that contains or submitted the change
		/// </summary>
		[PerforceTag("client")]
		public string Client;

		/// <summary>
		/// Time at which the change was submitted
		/// </summary>
		[PerforceTag("time")]
		public DateTime Time;

		/// <summary>
		/// The changelist description
		/// </summary>
		[PerforceTag("desc")]
		public string Description;

		/// <summary>
		/// The change status (submitted, pending, etc...)
		/// </summary>
		[PerforceTag("status")]
		public ChangeStatus Status;

		/// <summary>
		/// Whether the change is restricted or not
		/// </summary>
		[PerforceTag("changeType")]
		public ChangeType Type;

		/// <summary>
		/// Narrowest path which contains all files affected by this change
		/// </summary>
		[PerforceTag("path", Optional = true)]
		public string Path;

		/// <summary>
		/// The files affected by this change
		/// </summary>
		[PerforceRecordList]
		public List<DescribeFileRecord> Files = new List<DescribeFileRecord>();

		/// <summary>
		/// Format this record for display in the debugger
		/// </summary>
		/// <returns>Summary of this revision</returns>
		public override string ToString()
		{
			return String.Format("{0}: {1}", Number, Description);
		}
	}
}
