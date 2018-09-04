// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	interface IPerforceModalTask
	{
		bool Run(PerforceConnection Perforce, TextWriter Log, out string ErrorMessage);
	}

	class PerforceModalTask : IModalTask
	{
		public string ProjectFileName;
		public string ServerAndPort;
		public string UserName;
		public string Password;
		public LoginResult LoginResult;
		public IPerforceModalTask InnerTask;
		public TextWriter Log;

		public PerforceModalTask(string ProjectFileName, string ServerAndPort, string UserName, IPerforceModalTask InnerTask, TextWriter Log)
		{
			this.ProjectFileName = ProjectFileName;
			this.ServerAndPort = ServerAndPort;
			this.UserName = UserName;
			this.InnerTask = InnerTask;
			this.Log = Log;
		}

		public static bool TryGetServerSettings(string ProjectFileName, ref string ServerAndPort, ref string UserName, TextWriter Log)
		{
			// Read the P4PORT setting for the server, if necessary. Change to the project folder if set, so we can respect the contents of any P4CONFIG file.
			if(ServerAndPort == null)
			{
				string PrevDirectory = Directory.GetCurrentDirectory();
				try
				{
					PerforceConnection Perforce = new PerforceConnection(UserName, null, null);
					if(ProjectFileName != null)
					{
						try
						{
							Directory.SetCurrentDirectory(Path.GetDirectoryName(ProjectFileName));
						}
						catch
						{
							// Just ignore
						}
					}

					string NewServerAndPort;
					if (Perforce.GetSetting("P4PORT", out NewServerAndPort, Log))
					{
						ServerAndPort = NewServerAndPort;
					}
				}
				finally
				{
					Directory.SetCurrentDirectory(PrevDirectory);
				}
			}

			// Update the server and username from the reported server info if it's not set
			if(ServerAndPort == null || UserName == null)
			{
				PerforceConnection Perforce = new PerforceConnection(UserName, null, ServerAndPort);

				PerforceInfoRecord PerforceInfo;
				if(!Perforce.Info(out PerforceInfo, Log))
				{
					return false;
				}
				if(ServerAndPort == null && !String.IsNullOrEmpty(PerforceInfo.ServerAddress))
				{
					ServerAndPort = PerforceInfo.ServerAddress;
				}
				if(UserName == null && !String.IsNullOrEmpty(PerforceInfo.UserName))
				{
					UserName = PerforceInfo.UserName;
				}
				if(ServerAndPort == null || UserName == null)
				{
					return false;
				}
			}

			// Otherwise succeed
			return true;
		}

		public bool Run(out string ErrorMessage)
		{
			// Set the default login state to failed
			LoginResult = LoginResult.Failed;

			// Get the server settings
			if(!TryGetServerSettings(ProjectFileName, ref ServerAndPort, ref UserName, Log))
			{
				ErrorMessage = "Unable to get Perforce server settings.";
				return false;
			}

			// Create the connection
			PerforceConnection Perforce = new PerforceConnection(UserName, null, ServerAndPort);

			// If we've got a password, execute the login command
			if(Password != null)
			{
				string PasswordErrorMessage;
				LoginResult = Perforce.Login(Password, out PasswordErrorMessage, Log);
				if(LoginResult != LoginResult.Succeded)
				{
					Log.WriteLine(PasswordErrorMessage);
					ErrorMessage = String.Format("Unable to login: {0}", PasswordErrorMessage);
					return false;
				}
			}

			// Check that we're logged in
			bool bLoggedIn;
			if(!Perforce.GetLoggedInState(out bLoggedIn, Log))
			{
				ErrorMessage = "Unable to get login status.";
				return false;
			}
			if(!bLoggedIn)
			{
				LoginResult = LoginResult.MissingPassword;
				ErrorMessage = "User is not logged in to Perforce.";
				Log.WriteLine(ErrorMessage);
				return false;
			}

			// Execute the inner task
			LoginResult = LoginResult.Succeded;
			return InnerTask.Run(Perforce, Log, out ErrorMessage);
		}

		public static ModalTaskResult Execute(IWin32Window Owner, string ProjectFileName, string ServerAndPort, string UserName, IPerforceModalTask PerforceTask, string Title, string Message, TextWriter Log, out string ErrorMessage)
		{
			PerforceModalTask Task = new PerforceModalTask(ProjectFileName, ServerAndPort, UserName, PerforceTask, Log);
			for(;;)
			{
				string TaskErrorMessage;
				ModalTaskResult TaskResult = ModalTask.Execute(Owner, Task, Title, Message, out TaskErrorMessage);

				if(Task.LoginResult == LoginResult.Succeded)
				{
					ErrorMessage = TaskErrorMessage;
					return TaskResult;
				}
				else if(Task.LoginResult == LoginResult.MissingPassword)
				{
					PasswordWindow PasswordWindow = new PasswordWindow(String.Format("Enter the password for user '{0}' on server '{1}'.", Task.UserName, Task.ServerAndPort), Task.Password);
					if(PasswordWindow.ShowDialog() != DialogResult.OK)
					{
						ErrorMessage = null;
						return ModalTaskResult.Aborted;
					}
					Task.Password = PasswordWindow.Password;
				}
				else if(Task.LoginResult == LoginResult.IncorrectPassword)
				{
					PasswordWindow PasswordWindow = new PasswordWindow(String.Format("Authentication failed. Enter the password for user '{0}' on server '{1}'.", Task.UserName, Task.ServerAndPort), Task.Password);
					if(PasswordWindow.ShowDialog() != DialogResult.OK)
					{
						ErrorMessage = null;
						return ModalTaskResult.Aborted;
					}
					Task.Password = PasswordWindow.Password;
				}
				else
				{
					ErrorMessage = TaskErrorMessage;
					return ModalTaskResult.Failed;
				}
			}
		}
	}
}
