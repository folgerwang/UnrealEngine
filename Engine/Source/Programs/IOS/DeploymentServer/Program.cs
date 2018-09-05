/**
 * Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
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
		static long TimeOut = 120000;
		static Stopwatch GlobalTimer = Stopwatch.StartNew();

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

			protected bool bHasCommand = false;
			protected bool bNeedsResponse = false;
			protected string Command = "";
			protected List<string> FileList = new List<string>();
			protected string Bundle = "";
			protected string Manifest = "";
			protected string ipaPath = "";
			protected string Device = "";
			protected bool bKeepAlive = false;

			System.Threading.Thread runLoop = null;
			bool bCommandComplete = true;

			public void StopRun()
			{
				try
				{
					if (runLoop != null)
					{
						runLoop.Abort();
					}
				}
				catch { }
			}

			public bool ParseCommand(string[] Arguments, int Start)
			{
				if (Arguments.Length >= Start)
				{
					bKeepAlive = true;
					Command = Arguments[0].ToLowerInvariant();
					FileList.Clear();
					for (int ArgIndex = Start; ArgIndex < Arguments.Length; ArgIndex++)
					{
						string Arg = Arguments[ArgIndex].ToLowerInvariant();
						if (Arg.StartsWith("-"))
						{
							switch (Arg)
							{
								case "-file":
									if (Arguments.Length > ArgIndex + 1)
									{
										FileList.Add(Arguments[++ArgIndex]);
									}
									else
									{
										return false;
									}
									break;

								case "-bundle":
									if (Arguments.Length > ArgIndex + 1)
									{
										Bundle = Arguments[++ArgIndex];
									}
									else
									{
										return false;
									}
									break;

								case "-manifest":
									if (Arguments.Length > ArgIndex + 1)
									{
										Manifest = Arguments[++ArgIndex];
									}
									else
									{
										return false;
									}
									break;

								case "-ipa":
									if (Arguments.Length > ArgIndex + 1)
									{
										ipaPath = Arguments[++ArgIndex];
									}
									else
									{
										return false;
									}
									break;

								case "-device":
									if (Arguments.Length > ArgIndex + 1)
									{
										Device = Arguments[++ArgIndex];
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
										if (Arguments.Length > ArgIndex + 1)
										{
											long ArgTime = TimeOut;
											long.TryParse(Arguments[++ArgIndex], out ArgTime);
											if (ArgTime > 0)
											{
												TimeOut = ArgTime;
												Console.WriteLine(string.Format("Deployment Server timeout set to {0} (remote)", TimeOut.ToString()));
											}
										}
										break;
									}
							}
						}
					}
				}
				return true;
			}

			public int RunCommand(TextWriter Writer)
			{
				TextWriter ConsoleOld = Console.Out;
				Console.SetOut(Writer);

				bool bResult = true;
				bCommandComplete = false;
				bool bWaitForCompletion = ShouldWaitForCompletion(Command);
				bNeedsResponse = !bWaitForCompletion;

				runLoop = new System.Threading.Thread(delegate ()
				{

					DeploymentProxy.Deployer.DeviceId = Device;

					//Console.WriteLine("Running Command: " + Command);
					
					switch (Command)
					{
						case "stop":
							Console.WriteLine("Deployment Server Stopping ...");
							IsStopping = true;
							while (ClientCounter > 1) // wait for other threads to stop so the client requesting to stop to block until safely stopped
							{
								CoreFoundationRunLoop.RunLoopRunInMode(CoreFoundationRunLoop.kCFRunLoopDefaultMode(), 1.0, 0);
								System.Threading.Thread.Sleep(50);
							}
							break;
						case "backup":
							bResult = DeploymentProxy.Deployer.BackupFiles(Bundle, FileList.ToArray());
							break;

						case "deploy":
							bResult = DeploymentProxy.Deployer.InstallFilesOnDevice(Bundle, Manifest);
							break;

						case "copyfile":
							bResult = DeploymentProxy.Deployer.CopyFileToDevice(Bundle, FileList[0], FileList[1]);
							break;

						case "install":
							bResult = DeploymentProxy.Deployer.InstallIPAOnDevice(ipaPath);
							break;

						case "enumerate":
							DeploymentProxy.Deployer.EnumerateConnectedDevices();
							break;

						case "listdevices":
							DeploymentProxy.Deployer.ListDevices();
							break;

						case "listentodevice":
							if (Device.Length < 5)
							{
								Console.WriteLine("Device ID not present.");
							}
							else
							{
								DeploymentProxy.Deployer.ListenToDevice(Device, Writer);
							}
							break;
					}
					bCommandComplete = true;
				});
				runLoop.Start();
				if (bWaitForCompletion)
				{
					while (!bCommandComplete)
					{
						CoreFoundationRunLoop.RunLoopRunInMode(CoreFoundationRunLoop.kCFRunLoopDefaultMode(), 1.0, 0);
					}
				}
				//Console.WriteLine("after list ...");
				Writer.Flush();
				Console.SetOut(ConsoleOld);

				return bResult ? 0 : 1;
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
			return Path.GetFullPath(".");
		}

		static Process CreateDeploymentServerProcess()
		{
			Process NewProcess = new Process();
			if (Environment.OSVersion.Platform == PlatformID.MacOSX || Environment.OSVersion.Platform == PlatformID.Unix)
			{
				NewProcess.StartInfo.WorkingDirectory = GetDeploymentServerPath();
				NewProcess.StartInfo.FileName = "../../../Build/BatchFiles/Mac/RunMono.sh";
				NewProcess.StartInfo.Arguments = "\"" + NewProcess.StartInfo.WorkingDirectory + "/DeploymentServer.exe\" -iphonepackager " + Process.GetCurrentProcess().Id.ToString();
			}
			else
			{
				NewProcess.StartInfo.WorkingDirectory = GetDeploymentServerPath();
				NewProcess.StartInfo.FileName = NewProcess.StartInfo.WorkingDirectory + "\\DeploymentServer.exe";
				NewProcess.StartInfo.Arguments = "-iphonepackager " + Process.GetCurrentProcess().Id.ToString();
			}
			NewProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
			NewProcess.StartInfo.UseShellExecute = true;
			//NewProcess.StartInfo.UseShellExecute = false;

			try
			{
				NewProcess.Start();
				System.Threading.Thread.Sleep(500);
			}
			catch (System.Exception ex)
			{
				Console.WriteLine("Failed to create deployment server process ({0})", ex.Message);
			}

			return NewProcess;
		}

		static TcpClient IsServiceRegistered()
		{
			// querring the TCP interface should work on mac as well
			TcpClient Client = null;
			try
			{
				Client = new TcpClient("localhost", Port); //The client gets here
			}
			catch
			{
				if (Client != null)
				{
					Client.Close();
				}
				Client = null;
			}

			return Client;
		}

		protected static void ParseServerParam(string[] Arguments)
		{
			if (Arguments.Length >= 2)
			{
				for (int ArgIndex = 2; ArgIndex < Arguments.Length; ArgIndex++)
				{
					string Arg = Arguments[ArgIndex].ToLowerInvariant();
					if (Arg.StartsWith("-"))
					{
						switch (Arg)
						{
							case "-timeout":
								{
									if (Arguments.Length > ArgIndex + 1)
									{
										long ArgTime = TimeOut;
										long.TryParse(Arguments[++ArgIndex], out ArgTime);
										if (ArgTime > 0)
										{
											TimeOut = ArgTime;
											Console.WriteLine(string.Format("Deployment Server timeout set to {0}", TimeOut.ToString()));
										}
									}
									break;
								}
						}
					}
				}
			}
		}

		static int Main(string[] args)
		{
			String LocalCommand = "";
			if (args.Length > 0)
			{
				LocalCommand = args[0].ToLowerInvariant();
			}
			else
			{
				Console.WriteLine("Deployment Server usage: ");
				Console.WriteLine("DeploymentServer.exe <command> [<parameter> [<value>] ...]");
				Console.WriteLine("Valid Commands:");
				Console.WriteLine("\t stop");
				Console.WriteLine("\t backup");
				Console.WriteLine("\t deploy");
				Console.WriteLine("\t copyfile");
				Console.WriteLine("\t install");
				Console.WriteLine("\t enumerate");
				Console.WriteLine("\t listdevices");
				Console.WriteLine("\t listentodevice");
				Console.WriteLine("\t -iphonepackager");
				Console.WriteLine("Valid Parameters:");
				Console.WriteLine("\t -file <filename>");
				Console.WriteLine("\t -bundle <bundle name>");
				Console.WriteLine("\t -manifest <manifest file>");
				Console.WriteLine("\t -ipa <ipa path>");
				Console.WriteLine("\t -device <device ID>");
				Console.WriteLine("\t -nokeepalive");
				Console.WriteLine("\t -timeout <miliseconds>");
				Console.WriteLine("");

				return 0;
			}
			try
			{
				if (LocalCommand != "stop")
				{
					bool bCreatedMutex = false;
					String MutexName = "Global\\DeploymentServer_Mutex_RestartNotAllowed";
					Mutex DSBlockMutex = new Mutex(true, MutexName, out bCreatedMutex);
					if (!bCreatedMutex)
					{
						// running is not allowed (perhaps a build command is in progress)
						Program.ExitCode = 1;
						return 1;
					}
					DSBlockMutex.ReleaseMutex();
					DSBlockMutex.Dispose();
					DSBlockMutex = null;
				}
			}
			catch
			{
				Program.ExitCode = 0;
				return 0;
			}
			int.TryParse(ConfigurationManager.AppSettings["DSPort"], out Port);
			if (Port < 1 || Port > 65535)
			{
				Port = DefaultPort;
			}

			TcpClient IsServiceRunning = IsServiceRegistered();

			// parrent ID not needed anymore
			if (args[0].Equals("-iphonepackager"))
			{
				if (IsServiceRunning != null)
					return 0;
				
				TcpListener server = null;
				FileStream OutSm = null;
				TextWriter Writer = null;
				TextWriter OldConsole = Console.Out;
				try
				{
					// We were run as a 'child' process, quit when our 'parent' process exits
					// There is no parent-child relationship WRT windows, it's self-imposed.

					DeploymentProxy.Deployer = new DeploymentImplementation();

					IpcServerChannel Channel = new IpcServerChannel("iPhonePackager");
					ChannelServices.RegisterChannel(Channel, false);
					RemotingConfiguration.RegisterWellKnownServiceType(typeof(DeploymentProxy), "DeploymentServer_PID", WellKnownObjectMode.Singleton);

					long.TryParse(ConfigurationManager.AppSettings["DSTimeOut"], out TimeOut);
					if (TimeOut < 30000)
					{
						TimeOut = 30000;
					}

					server = new TcpListener(IPAddress.Any, Port);
					server.Start();

					OutSm = new FileStream("DeploymentServer.log", FileMode.Create, FileAccess.Write);
					Writer = new StreamWriter(OutSm);
					
					Console.SetOut(Writer);

					ParseServerParam(args);

					Console.WriteLine(string.Format("Deployment Server istening to port {0}", Port.ToString()));
					Console.WriteLine(string.Format("Deployment Server inactivity timeout {0}", TimeOut.ToString()));
					Console.WriteLine("---------------------------------------------------------");

					// Processing commands

					System.Threading.Thread processClient = new System.Threading.Thread(delegate ()
					{
						while (true)
						{
							TcpClient client = server.AcceptTcpClient();
							TrackTCPClient(client);
							System.Threading.Thread.Sleep(100);
						}
					});
					processClient.Start();

					// this will exit on its own after the set inactivity time or by a kill command or by a remote "stop" command
					while (true)
					{
						CoreFoundationRunLoop.RunLoopRunInMode(CoreFoundationRunLoop.kCFRunLoopDefaultMode(), 1.0, 0);
						System.Threading.Thread.Sleep(50);
						Writer.Flush();
						OutSm.Flush();
						if (ClientCounter <= 0)
						{
							if (IsStopping)
							{
								Console.WriteLine("Deployment Server IsStopping exit.");
								break;
							}
							if (GlobalTimer.ElapsedMilliseconds > TimeOut)
							{
								Console.WriteLine("Deployment Server inactivity timeout.");
								break;
							}
						}
					}
				}
				catch (SocketException e)
				{
					Console.WriteLine("SocketException: {0}", e);
				}
				catch (System.Exception Ex)
				{
					Console.WriteLine(Ex.Message);
				}
				finally
				{
					if (server != null)
						server.Stop();
					if (Writer != null)
						Writer.Close();
					if (OutSm != null)
						OutSm.Close();
				}
				Console.SetOut(OldConsole);
				Console.WriteLine("Deployment Server Stopped.");
			}
			else
			{
				if (IsServiceRunning == null)
				{
					if (args.Length < 1 || LocalCommand == "stop")
					{
						Console.WriteLine("Deployment Server not running ...");
						return 0;
					}
					Console.WriteLine("Trying to create server process ...");
					CreateDeploymentServerProcess();
				}
				else
				{
					Console.WriteLine("Found interface ...");
				}
				// Parse the command
				TcpClient Client = IsServiceRunning;
				try
				{
					int RetryCount = 3;
					while (RetryCount > 0 && Client == null)
					{
						try
						{
							if (Client == null)
							{
								Client = new TcpClient("localhost", Port); //The client gets here
							}
						}
						catch (Exception e)
						{
							RetryCount--;
							Client = null;
							if (RetryCount <= 0)
							{
								throw (e);
							}
							System.Threading.Thread.Sleep(500);
						}
					}
					StreamReader clientIn = new StreamReader(Client.GetStream());
					StreamWriter clientOut = new StreamWriter(Client.GetStream());
					{
						string Response = clientIn.ReadLine();
						if (TcpClientInfo.NeedsVersionCheck(LocalCommand))
						{
							Console.WriteLine("Got Server Path: '{0}", Response);
							if (!Response.Equals("DIR" + GetDeploymentServerPath()))
							{
								Console.WriteLine("Wrong server running, restarting the server ...");
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
										Console.WriteLine(Response);
									}
								}
								if (Client != null)
								{
									Client.Close();
								}
								Client = null;
								CreateDeploymentServerProcess();
								IsServiceRunning = IsServiceRegistered();
								Client = IsServiceRunning;
								clientIn = new StreamReader(Client.GetStream());
								clientOut = new StreamWriter(Client.GetStream());
							}
						}
					}

					for (int ArgIndex = 0; ArgIndex < args.Length; ArgIndex++)
					{
						clientOut.Write(args[ArgIndex] + ' ');
					}
					clientOut.Write("\r");
					clientOut.Flush();

					while (true)
					{
						//Console.WriteLine("Pending response ...");
						string Response = clientIn.ReadLine();
						if (Response.EndsWith("CMDOK"))
						{
							Program.ExitCode = 1;
							break;
						}
						else if (Response.EndsWith("CMDFAIL"))
						{
							Program.ExitCode = 0;
							break;
						}
						else
						{
							Console.WriteLine(Response);
						}
					}
				}
				catch (Exception e)
				{
					Console.WriteLine("Exception: {0}", e);
				}
				finally
				{
					if (Client != null)
					{
						Client.Close();
					}
				}
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
					Console.WriteLine("Command " + i.ToString() + " " + Commands[i]);
					if (i < Commands.Length - 1 || Commands[i].EndsWith("\r"))
					{
						string[] Arguments = Commands[i].Split(' ');
						if (Client.ParseCommand(Arguments, 0))
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

		private static void TrackTCPClient(TcpClient Client)
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
					Console.WriteLine("Client [{0}] connected.", ClientIP);

					TextWriter Writer = new StreamWriter(ClStream);
					
					Writer.WriteLine("DIR" + GetDeploymentServerPath());
					Writer.Flush();

					Byte[] Buffer = new Byte[2048];
					string Unprocessed = "";
					ClientInfo = new TcpClientInfo();
					
					while (true)
					{
						if (ClientInfo.HasCommand && !IsRunningCommand && !ClientInfo.IsStillRunning && !IsStopping)
						{
							ClientInfo.HasCommand = false;
							IsRunningCommand = true;

							int RetCode = 0;
							try
							{
								LastCommand = ClientInfo.LastCommand;
								RetCode = ClientInfo.RunCommand(Writer);
							}
							catch
							{

							}
							IsRunningCommand = false;

							Console.WriteLine("Command completed");
							if (!ClientInfo.IsStillRunning)
							{
								Writer.WriteLine((RetCode == 1 ? "\nCMDOK" : "\nCMDFAIL"));
								Writer.Flush();
								ClStream.Flush();
								break;
							}
						}
						
						if (ClStream.DataAvailable)
						{
							int Bytes = ClStream.Read(Buffer, 0, Buffer.Length);
							string Cmd = Unprocessed + System.Text.Encoding.ASCII.GetString(Buffer, 0, Bytes);
							//Console.WriteLine("Got data: " + Bytes.ToString());
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
								// still connected
								//Console.WriteLine("Still Connected");
							}
							catch (SocketException e)
							{
								// 10035 == WSAEWOULDBLOCK
								if (e.NativeErrorCode.Equals(10035))
								{
									//Console.WriteLine("Still Connected, but the Send would block");
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
						else if (ClientInfo.NeedsResponse)
						{
							Writer.WriteLine("\nCMDOK");
							Writer.Flush();
							ClStream.Flush();
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
						//Console.WriteLine("Pending client: " + IsRunningCommand.ToString() + " Command: " + ClientInfo.HasCommand.ToString());
					}
				}
				catch (Exception e)
				{
					Console.WriteLine("Exception: {0}", e);
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
							Console.WriteLine("Client [{0}] disconnected ({1}).", ((IPEndPoint)Client.Client.RemoteEndPoint).Address.ToString(), LastCommand);
						}
						Client.Close();
					}
				}
				ClientCounter--;
			});
			processTracker.Start();
		}
	}
}
