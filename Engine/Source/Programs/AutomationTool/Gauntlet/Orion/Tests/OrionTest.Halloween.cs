// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System.Linq;
using System.Net;
using EpicGame;

namespace OrionTest
{
	/// <summary>
	/// Test that just boots the client and server with Halloween Rotational Content Overrides to test Halloween map.
	/// </summary>
	public class Halloween : EpicGameTestNode
	{
		public Halloween(Gauntlet.UnrealTestContext InContext) 
			: base(InContext)
		{
		}

		public override EpicGameTestConfig GetConfiguration()
		{
			EpicGameTestConfig Config = base.GetConfiguration();

			Config.MaxDuration = 60.0f * 60.0f * 24.0f; // set to 1-day.		

			// none tests are considered attended
			Config.Attended = true;

			int ClientCount = Context.TestParams.ParseValue("numclients", 1);

			var ClientRoles = Config.RequireRoles(UnrealRoleType.Client, ClientCount);
			var ServerRole = Config.RequireRole(UnrealRoleType.Server);

            //Append halloween to command line
            ClientRoles.ToList().ForEach((R) =>
            {
                R.CommandLine += "-RotationalContentTags=RotationalContent.MapOverrides.Halloween";
            });

			if (Context.TestParams.ParseParam("nomcp"))
			{
				string LocalIP = Dns.GetHostEntry(Dns.GetHostName()).AddressList.Where(o => o.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork).First().ToString();

				Config.RequiresMCP = false;
				Config.Map = Context.TestParams.ParseValue("map", "monolith02");				

				ClientRoles.ToList().ForEach((R) =>
				{
					R.CommandLine += string.Format(" -longtimeouts -ExecCmds=\"open {0}\"", LocalIP);
				});

				ServerRole.CommandLine += " -longtimeouts";
			}

			ClientRoles.ToList().ForEach((R) =>
			{
				R.CommandLine += " -gauntlet=DelayedExec -delayedexec=\"game, 5, GiveTeamXP 1 10000, stat fps\"";
			});

			int TeamSize = Context.TestParams.ParseValue("teamsize", 0);

			if (TeamSize > 0)
			{
				int MaxPlayers = Context.TestParams.ParseValue("maxplayers", 1);

				ClientRoles.ToList().ForEach((R) =>
				{
					R.CommandLine += string.Format(" -teamsize={0} -maxplayers={1}", TeamSize, MaxPlayers);
				});

				ServerRole.CommandLine += string.Format(" -teamsize={0} -maxplayers={1}", TeamSize, MaxPlayers);
			}

			return Config;
		}
	}
}
