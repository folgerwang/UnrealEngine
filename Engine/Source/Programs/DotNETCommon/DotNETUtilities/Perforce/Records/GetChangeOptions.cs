// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Options for the GetChange command (p4 change -o)
	/// </summary>
	public enum GetChangeOptions
	{
		/// <summary>
		/// No options are specified
		/// </summary>
		None = 0,

		/// <summary>
		/// If a changelist was renumbered on submit, and you know only the original changelist number, use OriginalChangeNumber and the original changelist number to view or edit the changelist.
		/// </summary>
		BeforeRenumber = 1,
	}

}
