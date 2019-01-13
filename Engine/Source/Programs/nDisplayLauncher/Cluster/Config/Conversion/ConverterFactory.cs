// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using nDisplayLauncher.Cluster.Config.Conversion.Converters;

namespace nDisplayLauncher.Cluster.Config.Conversion
{
	class ConverterFactory
	{
		public static IConfigConverter CreateConverter(ConfigurationVersion Ver)
		{
			switch (Ver)
			{
				case ConfigurationVersion.Ver22:
					return new ConfigConverter_22();

				default:
					return null;
			}
		}
	}
}
