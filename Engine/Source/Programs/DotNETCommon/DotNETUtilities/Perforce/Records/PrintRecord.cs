// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Record containing information about a printed file
	/// </summary>
	public class PrintRecord
	{
		/// <summary>
		/// Path to the file in the depot
		/// </summary>
		[PerforceTag("depotFile")]
		public string DepotFile;

		/// <summary>
		/// Revision number of the file
		/// </summary>
		[PerforceTag("rev")]
		public int Revision;

		/// <summary>
		/// Change number of the file
		/// </summary>
		[PerforceTag("change")]
		public int ChangeNumber;

		/// <summary>
		/// Last action to the file
		/// </summary>
		[PerforceTag("action")]
		public FileAction Action;

		/// <summary>
		/// File type
		/// </summary>
		[PerforceTag("type")]
		public string Type;

		/// <summary>
		/// Submit time of the file
		/// </summary>
		[PerforceTag("time")]
		public DateTime Time;

		/// <summary>
		/// Size of the file in bytes
		/// </summary>
		[PerforceTag("fileSize")]
		public long FileSize;
	}
}
