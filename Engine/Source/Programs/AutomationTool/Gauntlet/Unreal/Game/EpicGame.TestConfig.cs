// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System.Linq;
using System.Net;
using System;
using UnrealBuildTool;
using AutomationTool;

namespace EpicGame
{

	/// <summary>
	/// An additional set of options that pertain to internal epic games.
	/// </summary>
	public class EpicGameTestConfig : UE4Game.UE4TestConfig, IAutoParamNotifiable
	{
		/// <summary>
		/// Should this test skip mcp?
		/// </summary>
		[AutoParam(false)]
		public bool NoMCP = false;

		[AutoParam(false)]
		public bool FastCook = false;

		/// <summary>
		/// Which backend to use for matchmaking
		/// </summary>
		[AutoParam("DevLatest")]
		public string EpicApp = "";

		/// <summary>
		/// Unique buildid to avoid matchmaking collisions
		/// </summary>
		[AutoParam("")]
		protected string BuildIDOverride = "";

		/// <summary>
		/// Unique server port to avoid matchmaking collisions
		/// </summary>
		[AutoParam(ServerPortStart)]
		protected int ServerPort;
		const int ServerPortStart = 7777;

		/// <summary>
		/// Unique server beacon port to avoid matchmaking collisions
		/// </summary>
		[AutoParam(BeaconPortStart)]
		protected int BeaconPort;
		const int BeaconPortStart = 15000;

		/// <summary>
		/// Make sure the client gets -logpso when we are collecting them
		/// </summary>
		[AutoParam(false)]
        public bool LogPSO = false;

        /// <summary>
        /// Should this test assign a random test account?
        /// </summary>
        [AutoParam(true)]
        public bool PreAssignAccount = true;


        // incrementing value to ensure we can assign unique values to ports etc
        static private int NumberOfConfigsCreated = 0;

		public EpicGameTestConfig()
		{
			NumberOfConfigsCreated++;
		}

		~EpicGameTestConfig()
		{
		}


		public void ParametersWereApplied(string[] Params)
		{
			
			if (string.IsNullOrEmpty(BuildIDOverride))
			{
				// pick a default buildid that's the last 4 digits of our IP
				string LocalIP = Dns.GetHostEntry(Dns.GetHostName()).AddressList.Where(o => o.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork).First().ToString();
				LocalIP = LocalIP.Replace(".", "");
				BuildIDOverride = LocalIP.Substring(LocalIP.Length - 4);
			}

			// techinically this doesn't matter for mcp because the server will pick a free port and tell the backend what its using, but
			// nomcp requires us to know the port and thus we need to make sure ones we pick haven't been previously assigned or grabbed
			if (NumberOfConfigsCreated > 1)
			{
				BuildIDOverride += string.Format("{0}", NumberOfConfigsCreated);
				ServerPort = (ServerPortStart + NumberOfConfigsCreated);
				BeaconPort = (BeaconPortStart + NumberOfConfigsCreated);
			}
		}

		public override void ApplyToConfig(UnrealAppConfig AppConfig)
		{
			base.ApplyToConfig(AppConfig);

			if (AppConfig.ProcessType.IsClient() || AppConfig.ProcessType.IsServer())
			{
				string McpString = "";

				if (AppConfig.ProcessType.IsServer())
				{
					// set explicit server and beacon port for online services
					// this is important when running tests in parallel to avoid matchmaking collisions
					McpString += string.Format(" -port={0}", ServerPort);
					McpString += string.Format(" -beaconport={0}", BeaconPort);
				}

				if (NoMCP)
				{

					McpString += " -nomcp -notimeouts";

					// if this is a client, and there is a server role, find our PC's IP address and tell it to connect to us
					if (AppConfig.ProcessType.IsClient() &&
							(RequiredRoles.ContainsKey(UnrealTargetRole.Server) || RequiredRoles.ContainsKey(UnrealTargetRole.EditorServer)))
					{
						// find all valid IP addresses but throw away anything with an invalid range
						var LocalAddress = Dns.GetHostEntry(Dns.GetHostName()).AddressList
							.Where(o => o.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork
								&& o.GetAddressBytes()[0] != 169)
							.FirstOrDefault();

						if (LocalAddress == null)
						{
							throw new AutomationException("Could not find local IP address");
						}
						string LocalIP = Globals.Params.ParseValue("serverip", LocalAddress.ToString());
						McpString += string.Format(" -ExecCmds=\"open {0}:{1}\"", LocalIP, ServerPort);
					}
					
				}
				else
				{
					McpString += string.Format(" -epicapp={0} -buildidoverride={1}", EpicApp, BuildIDOverride);
				}

				if (FastCook)
				{
					McpString += " -FastCook";
				}

				AppConfig.CommandLine += McpString;
			}

			if (AppConfig.ProcessType.IsClient())
			{
				// turn off skill-based matchmaking, turn off porta;
				AppConfig.CommandLine += " -nosbmm";

                if (LogPSO)
                {
                    AppConfig.CommandLine += " -logpso";
                }

                if (AppConfig.Platform == UnrealTargetPlatform.Win64)
				{
					// turn off skill-based matchmaking, turn off porta;
					AppConfig.CommandLine += " -noepicportal";
				}

				// select an account
				if (NoMCP == false && AppConfig.Platform != UnrealTargetPlatform.PS4 && AppConfig.Platform != UnrealTargetPlatform.XboxOne && PreAssignAccount == true)
				{
					Account UserAccount = AccountPool.Instance.ReserveAccount();
					UserAccount.ApplyToConfig(AppConfig);
				}
			}
		}

		
	}
	
	/// <summary>
	/// Test that just boots the client and server and does nothing
	/// </summary>
	public abstract class EpicGameTestNode : UnrealTestNode<EpicGameTestConfig>
	{
		public EpicGameTestNode(UnrealTestContext InContext) : base(InContext)
		{

		}
	}

}