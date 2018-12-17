// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// The type of a changelist
	/// </summary>
	public enum ChangeType
	{
		/// <summary>
		/// When creating a new changelist, leaves the changelist type unspecified.
		/// </summary>
		Unspecified,

		/// <summary>
		/// This change is visible to anyone
		/// </summary>
		[PerforceEnum("public")]
		Public,

		/// <summary>
		/// This change is restricted
		/// </summary>
		[PerforceEnum("restricted")]
		Restricted,
	}
}
