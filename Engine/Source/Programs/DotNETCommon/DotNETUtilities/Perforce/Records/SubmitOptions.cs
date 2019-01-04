// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Options for the 'submit' command
	/// </summary>
	[Flags]
	public enum SubmitOptions
	{
		/// <summary>
		/// No options specified
		/// </summary>
		None = 0,

		/// <summary>
		/// Reopen files for edit in the default changelist after submission.
		/// </summary>
		ReopenAsEdit = 1,
	}
}
