// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace nDisplayLauncher.Config
{
	public abstract class ConfigItem
	{
		public string validationName = "Object validation";

		public abstract string CreateCfg();
		public abstract bool Validate();
	}
}
