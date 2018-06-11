// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace nDisplayLauncher.Config
{
	class DynamicCamera : Camera
	{
		public BaseInput tracker { get; set; }
		public string trackerChannel { get; set; }
	}
}
