// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System.Linq;
using System.Net;
using System;
using UnrealBuildTool;
using AutomationTool;
using System.Net.NetworkInformation;

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
		[AutoParam]
		public bool NoMCP = false;

		[AutoParam]
		public bool FastCook = false;

		/// <summary>
		/// Which backend to use for matchmaking
		/// </summary>
		[AutoParam]
		public string EpicApp = "DevLatest";

		/// <summary>
		/// Unique buildid to avoid matchmaking collisions
		/// </summary>
		[AutoParam]
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
		[AutoParam]
        public bool LogPSO = false;

        /// <summary>
        /// Should this test assign a random test account?
        /// </summary>
        [AutoParam]
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

					AppConfig.CommandLine += " -net.forcecompatible";
				}

				// Default to the first address with a valid prefix
				var LocalAddress = Dns.GetHostEntry(Dns.GetHostName()).AddressList
					.Where(o => o.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork
						&& o.GetAddressBytes()[0] != 169)
					.FirstOrDefault();

				var ActiveInterfaces = NetworkInterface.GetAllNetworkInterfaces()
					.Where(I => I.OperationalStatus == OperationalStatus.Up);

				bool MultipleInterfaces = ActiveInterfaces.Count() > 1;
				
				if (MultipleInterfaces)
				{
					// Now, lots of Epic PCs have virtual adapters etc, so see if there's one that's on our network and if so use that IP
					var PreferredInterface = ActiveInterfaces
						.Where(I => I.GetIPProperties().DnsSuffix.Equals("epicgames.net", StringComparison.OrdinalIgnoreCase))
						.SelectMany(I => I.GetIPProperties().UnicastAddresses)
						.Where(A => A.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
						.FirstOrDefault();

					if (PreferredInterface != null)
					{
						LocalAddress = PreferredInterface.Address;
					}					
				}

				if (LocalAddress == null)
				{
					throw new AutomationException("Could not find local IP address");
				}

				string RequestedServerIP = Globals.Params.ParseValue("serverip", "");
				string RequestedClientIP = Globals.Params.ParseValue("clientip", "");
				string ServerIP = string.IsNullOrEmpty(RequestedServerIP) ? LocalAddress.ToString() : RequestedServerIP;
				string ClientIP = string.IsNullOrEmpty(RequestedClientIP) ? LocalAddress.ToString() : RequestedClientIP;
				

				// Do we need to add the -multihome argument to bind to specific IP?
				if (AppConfig.ProcessType.IsServer() && (MultipleInterfaces || !string.IsNullOrEmpty(RequestedServerIP)))
				{ 		
					AppConfig.CommandLine += string.Format(" -multihome={0}", ServerIP);
				}

				// client too, but only desktop platforms
				if (AppConfig.ProcessType.IsClient() && (MultipleInterfaces || !string.IsNullOrEmpty(RequestedClientIP)))
				{
					if (AppConfig.Platform == UnrealTargetPlatform.Win64 || AppConfig.Platform == UnrealTargetPlatform.Mac)
					{
						AppConfig.CommandLine += string.Format(" -multihome={0}", ClientIP);
					}
				}

				if (NoMCP)
				{
					McpString += " -nomcp -notimeouts";

					// if this is a client, and there is a server role, find our PC's IP address and tell it to connect to us
					if (AppConfig.ProcessType.IsClient() &&
							(RequiredRoles.ContainsKey(UnrealTargetRole.Server) || RequiredRoles.ContainsKey(UnrealTargetRole.EditorServer)))
					{						
						McpString += string.Format(" -ExecCmds=\"open {0}:{1}\"", ServerIP, ServerPort);
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