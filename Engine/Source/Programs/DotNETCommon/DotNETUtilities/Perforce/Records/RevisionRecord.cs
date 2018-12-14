// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Stores a revision record for a file
	/// </summary>
	public class RevisionRecord
	{
		/// <summary>
		/// The revision number of this file
		/// </summary>
		[PerforceTag("rev")]
		public readonly int RevisionNumber;

		/// <summary>
		/// The changelist responsible for this revision of the file
		/// </summary>
		[PerforceTag("change")]
		public readonly int ChangeNumber;

		/// <summary>
		/// Action performed to the file in this revision
		/// </summary>
		[PerforceTag("action")]
		public readonly FileAction Action;

		/// <summary>
		/// Type of the file
		/// </summary>
		[PerforceTag("type")]
		public readonly string Type;

		/// <summary>
		/// Timestamp of this modification
		/// </summary>
		[PerforceTag("time")]
		public readonly DateTime Time;

		/// <summary>
		/// Author of the changelist
		/// </summary>
		[PerforceTag("user")]
		public readonly string UserName;

		/// <summary>
		/// Client that submitted this changelist
		/// </summary>
		[PerforceTag("client")]
		public readonly string ClientName;

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
		/// Description of this changelist
		/// </summary>
		[PerforceTag("desc")]
		public string Description;

		/// <summary>
		/// Integration records for this revision
		/// </summary>
		[PerforceRecordList]
		public List<IntegrationRecord> Integrations = new List<IntegrationRecord>();

		/// <summary>
		/// Format this record for display in the debugger
		/// </summary>
		/// <returns>Summary of this revision</returns>
		public override string ToString()
		{
			return String.Format("#{0} change {1} {2} on {3} by {4}@{5}", RevisionNumber, ChangeNumber, Action, Time, UserName, ClientName);
		}
	}
}
