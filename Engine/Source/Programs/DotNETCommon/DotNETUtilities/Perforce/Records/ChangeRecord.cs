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
	public class ChangeRecord
	{
		/// <summary>
		/// The changelist number. -1 for "new".
		/// </summary>
		[PerforceTag("Change")]
		public int Number = -1;

		/// <summary>
		/// The date the change was last modified
		/// </summary>
		[PerforceTag("Date", Optional = true)]
		public DateTime Date;

		/// <summary>
		/// The user that owns or submitted the change
		/// </summary>
		[PerforceTag("User")]
		public string User;

		/// <summary>
		/// The client that owns the change
		/// </summary>
		[PerforceTag("Client")]
		public string Client;

		/// <summary>
		/// Current changelist status
		/// </summary>
		[PerforceTag("Status")]
		public ChangeStatus Status;

		/// <summary>
		/// Whether the change is restricted or not
		/// </summary>
		[PerforceTag("Type")]
		public ChangeType Type;

		/// <summary>
		/// Description for the changelist
		/// </summary>
		[PerforceTag("Description")]
		public string Description;

		/// <summary>
		/// Files that are open in this changelist
		/// </summary>
		[PerforceTag("Files", Optional = true)]
		public List<string> Files = new List<string>();
	}
}
