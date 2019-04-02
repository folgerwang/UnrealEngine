// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;

namespace Tools.DotNETCommon
{
	public static class AssemblyUtils
	{
        /// <summary>
        /// Gets the original location (path and filename) of an assembly.
        /// This method is using Assembly.CodeBase property to properly resolve original
        /// assembly path in case shadow copying is enabled.
        /// </summary>
        /// <returns>Absolute path and filename to the assembly.</returns>
        public static string GetOriginalLocation(this Assembly ThisAssembly)
        {
            return new Uri(ThisAssembly.CodeBase).LocalPath;
        }
    
        /// <summary>
        /// Version info of the executable which runs this code.
        /// </summary>
        public static FileVersionInfo ExecutableVersion
        {
            get
            {
                return FileVersionInfo.GetVersionInfo(Assembly.GetEntryAssembly().GetOriginalLocation());
            }
        }

        /// <summary>
        /// Installs an assembly resolver. Mostly used to get shared assemblies that we don't want copied around to various output locations as happens when "Copy Local" is set to true
        /// for an assembly reference (which is the default).
        /// </summary>
        public static void InstallAssemblyResolver(string PathToBinariesDotNET)
        {
			AppDomain.CurrentDomain.AssemblyResolve += (sender, args) =>
            {
			    // Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
			    string AssemblyName = args.Name.Split(',')[0];

                // Look for known assembly names we check into Binaries/DotNET/. Return null if we can't find it.
                return (
                    from KnownAssemblyName in new[] { "SwarmAgent.exe", "Ionic.Zip.Reduced.dll" }
                    where AssemblyName.Equals(Path.GetFileNameWithoutExtension(KnownAssemblyName), StringComparison.InvariantCultureIgnoreCase)
                    let ResolvedAssemblyFilename = Path.Combine(PathToBinariesDotNET, KnownAssemblyName)
                    // check if the file exists first. If we just try to load it, we correctly throw an exception, but it's a generic
                    // FileNotFoundException, which is not informative. Better to return null.
                    select File.Exists(ResolvedAssemblyFilename) ? Assembly.LoadFile(ResolvedAssemblyFilename) : null
                    ).FirstOrDefault();
            };
        }

		/// <summary>
		/// Installs an assembly resolver, which will load *any* assembly which exists recursively within the supplied folder.
		/// </summary>
		/// <param name="RootDirectory">The directory to enumerate.</param>
		public static void InstallRecursiveAssemblyResolver(string RootDirectory)
		{
			// Our Dictionary<string,string> will be used to hold the mapping of assembly name to path on disk. It will be captured by the AssemblyResolve lambda below.
			Dictionary<string, string> AssemblyLocationCache = new Dictionary<string, string>();
			// Create a temporary dictionary to track last modified date of each assembly, so we can ensure we always reference the latest one in the case of stale assemblies on disk.
			Dictionary<string, DateTime> AssemblyWriteTimes = new Dictionary<string, DateTime>();
			// Initialize our cache of assemblies by enumerating all files in the given folder.
			foreach (string DiscoveredAssembly in Directory.EnumerateFiles(RootDirectory, "*.dll", SearchOption.AllDirectories))
			{
				string AssemblyName = Path.GetFileNameWithoutExtension(DiscoveredAssembly);
				DateTime AssemblyLastWriteTime = File.GetLastWriteTimeUtc(DiscoveredAssembly);
				if (AssemblyLocationCache.ContainsKey(AssemblyName))
				{
					// We already have this assembly in our cache. Only replace it if the discovered file is newer (to avoid stale assemblies breaking stuff).
					if (AssemblyLastWriteTime > AssemblyWriteTimes[AssemblyName])
					{
						AssemblyLocationCache[AssemblyName] = DiscoveredAssembly;
						AssemblyWriteTimes[AssemblyName] = AssemblyLastWriteTime;
					}
				}
				else
				{
					// This is the first copy of this assembly ... add it to our cache.
					AssemblyLocationCache.Add(AssemblyName, DiscoveredAssembly);
					AssemblyWriteTimes.Add(AssemblyName, AssemblyLastWriteTime);
				}
			}
			AppDomain.CurrentDomain.AssemblyResolve += (sender, args) =>
			{
				// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
				string AssemblyName = args.Name.Split(',')[0];
				if (AssemblyLocationCache.ContainsKey(AssemblyName))
				{
					// We have this assembly in our folder.
					string AssemblyLocation = AssemblyLocationCache[AssemblyName];
					if (File.Exists(AssemblyLocation))
					{
						// The assembly still exists, so load it.
						return Assembly.LoadFile(AssemblyLocation);
					}
					else
					{
						// The assembly no longer exists on disk, so remove it from our cache.
						AssemblyLocationCache.Remove(AssemblyName);
					}
				}
				return null;
			};
		}
    }
}
