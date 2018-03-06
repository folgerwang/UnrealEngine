// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Action to take for precompiling binaries and modules
	/// </summary>
	enum PrecompileMode
	{
		/// <summary>
		/// Build the module normally
		/// </summary>
		None,

		/// <summary>
		/// Create precompiled build products
		/// </summary>
		Create,

		/// <summary>
		/// Use existing precompiled build products
		/// </summary>
		Use,
	}
}
