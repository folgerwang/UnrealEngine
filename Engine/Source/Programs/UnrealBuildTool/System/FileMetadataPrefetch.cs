// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Prefetches metadata from the filesystem, by populating FileItem and DirectoryItem objects for requested directory trees. Since 
	/// </summary>
	static class FileMetadataPrefetch
	{
		/// <summary>
		/// Queue for tasks added to the thread pool
		/// </summary>
		static ThreadPoolWorkQueue Queue = new ThreadPoolWorkQueue();

		/// <summary>
		/// Used to cancel any queued tasks
		/// </summary>
		static CancellationTokenSource CancelSource = new CancellationTokenSource();

		/// <summary>
		/// The cancellation token
		/// </summary>
		static CancellationToken CancelToken = CancelSource.Token;

		/// <summary>
		/// Set of all the directory trees that have been queued up, to save adding any more than once.
		/// </summary>
		static HashSet<DirectoryReference> QueuedDirectories = new HashSet<DirectoryReference>();

		/// <summary>
		/// Enqueue the engine directory for prefetching
		/// </summary>
		public static void QueueEngineDirectory()
		{
			lock(QueuedDirectories)
			{
				if(QueuedDirectories.Add(UnrealBuildTool.EngineDirectory))
				{
					Enqueue(() => ScanEngineDirectory());
				}
			}
		}

		/// <summary>
		/// Enqueue a project directory for prefetching
		/// </summary>
		/// <param name="ProjectDirectory">The project directory to prefetch</param>
		public static void QueueProjectDirectory(DirectoryReference ProjectDirectory)
		{
			lock(QueuedDirectories)
			{
				if(QueuedDirectories.Add(ProjectDirectory))
				{
					Enqueue(() => ScanProjectDirectory(DirectoryItem.GetItemByDirectoryReference(ProjectDirectory)));
				}
			}
		}

		/// <summary>
		/// Enqueue a directory tree for prefetching
		/// </summary>
		/// <param name="Directory">Directory to start searching from</param>
		public static void QueueDirectoryTree(DirectoryReference Directory)
		{
			lock(QueuedDirectories)
			{
				if(QueuedDirectories.Add(Directory))
				{
					Enqueue(() => ScanDirectoryTree(DirectoryItem.GetItemByDirectoryReference(Directory)));
				}
			}
		}

		/// <summary>
		/// Wait for the prefetcher to complete all reqeusted tasks
		/// </summary>
		public static void Wait()
		{
			Queue.Wait();
		}

		/// <summary>
		/// Stop prefetching items, and cancel all pending tasks. synchronous.
		/// </summary>
		public static void Stop()
		{
			CancelSource.Cancel();
			Queue.Wait();
		}

		/// <summary>
		/// Enqueue a task which checks for the cancellation token first
		/// </summary>
		/// <param name="Action">Action to enqueue</param>
		static void Enqueue(System.Action Action)
		{
			Queue.Enqueue(() => { if(!CancelToken.IsCancellationRequested){ Action(); } });
		}

		/// <summary>
		/// Scans the engine directory, adding tasks for subdirectories
		/// </summary>
		static void ScanEngineDirectory()
		{
			DirectoryItem EngineDirectory = DirectoryItem.GetItemByDirectoryReference(UnrealBuildTool.EngineDirectory);
			EngineDirectory.CacheDirectories();

			DirectoryItem EnginePluginsDirectory = DirectoryItem.Combine(EngineDirectory, "Plugins");
			Enqueue(() => ScanPluginFolder(EnginePluginsDirectory));

			DirectoryItem EngineRuntimeDirectory = DirectoryItem.GetItemByDirectoryReference(UnrealBuildTool.EngineSourceRuntimeDirectory);
			Enqueue(() => ScanDirectoryTree(EngineRuntimeDirectory));

			DirectoryItem EngineDeveloperDirectory = DirectoryItem.GetItemByDirectoryReference(UnrealBuildTool.EngineSourceDeveloperDirectory);
			Enqueue(() => ScanDirectoryTree(EngineDeveloperDirectory));

			DirectoryItem EngineEditorDirectory = DirectoryItem.GetItemByDirectoryReference(UnrealBuildTool.EngineSourceEditorDirectory);
			Enqueue(() => ScanDirectoryTree(EngineEditorDirectory));
		}

		/// <summary>
		/// Scans a project directory, adding tasks for subdirectories
		/// </summary>
		/// <param name="ProjectDirectory">The project directory to search</param>
		static void ScanProjectDirectory(DirectoryItem ProjectDirectory)
		{
			DirectoryItem ProjectPluginsDirectory = DirectoryItem.Combine(ProjectDirectory, "Plugins");
			Enqueue(() => ScanPluginFolder(ProjectPluginsDirectory));

			DirectoryItem ProjectSourceDirectory = DirectoryItem.Combine(ProjectDirectory, "Source");
			Enqueue(() => ScanDirectoryTree(ProjectSourceDirectory));
		}

		/// <summary>
		/// Scans a plugin parent directory, adding tasks for subdirectories
		/// </summary>
		/// <param name="Directory">The directory which may contain plugin directories</param>
		static void ScanPluginFolder(DirectoryItem Directory)
		{
			foreach(DirectoryItem SubDirectory in Directory.EnumerateDirectories())
			{
				if(SubDirectory.EnumerateFiles().Any(x => x.HasExtension(".uplugin")))
				{
					Enqueue(() => ScanDirectoryTree(DirectoryItem.Combine(SubDirectory, "Source")));
				}
				else
				{
					Enqueue(() => ScanPluginFolder(SubDirectory));
				}
			}
		}

		/// <summary>
		/// Scans an arbitrary directory tree
		/// </summary>
		/// <param name="Directory">Root of the directory tree</param>
		static void ScanDirectoryTree(DirectoryItem Directory)
		{
			foreach(DirectoryItem SubDirectory in Directory.EnumerateDirectories())
			{
				Enqueue(() => ScanDirectoryTree(SubDirectory));
			}
			Directory.CacheFiles();
		}
	}
}
