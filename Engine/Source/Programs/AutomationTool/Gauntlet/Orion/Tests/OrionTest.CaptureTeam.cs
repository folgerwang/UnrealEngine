// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System.Linq;
using System.Net;

namespace OrionTest
{
	/// <summary>
	/// Test that just boots the client and server and does nothing
	/// </summary>
	public class CaptureTeam : Gauntlet.UnrealTestNode
	{
		public CaptureTeam(Gauntlet.UnrealTestContext InContext)
			: base(InContext)
		{
		}

		public override UnrealTestConfiguration GetConfiguration()
		{
			UnrealTestConfiguration Config = base.GetConfiguration();
			Config.MaxDuration = 60.0f * 60.0f * 24.0f; // set to 1-day.		

			Config.ClientCount = Context.TestParams.ParseValue("numclients", 1);

			string LocalIP = Dns.GetHostEntry(Dns.GetHostName()).AddressList.Where(o => o.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork).First().ToString();

			Config.EpicApp = null;
			Config.Map = Context.TestParams.ParseValue("map", "monolith");

			// server commandline, no need for nomcp as it's controlled by the 'EpicApp' value above
			Config.ServerCommandline += " -log -notimeouts";

			// client commandline, no need for nomcp as it's controlled by the 'EpicApp' value above

			// This tells the client to connect to the server above
			Config.ClientCommandline += string.Format(" -ExecCmds=\"open {0}\"", LocalIP);

			// Other client args
			Config.ClientCommandline += " -notexturestreaming -notimeouts -log -hidehudkeys";			

			return Config;
		}
	}
}
