// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace nDisplayLauncher.Cluster.Config.Conversion.Converters
{
	class ConfigConverter_22 : ConfigConverterBase, IConfigConverter
	{
		List<KeyValuePair<ETokenType, string>> LinesToAdd = new List<KeyValuePair<ETokenType, string>>();

		public bool Convert(string FileOld, string FileNew)
		{
			return LoadContent(FileOld)
				&& AddInfoEntity()
				&& AddPortCeParameter()
				&& IntroduceWindowEntity()
				&& Finalize()
				&& SaveConfig(FileNew);
		}

		private bool AddInfoEntity()
		{
			if (ConfigLines.Exists(x => x.Key == ETokenType.Info))
			{
				// This entity shouldn't be here since it's introduced in v22
				return false;
			}

			LinesToAdd.Add(new KeyValuePair<ETokenType, string>(ETokenType.Other, "# AUTO_CONVERSION, new entities start"));
			LinesToAdd.Add(new KeyValuePair<ETokenType, string>(ETokenType.Info, "[info] version=22"));

			return true;
		}

		private bool AddPortCeParameter()
		{
			for (int i = 0; i < ConfigLines.Count; ++i)
			{
				if (ConfigLines[i].Key == ETokenType.ClusterNode)
				{
					if (ConfigLines[i].Value.Contains("master=true"))
					{
						if (!ConfigLines[i].Value.Contains("port_ce="))
						{
							KeyValuePair<ETokenType, string> NewLine = new KeyValuePair<ETokenType, string>(
								ETokenType.ClusterNode,
								ConfigLines[i].Value + " port_ce=41003");

							ConfigLines[i] = NewLine;
						}

						return true;
					}
				}
			}

			return false;
		}

		private bool IntroduceWindowEntity()
		{
			for (int i = 0; i < ConfigLines.Count; ++i)
			{
				if (ConfigLines[i].Key == ETokenType.ClusterNode)
				{
					// Token constants
					const string TokId = "id";
					const string TokViewport = "viewport";
					const string TokScreen = "screen";
					const string TokIsWindowed = "windowed";
					const string TokIsFullscreen = "fullscreen";
					const string TokWinX = "WinX";
					const string TokWinY = "WinY";
					const string TokResX = "ResX";
					const string TokResY = "ResY";

					// Extract old values
					string ClusterNodeId = Parser.GetStringValue(TokId, ConfigLines[i].Value);
					string OldViewportId = Parser.GetStringValue(TokViewport, ConfigLines[i].Value);
					string ScreenId      = Parser.GetStringValue(TokScreen, ConfigLines[i].Value);
					string WindowId      = "wnd_" + ClusterNodeId;

					// Previously window location/size was specified in [cluster_node], now it's moved to the [window]
					bool IsFullscreen = true;
					int WinX = 0;
					int WinY = 0;
					int ResX = 0;
					int ResY = 0;

					// We're in windowed mode only if all WinX/WinY/ResX/ResY arguments specified
					if (ConfigLines[i].Value.Contains(TokWinX) &&
						ConfigLines[i].Value.Contains(TokWinY) &&
						ConfigLines[i].Value.Contains(TokResX) &&
						ConfigLines[i].Value.Contains(TokResY))
					{
						IsFullscreen = false;
						WinX = Parser.GetIntValue(TokWinX, ConfigLines[i].Value);
						WinY = Parser.GetIntValue(TokWinY, ConfigLines[i].Value);
						ResX = Parser.GetIntValue(TokResX, ConfigLines[i].Value);
						ResY = Parser.GetIntValue(TokResY, ConfigLines[i].Value);
					}

					// Clean the [cluster_node] line from old stuff
					string NewClusterNodeLine = ConfigLines[i].Value;
					NewClusterNodeLine = Parser.RemoveArgument(TokViewport, NewClusterNodeLine);
					NewClusterNodeLine = Parser.RemoveArgument(TokScreen, NewClusterNodeLine);
					NewClusterNodeLine = Parser.RemoveArgument(TokIsWindowed, NewClusterNodeLine);
					NewClusterNodeLine = Parser.RemoveArgument(TokIsFullscreen, NewClusterNodeLine);
					NewClusterNodeLine = Parser.RemoveArgument(TokWinX, NewClusterNodeLine);
					NewClusterNodeLine = Parser.RemoveArgument(TokWinY, NewClusterNodeLine);
					NewClusterNodeLine = Parser.RemoveArgument(TokResX, NewClusterNodeLine);
					NewClusterNodeLine = Parser.RemoveArgument(TokResY, NewClusterNodeLine);
					NewClusterNodeLine += string.Format(" window={0}", WindowId);
					ConfigLines[i] = new KeyValuePair<ETokenType, string>(ETokenType.ClusterNode, NewClusterNodeLine);

					// Build a new [window] entity
					string NewViewportId = "vp_" + ClusterNodeId;
					string NewWindowLine = string.Format("[window] id={0} viewports={1}", WindowId, NewViewportId);
					if (IsFullscreen)
					{
						NewWindowLine = string.Format("{0} {1}=true", NewWindowLine, TokIsFullscreen);
					}
					else
					{
						NewWindowLine = string.Format("{0} {1}=false", NewWindowLine, TokIsFullscreen);
						NewWindowLine = string.Format("{0} {1}={2} {3}={4} {5}={6} {7}={8}", NewWindowLine,
							TokWinX, WinX, TokWinY, WinY, TokResX, ResX, TokResY, ResY);
					}
					LinesToAdd.Add(new KeyValuePair<ETokenType, string>(ETokenType.Window, NewWindowLine));

					// Finally, we need to update a viewport. We create a new line that will be added later.
					for (int j = 0; j < ConfigLines.Count; ++j)
					{
						if (ConfigLines[j].Key == ETokenType.Viewport)
						{
							if (OldViewportId == Parser.GetStringValue(TokId, ConfigLines[j].Value))
							{
								string NewViewportLine = ConfigLines[j].Value + " screen=" + ScreenId;
								NewViewportLine = NewViewportLine.Replace(TokId + "=" + OldViewportId, TokId + "=" + NewViewportId);
								LinesToAdd.Add(new KeyValuePair<ETokenType, string>(ETokenType.Viewport, NewViewportLine));
							}
						}
					}
				}
			}

			// Now we just remove old [viewport] entities since we have new ones in LinesToAdd
			ConfigLines.RemoveAll(x => x.Key == ETokenType.Viewport);

			return true;
		}

		private bool Finalize()
		{
			LinesToAdd.Add(new KeyValuePair<ETokenType, string>(ETokenType.Other, "# AUTO_CONVERSION, new entities finish"));
			LinesToAdd.Add(new KeyValuePair<ETokenType, string>(ETokenType.Other, string.Empty));
			LinesToAdd.Add(new KeyValuePair<ETokenType, string>(ETokenType.Other, string.Empty));

			ConfigLines.InsertRange(0, LinesToAdd);

			return true;
		}
	}
}
