// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using nDisplayLauncher.Log;
using System;


namespace nDisplayLauncher.Cluster.Config.Entity
{
	public class EntityClusterNode : EntityBase
	{
		public string Id        { get; set; } = string.Empty;
		public string Window    { get; set; } = string.Empty;
		public bool   HasSound  { get; set; } = false;
		public bool   IsMaster  { get; set; } = false;
		public string Addr      { get; set; } = string.Empty;
		public int    PortCS    { get; set; } = 0;
		public int    PortSS    { get; set; } = 0;
		public int    PortCE    { get; set; } = 0;


		public EntityClusterNode()
		{
		}

		public EntityClusterNode(string text)
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
			Id       = Parser.GetStringValue("id", text);
			Window   = Parser.GetStringValue("window", text);
			HasSound = Parser.GetBoolValue("sound", text);
			IsMaster = Parser.GetBoolValue("master", text);
			Addr     = Parser.GetStringValue("addr", text);
			PortCS   = Parser.GetIntValue("port_cs", text);
			PortSS   = Parser.GetIntValue("port_ss", text);
			PortCE   = Parser.GetIntValue("port_ce", text);
		}
	}
}
