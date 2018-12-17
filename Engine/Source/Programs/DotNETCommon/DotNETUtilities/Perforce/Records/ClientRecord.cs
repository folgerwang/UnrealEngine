// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Represents a Perforce clientspec
	/// </summary>
	public class ClientRecord
	{
		/// <summary>
		/// The client workspace name, as specified in the P4CLIENT environment variable or its equivalents.
		/// </summary>
		[PerforceTag("Client")]
		public string Name;

		/// <summary>
		/// The Perforce user name of the user who owns the workspace.
		/// </summary>
		[PerforceTag("Owner")]
		public string Owner;

		/// <summary>
		/// The date the workspace specification was last modified.
		/// </summary>
		[PerforceTag("Update", Optional = true)]
		public DateTime Update;

		/// <summary>
		/// The date and time that the workspace was last used in any way.
		/// </summary>
		[PerforceTag("Access", Optional = true)]
		public DateTime Access;

		/// <summary>
		/// The name of the workstation on which this workspace resides.
		/// </summary>
		[PerforceTag("Host", Optional = true)]
		public string Host;

		/// <summary>
		/// A textual description of the workspace.
		/// </summary>
		[PerforceTag("Description", Optional = true)]
		public string Description;

		/// <summary>
		/// The directory (on the local host) relative to which all the files in the View: are specified. 
		/// </summary>
		[PerforceTag("Root")]
		public string Root;

		/// <summary>
		/// A set of seven switches that control particular workspace options.
		/// </summary>
		[PerforceTag("Options")]
		public ClientOptions Options;

		/// <summary>
		/// Options to govern the default behavior of p4 submit.
		/// </summary>
		[PerforceTag("SubmitOptions")]
		public ClientSubmitOptions SubmitOptions;

		/// <summary>
		/// Configure carriage-return/linefeed (CR/LF) conversion. 
		/// </summary>
		[PerforceTag("LineEnd")]
		public ClientLineEndings LineEnd;

		/// <summary>
		/// Associates the workspace with the specified stream.
		/// </summary>
		[PerforceTag("Stream", Optional = true)]
		public string Stream;

		/// <summary>
		/// The type of client.
		/// </summary>
		[PerforceTag("Type", Optional = true)]
		public string Type;

		/// <summary>
		/// Specifies the mappings between files in the depot and files in the workspace.
		/// </summary>
		[PerforceTag("View")]
		public List<string> View = new List<string>();
	}
}
