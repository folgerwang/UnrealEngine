// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using nDisplayLauncher.Log;
using nDisplayLauncher.Cluster.Config.Conversion.Converters;


namespace nDisplayLauncher.Cluster.Config.Conversion
{
	public static class ConfigConversion
	{
		public class ConversionResult
		{
			public bool   Success       = false;
			public string NewConfigFile = string.Empty;
		}

		public static ConversionResult Convert(string OrigFile, ConfigurationVersion ToVerison)
		{
			ConversionResult Result = new ConversionResult();

			ConfigurationVersion Ver = Parser.GetVersion(OrigFile);

			try
			{
				string OldFile = OrigFile;
				for (int i = (int)Ver + 1; i <= (int)Launcher.CurrentVersion; ++i)
				{
					Result.NewConfigFile = OldFile.Insert(OldFile.Length - ".cfg".Length, "." + ((ConfigurationVersion)i).ToString());

					IConfigConverter Converter = ConverterFactory.CreateConverter((ConfigurationVersion)i);
					if (Converter == null)
					{
						AppLogger.Log("Unknown config format");
						return Result;
					}

					Result.Success = Converter.Convert(OldFile, Result.NewConfigFile);
					if (!Result.Success)
					{
						throw new Exception(string.Format("Couldn't convert\n%s\n\to\n%s", OldFile, Result.NewConfigFile));
					}

					OldFile = Result.NewConfigFile;
				}
			}
			catch (Exception ex)
			{
				AppLogger.Log("Conversion error: " + ex.Message);
			}

			return Result;
		}
	}
}
