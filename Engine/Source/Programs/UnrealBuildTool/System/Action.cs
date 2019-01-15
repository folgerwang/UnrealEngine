// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Enumerates build action types.
	/// </summary>
	enum ActionType
	{
		BuildProject,

		Compile,

		CreateAppBundle,

		GenerateDebugInfo,

		Link,

		WriteMetadata,

		PostBuildStep,
	}

	/// <summary>
	/// A build action.
	/// </summary>
	class Action
	{
		///
		/// Preparation and Assembly (serialized)
		/// 

		/// <summary>
		/// The type of this action (for debugging purposes).
		/// </summary>
		public readonly ActionType ActionType;

		/// <summary>
		/// Every file this action depends on.  These files need to exist and be up to date in order for this action to even be considered
		/// </summary>
		public List<FileItem> PrerequisiteItems = new List<FileItem>();

		/// <summary>
		/// The files that this action produces after completing
		/// </summary>
		public List<FileItem> ProducedItems = new List<FileItem>();

		/// <summary>
		/// Items that should be deleted before running this action
		/// </summary>
		public List<FileItem> DeleteItems = new List<FileItem>();

		/// <summary>
		/// For C++ source files, specifies a dependency list file used to check changes to header files
		/// </summary>
		public FileItem DependencyListFile;

		/// <summary>
		/// Set of other actions that this action depends on. This set is built when the action graph is linked.
		/// </summary>
		public HashSet<Action> PrerequisiteActions;

		/// <summary>
		/// Directory from which to execute the program to create produced items
		/// </summary>
		public DirectoryReference WorkingDirectory = null;

		/// <summary>
		/// True if we should log extra information when we run a program to create produced items
		/// </summary>
		public bool bPrintDebugInfo = false;

		/// <summary>
		/// The command to run to create produced items
		/// </summary>
		public FileReference CommandPath = null;

		/// <summary>
		/// Command-line parameters to pass to the program
		/// </summary>
		public string CommandArguments = null;

		/// <summary>
		/// Optional friendly description of the type of command being performed, for example "Compile" or "Link".  Displayed by some executors.
		/// </summary>
		public string CommandDescription = null;

		/// <summary>
		/// Human-readable description of this action that may be displayed as status while invoking the action.  This is often the name of the file being compiled, or an executable file name being linked.  Displayed by some executors.
		/// </summary>
		public string StatusDescription = "...";

		/// <summary>
		/// If set, will be output whenever the group differs to the last executed action. Set when executing multiple targets at once.
		/// </summary>
		public List<string> GroupNames = new List<string>();

		/// <summary>
		/// True if this action is allowed to be run on a remote machine when a distributed build system is being used, such as XGE
		/// </summary>
		public bool bCanExecuteRemotely = false;

		/// <summary>
		/// True if this action is allowed to be run on a remote machine with SNDBS. Files with #import directives must be compiled locally. Also requires bCanExecuteRemotely = true.
		/// </summary>
		public bool bCanExecuteRemotelyWithSNDBS = true;

		/// <summary>
		/// True if this action is using the GCC compiler.  Some build systems may be able to optimize for this case.
		/// </summary>
		public bool bIsGCCCompiler = false;

		/// <summary>
		/// Whether the action is using a pre-compiled header to speed it up.
		/// </summary>
		public bool bIsUsingPCH = false;

		/// <summary>
		/// Whether we should log this action, whether executed locally or remotely.  This is useful for actions that take time
		/// but invoke tools without any console output.
		/// </summary>
		public bool bShouldOutputStatusDescription = true;

		/// <summary>
		/// True if any libraries produced by this action should be considered 'import libraries'
		/// </summary>
		public bool bProducesImportLibrary = false;



		///
		/// Preparation only (not serialized)
		///

		/// <summary>
		/// Total number of actions depending on this one.
		/// </summary>
		public int NumTotalDependentActions = 0;


		///
		/// Assembly only (not serialized)
		///

		/// <summary>
		/// Start time of action, optionally set by executor.
		/// </summary>
		public DateTimeOffset StartTime = DateTimeOffset.MinValue;

		/// <summary>
		/// End time of action, optionally set by executor.
		/// </summary>
		public DateTimeOffset EndTime = DateTimeOffset.MinValue;


		public Action(ActionType InActionType)
		{
			ActionType = InActionType;
		}

		public Action(BinaryArchiveReader Reader)
		{
			ActionType = (ActionType)Reader.ReadByte();
			WorkingDirectory = Reader.ReadDirectoryReference();
			bPrintDebugInfo = Reader.ReadBool();
			CommandPath = Reader.ReadFileReference();
			CommandArguments = Reader.ReadString();
			CommandDescription = Reader.ReadString();
			StatusDescription = Reader.ReadString();
			bCanExecuteRemotely = Reader.ReadBool();
			bCanExecuteRemotelyWithSNDBS = Reader.ReadBool();
			bIsGCCCompiler = Reader.ReadBool();
			bIsUsingPCH = Reader.ReadBool();
			bShouldOutputStatusDescription = Reader.ReadBool();
			bProducesImportLibrary = Reader.ReadBool();
			PrerequisiteItems = Reader.ReadList(() => Reader.ReadFileItem());
			ProducedItems = Reader.ReadList(() => Reader.ReadFileItem());
			DeleteItems = Reader.ReadList(() => Reader.ReadFileItem());
			DependencyListFile = Reader.ReadFileItem();
		}

		/// <summary>
		/// ISerializable: Called when serialized to report additional properties that should be saved
		/// </summary>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteByte((byte)ActionType);
			Writer.WriteDirectoryReference(WorkingDirectory);
			Writer.WriteBool(bPrintDebugInfo);
			Writer.WriteFileReference(CommandPath);
			Writer.WriteString(CommandArguments);
			Writer.WriteString(CommandDescription);
			Writer.WriteString(StatusDescription);
			Writer.WriteBool(bCanExecuteRemotely);
			Writer.WriteBool(bCanExecuteRemotelyWithSNDBS);
			Writer.WriteBool(bIsGCCCompiler);
			Writer.WriteBool(bIsUsingPCH);
			Writer.WriteBool(bShouldOutputStatusDescription);
			Writer.WriteBool(bProducesImportLibrary);
			Writer.WriteList(PrerequisiteItems, Item => Writer.WriteFileItem(Item));
			Writer.WriteList(ProducedItems, Item => Writer.WriteFileItem(Item));
			Writer.WriteList(DeleteItems, Item => Writer.WriteFileItem(Item));
			Writer.WriteFileItem(DependencyListFile);
		}

		/// <summary>
		/// Creates an action which calls UBT recursively
		/// </summary>
		/// <param name="Type">Type of the action</param>
		/// <param name="Arguments">Arguments for the action</param>
		/// <returns>New action instance</returns>
		public static Action CreateRecursiveAction<T>(ActionType Type, string Arguments) where T : ToolMode
		{
			ToolModeAttribute Attribute = typeof(T).GetCustomAttribute<ToolModeAttribute>();
			if(Attribute == null)
			{
				throw new BuildException("Missing ToolModeAttribute on {0}", typeof(T).Name);
			}

			Action NewAction = new Action(Type);
			NewAction.CommandPath = UnrealBuildTool.GetUBTPath();
			NewAction.CommandArguments = String.Format("-Mode={0} {1}", Attribute.Name, Arguments);
			return NewAction;
		}

		/// <summary>
		/// Finds conflicts betwee two actions, and prints them to the log
		/// </summary>
		/// <param name="Other">Other action to compare to.</param>
		/// <returns>True if any conflicts were found, false otherwise.</returns>
		public bool CheckForConflicts(Action Other)
		{
			bool bResult = true;
			if(ActionType != Other.ActionType)
			{
				LogConflict("action type is different", ActionType.ToString(), Other.ActionType.ToString());
				bResult = false;
			}
			if(!Enumerable.SequenceEqual(PrerequisiteItems, Other.PrerequisiteItems))
			{
				LogConflict("prerequisites are different", String.Join(", ", PrerequisiteItems.Select(x => x.Location)), String.Join(", ", Other.PrerequisiteItems.Select(x => x.Location)));
				bResult = false;
			}
			if(!Enumerable.SequenceEqual(DeleteItems, Other.DeleteItems))
			{
				LogConflict("deleted items are different", String.Join(", ", DeleteItems.Select(x => x.Location)), String.Join(", ", Other.DeleteItems.Select(x => x.Location)));
				bResult = false;
			}
			if(DependencyListFile != Other.DependencyListFile)
			{
				LogConflict("dependency list is different", (DependencyListFile == null)? "(none)" : DependencyListFile.AbsolutePath, (Other.DependencyListFile == null)? "(none)" : Other.DependencyListFile.AbsolutePath);
				bResult = false;
			}
			if(WorkingDirectory != Other.WorkingDirectory)
			{
				LogConflict("working directory is different", WorkingDirectory.FullName, Other.WorkingDirectory.FullName);
				bResult = false;
			}
			if(CommandPath != Other.CommandPath)
			{
				LogConflict("command path is different", CommandPath.FullName, Other.CommandPath.FullName);
				bResult = false;
			}
			if(CommandArguments != Other.CommandArguments)
			{
				LogConflict("command arguments are different", CommandArguments, Other.CommandArguments);
				bResult = false;
			}
			return bResult;
		}

		/// <summary>
		/// Adds the description of a merge error to an output message
		/// </summary>
		/// <param name="Description">Description of the difference</param>
		/// <param name="OldValue">Previous value for the field</param>
		/// <param name="NewValue">Conflicting value for the field</param>
		void LogConflict(string Description, string OldValue, string NewValue)
		{
			Log.TraceError("Unable to merge actions producing {0}: {1}", ProducedItems[0].Location.GetFileName(), Description);
			Log.TraceLog("  Previous: {0}", OldValue);
			Log.TraceLog("  Conflict: {0}", NewValue);
		}

		/// <summary>
		/// Increment the number of dependents, recursively
		/// </summary>
		/// <param name="VisitedActions">Set of visited actions</param>
		public void IncrementDependentCount(HashSet<Action> VisitedActions)
		{
			if(VisitedActions.Add(this))
			{
				NumTotalDependentActions++;
				foreach(Action PrerequisiteAction in PrerequisiteActions)
				{
					PrerequisiteAction.IncrementDependentCount(VisitedActions);
				}
			}
		}

		/// <summary>
		/// Compares two actions based on total number of dependent items, descending.
		/// </summary>
		/// <param name="A">Action to compare</param>
		/// <param name="B">Action to compare</param>
		public static int Compare(Action A, Action B)
		{
			// Primary sort criteria is total number of dependent files, up to max depth.
			if (B.NumTotalDependentActions != A.NumTotalDependentActions)
			{
				return Math.Sign(B.NumTotalDependentActions - A.NumTotalDependentActions);
			}
			// Secondary sort criteria is number of pre-requisites.
			else
			{
				return Math.Sign(B.PrerequisiteItems.Count - A.PrerequisiteItems.Count);
			}
		}

		public override string ToString()
		{
			string ReturnString = "";
			if (CommandPath != null)
			{
				ReturnString += CommandPath + " - ";
			}
			if (CommandArguments != null)
			{
				ReturnString += CommandArguments;
			}
			return ReturnString;
		}

		/// <summary>
		/// Returns the amount of time that this action is or has been executing in.
		/// </summary>
		public TimeSpan Duration
		{
			get
			{
				if (EndTime == DateTimeOffset.MinValue)
				{
					return DateTimeOffset.Now - StartTime;
				}

				return EndTime - StartTime;
			}
		}
	}
}
