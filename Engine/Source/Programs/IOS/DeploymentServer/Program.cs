/**
 * Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Runtime.Remoting;
using System.Runtime.Remoting.Channels;
using System.Runtime.Remoting.Channels.Ipc;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.IO;
using Microsoft.Win32;
using iPhonePackager;
using Manzana;
using System.Net.Sockets;
using System.Net;
using System.Threading;
using System.Configuration;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Linq;

namespace DeploymentServer
{
	class Program
	{
		static int ExitCode = 0;
		const int DefaultPort = 41000;
		static int Port = DefaultPort;
		static bool IsRunningCommand = false;
		static bool IsStopping = false;
		static int ClientCounter = 0;
		static long TimeOut = 30000;
		static Stopwatch GlobalTimer = Stopwatch.StartNew();
		static int ParentPID = 0;
		static string TestStartPath = null;

		static TextWriter LocalConsole;
		static public void LocalLog(string s)
		{
			LocalConsole.WriteLine(s);
		}

		class TCPPortForwarding
		{
			public String DeviceID;
			public int TCPPort;
			public int DevicePort;
		}

		static List<TCPPortForwarding> TCPForwarding = new List<TCPPortForwarding>();

		class TcpClientInfo
		{
			public bool HasCommand
			{
				get
				{
					return bHasCommand;
				}
				set
				{
					bHasCommand = value;
				}
			}
			public bool KeepAlive
			{
				get
				{
					return bKeepAlive;
				}
			}
			public bool IsStillRunning
			{
				get { return !bCommandComplete; }
			}

			public bool NeedsResponse
			{
				get { return bNeedsResponse; }
			}

			public string LastCommand
			{
				get { return Command; }
			}

			public bool GetLastResult
			{
				get { return LastResult; }
			}

			protected bool bHasCommand = false;
			protected bool bNeedsResponse = false;
			protected string Command = "";
			protected List<string> FileList = new List<string>();
			protected string Bundle = "";
			protected string Manifest = "";
			protected string IpaPath = "";
			protected string Device = "";
			protected string Param1 = "";
			protected string Param2 = "";
			protected bool bKeepAlive = false;
			protected bool LastResult = false;

			System.Threading.Thread runLoop = null;
			System.Threading.Thread listenLoop = null;
			bool bCommandComplete = true;

			public void StopRun()
			{
				try
				{
					if (runLoop != null)
					{
						runLoop.Abort();
					}
					if (listenLoop != null)
					{
						listenLoop.Abort();
					}
				}
				catch { }
			}

			public bool ParseCommand(List<string> Arguments)
			{
				if (Arguments.Count >= 0)
				{
					
					int Start = 0;
					while(Arguments[Start].StartsWith("-"))
					{
						Start++;
						if (Start >= Arguments.Count)
						{
							return false;
						}
					}
					bKeepAlive = true;
					Command = Arguments[Start].ToLowerInvariant();
					FileList.Clear();
					for (int ArgIndex = 0; ArgIndex < Arguments.Count; ArgIndex++)
					{
						Arguments[ArgIndex] = Arguments[ArgIndex].Trim('"');
					}
					for (int ArgIndex = Start + 1; ArgIndex < Arguments.Count; ArgIndex++)
					{
						string Arg = Arguments[ArgIndex].ToLowerInvariant();
						switch (Arg)
						{
							case "-file":
								if (Arguments.Count > ArgIndex + 1)
								{
									FileList.Add(Arguments[++ArgIndex]);
								}
								else
								{
									return false;
								}
								break;

							case "-bundle":
								if (Arguments.Count > ArgIndex + 1)
								{
									Bundle = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;

							case "-manifest":
								if (Arguments.Count > ArgIndex + 1)
								{
									Manifest = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;

							case "-ipa":
								if (Arguments.Count > ArgIndex + 1)
								{
									IpaPath = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;

							case "-device":
								if (Arguments.Count > ArgIndex + 1)
								{
									Device = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;
							case "-param":
								if (Arguments.Count > ArgIndex + 1)
								{
									Param1 = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;
							case "-nokeepalive":
								bKeepAlive = false;
								break;
							case "-timeout":
								{
									if (Arguments.Count > ArgIndex + 1)
									{
										long ArgTime = TimeOut;
										long.TryParse(Arguments[++ArgIndex], out ArgTime);
										if (ArgTime > 0)
										{
											TimeOut = ArgTime;
											LocalLog(string.Format("Deployment Server timeout set to {0} (remote)", TimeOut.ToString()));
										}
									}
									break;
								}
							default:
								{
									if (Param1.Length < 1)
									{
										Param1 = Arguments[ArgIndex];
									}
									else if (Param2.Length < 1)
									{
										Param2 = Arguments[ArgIndex];
									}
									break;
								}
						}
					}
				}
				return true;
			}

			public int RunCommand(TextWriter Writer)
			{
				TextWriter ConsoleOld = Console.Out;

				LastResult = true;
				bCommandComplete = false;
				bool bWaitForCompletion = ShouldWaitForCompletion(Command);
				bNeedsResponse = bWaitForCompletion;

				runLoop = new System.Threading.Thread(delegate ()
				{
					try
					{
						DeploymentProxy.Deployer.DeviceId = Device;

						switch (Command)
						{
							case "stop":
								Console.SetOut(Writer);
								//LocalLog("Deployment Server Stopping ...");
								IsStopping = true;
								int StopTimeout = 12;
								while (ClientCounter > 1) // wait for other threads to stop so the client requesting to stop to block until safely stopped
								{
									CoreFoundationRunLoop.RunLoopRunInMode(CoreFoundationRunLoop.kCFRunLoopDefaultMode(), 1.0, 0);
									System.Threading.Thread.Sleep(50);
									StopTimeout--;
									if (StopTimeout <= 0)
									{
										//LocalLog("Deployment Server Forced Stopping ...");
										ClientCounter = 0;
										TCPForwarding.Clear();
										break;
									}
								}
								break;

                            case "backupdocuments":
                            case "backupdocs":
                                Console.SetOut(Writer);
                                LastResult = DeploymentProxy.Deployer.BackupDocumentsDirectory(Bundle, FileList.Count > 0 ? FileList[0] : ".");
								Writer.Flush();
								break;

							case "backup":
								Console.SetOut(Writer);
								LastResult = DeploymentProxy.Deployer.BackupFiles(Bundle, FileList.ToArray());
								Writer.Flush();
								break;

							case "deploy":
								Console.SetOut(Writer);
								LastResult = DeploymentProxy.Deployer.InstallFilesOnDevice(Bundle, Manifest);
								Writer.Flush();
								break;

							case "copyfile":
								Console.SetOut(Writer);
								LastResult = DeploymentProxy.Deployer.CopyFileToDevice(Bundle, FileList[0], FileList[1]);
								Writer.Flush();
								break;

							case "install":
								Console.SetOut(Writer);
								LastResult = DeploymentProxy.Deployer.InstallIPAOnDevice(IpaPath);
								Writer.Flush();
								break;

							case "enumerate":
								Console.SetOut(Writer);
								DeploymentProxy.Deployer.EnumerateConnectedDevices();
								Writer.Flush();
								break;

							case "listdevices":
								Console.SetOut(Writer);
								DeploymentProxy.Deployer.ListDevices();
								Writer.Flush();
								break;

							case "command":
								if (Device.Length < 5)
								{
									LocalLog("Device ID not present.");
									Writer.WriteLine("[command] Device ID not present.");
								}
								else if (Param1.Length < 1)
								{
									LocalLog("Parameter not present.");
									Writer.WriteLine("[command] Device ID not present.");
								}
								else
								{
									try
									{
										IntPtr TCPService = new IntPtr();
										MobileDeviceInstance targetDevice = DeploymentProxy.Deployer.StartTCPTunnel(Device, ref TCPService);
										if (targetDevice != null)
										{
											int Ret = targetDevice.TunnelData(Param1, TCPService);
											targetDevice.CloseTunnel(TCPService);

											LocalLog("[UE4][command] Sent '" + Ret.ToString() + "' bytes. (" + Param1.ToString() + ")");
											Writer.WriteLine("[UE4][command] Sent '{0}' bytes. ({1})", Ret, Param1);
										}
										else
										{
											LocalLog("[UE4][command] Sent '" + Device.ToString() + "' bytes. (" + Param1.ToString() + ")");
											Writer.WriteLine("[UE4][command] Device '{0}' not detected. ({1})", Device, Param1);
										}
									}
									catch
									{
										LocalLog("Errors encountered while tunneling to device.");
										Writer.WriteLine("[command] Errors encountered while tunneling to device.");
									}
								}
								Writer.Flush();
								break;

							case "forward":
								if (Device.Length < 5)
								{
									LocalLog("Device ID not present.");
								}
								else if (Param1.Length < 1)
								{
									LocalLog("Start TCP port not present.");
								}
								else if (Param2.Length < 1)
								{
									LocalLog("Destination TCP port not present.");
								}
								else
								{
									LastResult = ForwardToDevice(Writer, Device, Param1, Param2);
								}
								break;
							case "listforwarding":
								foreach (TCPPortForwarding P in TCPForwarding)
								{
									Writer.WriteLine("{0}\r{1}\r{2}", P.DeviceID, P.TCPPort, P.DevicePort);
								}
								Writer.WriteLine("");
								Writer.Flush();
								break;

							case "listentodevice":
								if (Device.Length < 5)
								{
									LocalLog("Device ID not present.");
								}
								else
								{
									listenLoop = new System.Threading.Thread(delegate ()
									{
										try
										{
											DeploymentProxy.Deployer.ListenToDevice(Device, Writer);
										}
										catch (IOException)
										{
											// we expect this to happen so we don't log it
										}
										catch (ThreadAbortException)
										{
											// we expect this to happen so we don't log it
										}
										catch (Exception e)
										{
											LocalLog("Exception: " + e.ToString());
										}
										finally
										{
											listenLoop = null;
										}
									});
									listenLoop.Start();
								}
								break;
						}
					}
					catch (IOException)
					{
						// we expect this to happen so we don't log it
					}
					catch (ThreadAbortException)
					{
						// we expect this to happen so we don't log it
					}
					catch (Exception e)
					{
						Console.SetOut(ConsoleOld);
						if (Command != "stop")
						{
							LocalLog("Exception: " + e.ToString());
						}
						LastResult = false;
					}
					finally
					{
						Console.SetOut(ConsoleOld);
						bCommandComplete = true;
					}
					
				});
				try
				{
					runLoop.Start();
					if (bWaitForCompletion)
					{
						while (!bCommandComplete)
						{
							CoreFoundationRunLoop.RunLoopRunInMode(CoreFoundationRunLoop.kCFRunLoopDefaultMode(), 1.0, 0);
						}
						if (runLoop.ThreadState == System.Threading.ThreadState.Running)
						{
							runLoop.Abort();
						}
					}
				}
				catch (Exception e)
				{
					
					if (Command != "stop")
					{
						LocalLog("Exception: " + e.ToString());
					}
					bCommandComplete = false;
					LastResult = false;
				}
				finally
				{
						
				}

				return LastResult ? 0 : 1;
			}

			protected bool IsForwardingInUse(String Device, int Port1, int Port2)
			{
				foreach(TCPPortForwarding P in TCPForwarding)
				{
					if (P.DevicePort == Port2 && P.DeviceID == Device)
					{
						return true;
					}
					if (P.TCPPort == Port1)
					{
						return true;
					}
				}
				return false;
			}

			protected bool ForwardToDevice(TextWriter Writer, String Device, String Param1, String Param2)
			{
				int Port1 = 0;
				int Port2 = 0;
				if (!int.TryParse(Param1, out Port1))
				{
					Writer.WriteLine("Invalid start port specified.");
					return false;
				}
				if (!int.TryParse(Param2, out Port2))
				{
					Writer.WriteLine("Invalid destination port specified.");
					return false;
				}
				if (IsForwardingInUse(Device, Port1, Port2))
				{
					Writer.WriteLine("Device {0} already has a TCP connection on port {1} or Port {2} is already in use.", Device, Param1, Param2);
					return false;
				}
				
				Thread ProcessClient = new System.Threading.Thread(delegate ()
				{
					while (!IsStopping)
					{
						TCPPortForwarding TcpFW = null;
						try
						{
							IntPtr TCPService = new IntPtr();
							MobileDeviceInstance TargetDevice = null;

							TcpFW = new TCPPortForwarding();
							TcpFW.DeviceID = Device;
							TcpFW.TCPPort = Port1;
							TcpFW.DevicePort = Port2;
							TCPForwarding.Add(TcpFW);

							TcpListener Server = null;
							Server = new TcpListener(IPAddress.Any, Port1);
							Server.Start();

							try
							{
								TcpClient Client = Server.AcceptTcpClient();
								//LocalLog("Got TCP connection.");
								NetworkStream ClStream = Client.GetStream();

								TargetDevice = DeploymentProxy.Deployer.StartTCPTunnel(Device, ref TCPService);
								if (TargetDevice == null)
								{
									LocalLog("Cannot connect to device " + Device + " for port forwarding.");
									break;
								}
								//LocalLog("Connected to device.");

								Byte[] Buffer = new Byte[1024];

								while (!IsStopping)
								{
									if (ClStream.DataAvailable)
									{
										int Bytes = ClStream.Read(Buffer, 0, Buffer.Length);
										TargetDevice.TunnelBuffer(Buffer, Bytes, TCPService);
									}
									else
									{
										if (Client.Client.Poll(10, SelectMode.SelectRead))
										{
											//LocalLog("TCP disconnected.");
											break;
										}
										System.Threading.Thread.Sleep(100);
									}
								}
							}
							catch
							{

							}
							finally
							{
								//LocalLog("Port forwarding disconnected.");
								if (TargetDevice != null)
								{
									TargetDevice.CloseTunnel(TCPService);
								}
								if (Server != null)
								{
									Server.Stop();
								}
							}
						}
						catch
						{

						}
						finally
						{
							if (TcpFW != null)
							{
								TCPForwarding.Remove(TcpFW);
							}
						}
					}
				});
				ProcessClient.Start();
				return true;
			}

			public static bool NeedsVersionCheck(String Command)
			{
				switch (Command)
				{
					case "stop":
					case "listdevices":
					case "listentodevice":
						return false;
				}
				return true;
			}
			public static bool ShouldWaitForCompletion(String Command)
			{
				switch (Command)
				{
					case "listentodevice":
						return false;
				}
				return true;
			}
		}

		static String GetDeploymentServerPath()
		{
			return Path.GetDirectoryName(AppDomain.CurrentDomain.BaseDirectory);
		}

		static void CreateDeploymentServerProcess()
		{
			Process NewProcess = new Process();

			NewProcess.StartInfo.WorkingDirectory = GetDeploymentServerPath();

			if (Environment.OSVersion.Platform == PlatformID.MacOSX || Environment.OSVersion.Platform == PlatformID.Unix)
			{
				NewProcess.StartInfo.WorkingDirectory.TrimEnd('/');
				NewProcess.StartInfo.FileName = NewProcess.StartInfo.WorkingDirectory + "/../../../Build/BatchFiles/Mac/RunMono.sh";
				NewProcess.StartInfo.Arguments = "\"" + NewProcess.StartInfo.WorkingDirectory + "/DeploymentServer.exe\" server " + " " + NewProcess.StartInfo.WorkingDirectory;
			}
			else
			{
				NewProcess.StartInfo.WorkingDirectory.TrimEnd('\\');
				NewProcess.StartInfo.FileName = NewProcess.StartInfo.WorkingDirectory + "\\DeploymentServerLauncher.exe";
				NewProcess.StartInfo.Arguments = "server";
			}
			NewProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
			NewProcess.StartInfo.UseShellExecute = true;

			try
			{
				NewProcess.Start();
				System.Threading.Thread.Sleep(500);
			}
			catch (System.Exception ex)
			{
				LocalLog("Failed to create deployment server process: " + ex.Message);
			}
		}

		static TcpClient GetServiceClient(string LocalCommand)
		{
			TcpClient Client = null;
			int RetryCount = 10;
			while (RetryCount > 0 && Client == null)
			{
				if (!IsServerMutexPresent())
				{
					if (LocalCommand == "stop")
					{
						LocalLog("Deployment Server not running ...");
						return null;
					}
					LocalLog("Creating Deployment Server ...");
					CreateDeploymentServerProcess();
				}
				else
				{
					if (RetryCount == 5)
					{
						ForceKillProcesses();
						LocalLog("Creating Deployment Server ...");
						CreateDeploymentServerProcess();
					}
				}
				try
				{
					if (Client == null)
					{
						Client = new TcpClient("localhost", Port); //The client gets here
					}
				}
				catch (Exception e)
				{
					//LocalLog("Retrying count {0} {1}", RetryCount.ToString(), e.Message);
					RetryCount--;
					if (Client != null)
					{
						Client.Close();
					}
					Client = null;
					if (RetryCount <= 0)
					{
						throw (e);
					}
					System.Threading.Thread.Sleep(750);
				}
			}

			return Client;
		}

		static void CreateDeploymentInterface()
		{
			if (ChannelServices.GetChannel("iPhonePackager") == null)
			{
				IpcServerChannel Channel = new IpcServerChannel("iPhonePackager");
				ChannelServices.RegisterChannel(Channel, false);
			}
			string URI = "DeploymentServer_PID";
			if (ParentPID > 0)
			{
				URI += ParentPID.ToString();
			}
			RemotingConfiguration.RegisterWellKnownServiceType(typeof(DeploymentProxy), URI, WellKnownObjectMode.Singleton);
		}

		protected static void ParseServerParam(List<string> Arguments)
		{
			if (Arguments.Count > 2)
			{
				TestStartPath = Arguments[2];
			}
			else
			{
				TestStartPath = GetDeploymentServerPath();
			}
			if (Arguments.Count > 3)
			{
				for (int ArgIndex = 3; ArgIndex < Arguments.Count; ArgIndex++)
				{
					string Arg = Arguments[ArgIndex].ToLowerInvariant();
					if (Arg.StartsWith("-"))
					{
						switch (Arg)
						{
							case "-timeout":
								{
									if (Arguments.Count > ArgIndex + 1)
									{
										long ArgTime = TimeOut;
										long.TryParse(Arguments[++ArgIndex], out ArgTime);
										if (ArgTime > 0)
										{
											TimeOut = ArgTime;
											LocalLog(string.Format("Deployment Server timeout set to {0}", TimeOut.ToString()));
										}
									}
									break;
								}
						}
					}
				}
			}
		}

		private static String ServerMutexName = "Global\\DeploymentServer_Mutex_SERVERINSTANCE";

		static bool IsServerMutexPresent()
		{
			Mutex DSBlockMutex = null;
			bool ret = Mutex.TryOpenExisting(ServerMutexName, out DSBlockMutex);
			if (ret)
			{
				DSBlockMutex.Close();
			}
			return ret;
		}

		/**
		 *	Main Server Loop
		 */
		static void ServerLoop(string[] Args)
		{
			Program.ExitCode = 0;
			if (IsServerMutexPresent())
				return;

			bool bCreatedMutex = false;
			Mutex DSBlockMutex = null;

			try
			{
				DSBlockMutex = new Mutex(false, ServerMutexName, out bCreatedMutex);
				DSBlockMutex.WaitOne();
			}
			catch
			{
				LocalLog("Exception creating Deployment Server Mutex. ");
				return;
			}

			TcpListener Server = null;
			FileStream OutSm = null;
			TextWriter Writer = null;
			TextWriter OldConsole = Console.Out;
			System.Threading.Thread ProcessClient = null;
			try
			{
				OutSm = new FileStream("DeploymentServer.log", FileMode.Create, FileAccess.Write);
				Writer = new StreamWriter(OutSm);
				Console.SetOut(Writer);

				DeploymentProxy.Deployer = new DeploymentImplementation();
				long.TryParse(ConfigurationManager.AppSettings["DSTimeOut"], out TimeOut);
				if (TimeOut < 30000)
				{
					TimeOut = 30000;
				}
				Server = new TcpListener(IPAddress.Any, Port);
				Server.Start();

				string CommandLine = "";
				foreach (string Arg in Args)
				{
					CommandLine += Arg + " ";
				}
				List<string> Arguments = Regex.Matches(CommandLine, @"[\""].+?[\""]|[^ ]+")
												.Cast<Match>()
												.Select(m => m.Value)
												.ToList();
				ParseServerParam(Arguments);
				LocalLog(string.Format("Deployment Server listening to port {0}", Port.ToString()));
				LocalLog(string.Format("Deployment Server inactivity timeout {0}", TimeOut.ToString()));
				LocalLog(string.Format("Deployment Server starting from {0}", TestStartPath));
				LocalLog("---------------------------------------------------------");

				// Processing commands

				ProcessClient = new System.Threading.Thread(delegate ()
				{
					try
					{
						int localClientID = 0;
						while (true)
						{
							TcpClient client = Server.AcceptTcpClient();
							
							TrackTCPClient(client, localClientID);
							localClientID++;
							System.Threading.Thread.Sleep(100);
						}
					}
					catch
					{

					}
				});
				ProcessClient.Start();

				// this will exit on its own after the set inactivity time or by a kill command or by a remote "stop" command
				while (true)
				{
					CoreFoundationRunLoop.RunLoopRunInMode(CoreFoundationRunLoop.kCFRunLoopDefaultMode(), 1.0, 0);
					System.Threading.Thread.Sleep(50);
					Writer.Flush();
					OutSm.Flush();
					if (TCPForwarding.Count > 0)
					{
						GlobalTimer.Restart();
					}
					if (ClientCounter <= 0)
					{
						if (IsStopping)
						{
							LocalLog("Deployment Server IsStopping exit.");
							break;
						}
						if (GlobalTimer.ElapsedMilliseconds > TimeOut)
						{
							LocalLog("Deployment Server inactivity timeout.");
							IsStopping = true;
							break;
						}
					}
				}
			}
			catch (SocketException e)
			{
				LocalLog("SocketException: " + e.ToString());
			}
			catch (System.Exception Ex)
			{
				LocalLog("Exception: " + Ex.ToString());
				LocalLog("Stack: " + Ex.StackTrace.ToString());
				LocalLog("Inner: " + Ex.InnerException.Message.ToString());
			}
			finally
			{
				if (DSBlockMutex != null)
				{
					try
					{
						DSBlockMutex.ReleaseMutex();
						DSBlockMutex.Close();
						DSBlockMutex = null;
					}
					catch (AbandonedMutexException)
					{
						//This catch is included to insure the program keeps running in the event this exception occurs.
						LocalLog("Deployment Server Mutex abandoned.");
					}
					catch (ApplicationException)
					{
						//This catch is included to insure the program keeps running in the event this exception occurs.
						LocalLog("Deployment Server Mutex abandoned 1.");
					}
					catch (SynchronizationLockException)
					{
						//This catch is included to insure the program keeps running in the event this exception occurs.
						LocalLog("Deployment Server Mutex abandoned. 2");
					}
				}
				if (ProcessClient != null)
				{
					ProcessClient.Abort();
				}
				if (Server != null)
				{
					Server.Stop();
				}
				LocalLog("Deployment Server Stopped.");
				Console.SetOut(OldConsole);
				if (Writer != null)
				{
					Writer.Close();
				}
				if (OutSm != null)
				{
					OutSm.Close();
				}
			}
			
			Environment.Exit(0);
		}

		static void RunLocalInstance(string[] Args)
		{
			// running as one instance only
			DeploymentProxy.Deployer = new DeploymentImplementation();
			CreateDeploymentInterface();
			TcpClientInfo LocalClientInfo = new TcpClientInfo();
			List<string> Arguments = new List<string>();
			Arguments.AddRange(Args);

			if (ParentPID > 0)
			{
				try
				{
					Process ParentProcess = Process.GetProcessById(ParentPID);
					while (!ParentProcess.HasExited)
					{
						CoreFoundationRunLoop.RunLoopRunInMode(CoreFoundationRunLoop.kCFRunLoopDefaultMode(), 1.0, 0);
					}
				}
				catch (System.Exception Ex)
				{
					LocalLog(Ex.Message);
				}
			}
			else
			{
				if (LocalClientInfo.ParseCommand(Arguments))
				{
					LocalClientInfo.HasCommand = true;
					LocalLog("Running as local instance");
					LocalClientInfo.RunCommand(Console.Out);
					while (LocalClientInfo.IsStillRunning)
					{
						Thread.Sleep(100);
					}
					bool RetCode = LocalClientInfo.GetLastResult;
				}
			}
		}

		/**
		 * Main Client loop
		 */
		static void ClientLoop(string[] Args, string LocalCommand)
		{
			// on mac we only start one local instance due to mono limitations
			if (Environment.OSVersion.Platform == PlatformID.MacOSX || Environment.OSVersion.Platform == PlatformID.Unix || Args[0].Equals("-standalone"))
			{
				RunLocalInstance(Args);
				return;
			}

			TcpClient Client = GetServiceClient(LocalCommand);

			try
			{
				StreamReader clientIn = new StreamReader(Client.GetStream());
				StreamWriter clientOut = new StreamWriter(Client.GetStream());
				{
					string Response = clientIn.ReadLine();
					if (TcpClientInfo.NeedsVersionCheck(LocalCommand))
					{
						if (!Response.Equals("[DSDIR]\"" + GetDeploymentServerPath() + "\"", StringComparison.InvariantCultureIgnoreCase))
						{
							LocalLog("Wrong server running, restarting the server ...");
							clientOut.Write("stop");
							clientOut.Write("\r");
							clientOut.Flush();
							while (true)
							{
								Response = clientIn.ReadLine();
								if (Response.EndsWith("CMDOK") || Response.EndsWith("CMDFAIL"))
								{
									break;
								}
								else
								{
									LocalLog(Response);
								}
							}
							if (Client != null)
							{
								Client.Close();
							}
							Thread.Sleep(2500);
							Client = GetServiceClient("");
							clientIn = new StreamReader(Client.GetStream());
							clientOut = new StreamWriter(Client.GetStream());
						}
					}
				}

				for (int ArgIndex = 0; ArgIndex < Args.Length; ArgIndex++)
				{
					if (Args[ArgIndex].Contains(' '))
					{
						clientOut.Write('"' + Args[ArgIndex] + '"' + ' ');
					}
					else
					{
						clientOut.Write(Args[ArgIndex] + ' ');
					}
				}
				clientOut.Write("\r");
				clientOut.Flush();

				while (true)
				{
					string Response = clientIn.ReadLine();
					if (Response.EndsWith("CMDOK"))
					{
						Program.ExitCode = 0;
						break;
					}
					else if (Response.EndsWith("CMDFAIL"))
					{
						Program.ExitCode = 1;
						break;
					}
					else
					{
						LocalLog(Response);
					}
					Thread.Sleep(10);
				}
			}
			catch (Exception e)
			{
				if (LocalCommand != "stop")
				{
					LocalLog("Exception: " + e.ToString());
				}
			}
			finally
			{
				if (Client != null)
				{
					Client.Close();
				}
			}
		}

		static int Main(string[] Args)
		{
			LocalConsole = Console.Out;
			string LocalCommand = "";
			if (Args.Length > 0)
			{
				LocalCommand = Args[0].ToLowerInvariant();
			}
			else
			{
				LocalLog("Deployment Server usage: ");
				LocalLog("DeploymentServer.exe <command> [<parameter> [<value>] ...]");
				LocalLog("Valid Commands:");
				LocalLog("\t stop");
				LocalLog("\t backup");
				LocalLog("\t deploy");
				LocalLog("\t copyfile");
				LocalLog("\t install");
				LocalLog("\t enumerate");
				LocalLog("\t listdevices");
				LocalLog("\t listentodevice");
				LocalLog("\t command");
				LocalLog("\t forward");
				LocalLog("\t -iphonepackager");
				LocalLog("\t server");
				LocalLog("Valid Parameters:");
				LocalLog("\t -file <filename>");
				LocalLog("\t -bundle <bundle name>");
				LocalLog("\t -manifest <manifest file>");
				LocalLog("\t -ipa <ipa path>");
				LocalLog("\t -device <device ID>");
				LocalLog("\t -nokeepalive");
				LocalLog("\t -timeout <miliseconds>");
				LocalLog("\t -param <string parameter to be used for command>");
				LocalLog("");

				return 0;
			}
			int.TryParse(ConfigurationManager.AppSettings["DSPort"], out Port);
			if (Port < 1 || Port > 65535)
			{
				Port = DefaultPort;
			}

			// parrent ID not needed anymore
			if (Args[0].Equals("server"))
			{
				ServerLoop(Args);
			}
			else if (Args[0].Equals("-iphonepackager"))
			{
				ParentPID = int.Parse(Args[1]);
				RunLocalInstance(Args);
			}
			else
			{
				ClientLoop(Args, LocalCommand);
			}

			Environment.ExitCode = Program.ExitCode;
			return Program.ExitCode;
		}
		
		private static string ProcessTCPRequest(string ClientData, ref TcpClientInfo Client)
		{
			string[] Commands = ClientData.Split('\n');
			string Ret = "";
			if (Commands.Length >= 1)
			{
				// hm ... this will always process only the last command
				for (int i = 0; i < Commands.Length; i++)
				{
					if (i < Commands.Length - 1 || Commands[i].EndsWith("\r"))
					{
						List<string> Arguments = Regex.Matches(Commands[i].TrimEnd('\r'), @"[\""].+?[\""]|[^ ]+")
												.Cast<Match>()
												.Select(m => m.Value)
												.ToList();
						if (Client.ParseCommand(Arguments))
						{
							Client.HasCommand = true;
						}
					}
					else
					{
						Ret = Commands[i];
					}
				}
			}
			return Ret;
		}

		private static void TrackTCPClient(TcpClient Client, int localID)
		{
			string LastCommand = "";
			System.Threading.Thread processTracker = new System.Threading.Thread(delegate ()
			{
				ClientCounter++;
				TcpClientInfo ClientInfo = null;
				try
				{
					NetworkStream ClStream = Client.GetStream();
					string ClientIP = ((IPEndPoint)Client.Client.RemoteEndPoint).Address.ToString();
					//LocalLog("Client [{0}] IP:{1} connected.", localID, ClientIP);

					StreamWriter Writer = new StreamWriter(ClStream);
					Writer.AutoFlush = true;
					
					Writer.WriteLine("[DSDIR]" + TestStartPath);
					Writer.Flush();
					
					Byte[] Buffer = new Byte[2048];
					string Unprocessed = "";
					ClientInfo = new TcpClientInfo();
					bool IsCommandStarted = false;
					
					while (true)
					{
						//LocalLog("Looping [{0}]", localID);
						if (ClientInfo.HasCommand && !IsStopping)
						{
							if (!IsRunningCommand && !ClientInfo.IsStillRunning)
							{
								ClientInfo.HasCommand = false;
								IsRunningCommand = true;

								try
								{
									LastCommand = ClientInfo.LastCommand;
									ClientInfo.RunCommand(Writer);
									IsCommandStarted = true;
								}
								catch
								{

								}
								IsRunningCommand = false;
								if (!ClientInfo.IsStillRunning)
								{
									if (ClientInfo.NeedsResponse || !ClientInfo.GetLastResult)
									{
										Writer.WriteLine(ClientInfo.GetLastResult ? "\nCMDOK" : "\nCMDFAIL");
									}
									Writer.Flush();
									ClStream.Flush();
									break;
								}
							}
							else if (ClientInfo.LastCommand == "stop")
							{
								ClientInfo.RunCommand(Writer);
							}
						}
						
						if (ClStream.DataAvailable)
						{
							int Bytes = ClStream.Read(Buffer, 0, Buffer.Length);
							string Cmd = Unprocessed + System.Text.Encoding.ASCII.GetString(Buffer, 0, Bytes);
							Unprocessed = ProcessTCPRequest(Cmd, ref ClientInfo);
							if (ClientInfo.KeepAlive)
							{
								GlobalTimer.Restart();
							}
						}
						else
						{
							System.Threading.Thread.Sleep(100);
						}
						if (ClientInfo.IsStillRunning)
						{
							bool BlockingState = Client.Client.Blocking;
							try
							{
								byte[] tmp = new byte[1];

								Client.Client.Blocking = false;
								Client.Client.Send(tmp, 0, 0);
							}
							catch (SocketException e)
							{
								// 10035 == WSAEWOULDBLOCK
								if (e.NativeErrorCode.Equals(10035))
								{
									//LocalLog("Still Connected, but the Send would block");
								}
								else
								{
									// disconnected
									break;
								}
							}
							finally
							{
								Client.Client.Blocking = BlockingState;
							}
						}
						else if (IsCommandStarted)
						{
							// long running command has stopped
							break;
						}
						if (IsStopping)
						{
							try
							{
								if (ClientInfo.HasCommand)
								{
									Writer.WriteLine("\nCMDFAIL");
								}
								else
								{
									Writer.WriteLine("\nCMDOK");

								}
								Writer.Flush();
								ClStream.Flush();
							}
							catch
							{
								// we'll go silent since we're closing
							}
							break;
						}
					}
				}
				catch (IOException)
				{
					// we expect this to happen so we don't log it
				}
				catch (Exception e)
				{
					LocalLog("Exception: " + e.ToString());
				}
				finally
				{
					if (ClientInfo != null)
					{
						ClientInfo.StopRun();
					}
					if (Client != null)
					{
						if (Client.Client != null && Client.Client.RemoteEndPoint != null)
						{
							//LocalLog("Client [{0}] disconnected ({1}).", localID, LastCommand);
						}
						Client.Close();
					}
				}
				ClientCounter--;
			});
			processTracker.Start();
		}

		static void ForceKillProcesses()
		{
			foreach (var process in Process.GetProcessesByName("DeploymentServer"))
			{
				if (process.Id != Process.GetCurrentProcess().Id)
				{
					process.Kill();
				}
			}
			foreach (var process in Process.GetProcessesByName("DeploymentServerLauncher"))
			{
				if (process.Id != Process.GetCurrentProcess().Id)
				{
					process.Kill();
				}
			}
		}
	}
}
