// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace nDisplayLauncher.Cluster.Config.Conversion.Converters
{
	interface IConfigConverter
	{
		bool Convert(string FileOld, string FileNew);
	}
}
