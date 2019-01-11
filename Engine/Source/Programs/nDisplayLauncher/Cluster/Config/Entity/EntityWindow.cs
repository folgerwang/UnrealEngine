// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;

using nDisplayLauncher.Log;


namespace nDisplayLauncher.Cluster.Config.Entity
{
	public class EntityWindow : EntityBase
	{
		public string Id           { get; set; } = string.Empty;
		public bool   IsFullscreen { get; set; } = false;
		public int    WinX         { get; set; } = -1;
		public int    WinY         { get; set; } = -1;
		public int    ResX         { get; set; } = -1;
		public int    ResY         { get; set; } = -1;
		public List<string> Viewports { get; set; } = new List<string>();


		public EntityWindow()
		{
		}

		public EntityWindow(string text)
		{
			try
			{
				InitializeFromText(text);
			}
			catch (Exception ex)
			{
				AppLogger.Log(ex.Message);
			}
		}

		public override void InitializeFromText(string text)
		{
			Id = Parser.GetStringValue("id", text);
			IsFullscreen = Parser.GetBoolValue("fullscreen", text);
			WinX = Parser.GetIntValue("winx", text);
			WinY = Parser.GetIntValue("winy", text);
			ResX = Parser.GetIntValue("resx", text);
			ResY = Parser.GetIntValue("resy", text);
			Viewports = Parser.GetStringArrayValue("viewports", text);
		}
	}
}
