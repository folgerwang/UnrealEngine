// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Contains summary information about a changelist
	/// </summary>
	public class ChangesRecord
	{
		/// <summary>
		/// The changelist number
		/// </summary>
		[PerforceTag("change")]
		public int Number;

		/// <summary>
		/// The date the change was last modified
		/// </summary>
		[PerforceTag("time")]
		public DateTime Time;

		/// <summary>
		/// The user that owns or submitted the change
		/// </summary>
		[PerforceTag("user")]
		public string User;

		/// <summary>
		/// The client that owns the change
		/// </summary>
		[PerforceTag("client")]
		public string Client;

		/// <summary>
		/// Current changelist status
		/// </summary>
		[PerforceTag("status")]
		public ChangeStatus Status;

		/// <summary>
		/// Whether the change is restricted or not
		/// </summary>
		[PerforceTag("changeType")]
		public ChangeType Type;

		/// <summary>
		/// The path affected by this change.
		/// </summary>
		[PerforceTag("path", Optional = true)]
		public string Path;

		/// <summary>
		/// Description for the changelist
		/// </summary>
		[PerforceTag("desc")]
		public string Description;
	}
}
