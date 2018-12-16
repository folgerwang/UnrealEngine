// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Options for the p4 info command
	/// </summary>
	[Flags]
	public enum InfoOptions
	{
		/// <summary>
		/// No addiional options
		/// </summary>
		None = 0,

		/// <summary>
		/// Shortened output; exclude information that requires a database lookup.
		/// </summary>
		ShortOutput = 1,
	}
}
