// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using System.Text;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores information about a Visual C++ installation and compile environment
	/// </summary>
	class VCEnvironment
	{
		/// <summary>
		/// The compiler version
		/// </summary>
		public readonly WindowsCompiler Compiler;

		/// <summary>
		/// The compiler directory
		/// </summary>
		public readonly DirectoryReference CompilerDir;

		/// <summary>
		/// The compiler version
		/// </summary>
		public readonly VersionNumber CompilerVersion;

		/// <summary>
		/// The underlying toolchain to use. Using Clang/ICL will piggy-back on a Visual Studio toolchain for the CRT, linker, etc...
		/// </summary>
		public readonly WindowsCompiler ToolChain;

		/// <summary>
		/// Root directory containing the toolchain
		/// </summary>
		public readonly DirectoryReference ToolChainDir;

		/// <summary>
		/// The toolchain version number
		/// </summary>
		public readonly VersionNumber ToolChainVersion;
		
		/// <summary>
		/// Root directory containing the Windows Sdk
		/// </summary>
		public readonly DirectoryReference WindowsSdkDir;

		/// <summary>
		/// Version number of the Windows Sdk
		/// </summary>
		public readonly VersionNumber WindowsSdkVersion;

		/// <summary>
		/// The path to the linker for linking executables
		/// </summary>
		public readonly FileReference CompilerPath;

		/// <summary>
		/// The path to the linker for linking executables
		/// </summary>
		public readonly FileReference LinkerPath;

		/// <summary>
		/// The path to the linker for linking libraries
		/// </summary>
		public readonly FileReference LibraryManagerPath;

		/// <summary>
		/// Path to the resource compiler from the Windows SDK
		/// </summary>
		public readonly FileReference ResourceCompilerPath;

		/// <summary>
		/// The default system include paths
		/// </summary>
		public readonly List<DirectoryReference> IncludePaths = new List<DirectoryReference>();

		/// <summary>
		/// The default system library paths
		/// </summary>
		public readonly List<DirectoryReference> LibraryPaths = new List<DirectoryReference>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Platform">The platform to find the compiler for</param>
		/// <param name="Compiler">The compiler to use</param>
		/// <param name="CompilerDir">The compiler directory</param>
		/// <param name="CompilerVersion">The compiler version number</param>
		/// <param name="ToolChain">The base toolchain version</param>
		/// <param name="ToolChainDir">Directory containing the toolchain</param>
		/// <param name="ToolChainVersion">Version of the toolchain</param>
		/// <param name="WindowsSdkDir">Root directory containing the Windows Sdk</param>
		/// <param name="WindowsSdkVersion">Version of the Windows Sdk</param>
		public VCEnvironment(CppPlatform Platform, WindowsCompiler Compiler, DirectoryReference CompilerDir, VersionNumber CompilerVersion, WindowsCompiler ToolChain, DirectoryReference ToolChainDir, VersionNumber ToolChainVersion, DirectoryReference WindowsSdkDir, VersionNumber WindowsSdkVersion)
		{
			this.Compiler = Compiler;
			this.CompilerDir = CompilerDir;
			this.CompilerVersion = CompilerVersion;
			this.ToolChain = ToolChain;
			this.ToolChainDir = ToolChainDir;
			this.ToolChainVersion = ToolChainVersion;
			this.WindowsSdkDir = WindowsSdkDir;
			this.WindowsSdkVersion = WindowsSdkVersion;

			// Get the standard VC paths
			DirectoryReference VCToolPath32 = GetVCToolPath32(ToolChain, ToolChainDir);
			DirectoryReference VCToolPath64 = GetVCToolPath64(ToolChain, ToolChainDir);

            // Compile using 64 bit tools for 64 bit targets, and 32 for 32.
			CompilerPath = GetCompilerToolPath(Platform, Compiler, CompilerDir);

			// Regardless of the target, if we're linking on a 64 bit machine, we want to use the 64 bit linker (it's faster than the 32 bit linker and can handle large linking jobs)
			DirectoryReference DefaultLinkerDir = VCToolPath64;
			LinkerPath = GetLinkerToolPath(Platform, Compiler, DefaultLinkerDir);
			LibraryManagerPath = GetLibraryLinkerToolPath(Platform, Compiler, DefaultLinkerDir);

			// Get the resource compiler path from the Windows SDK
			ResourceCompilerPath = GetResourceCompilerToolPath(Platform, WindowsSdkDir, WindowsSdkVersion);

			// Add both toolchain paths to the PATH environment variable. There are some support DLLs which are only added to one of the paths, but which the toolchain in the other directory
			// needs to run (eg. mspdbcore.dll).
			if(Platform == CppPlatform.Win64)
			{
				AddDirectoryToPath(VCToolPath64);
				AddDirectoryToPath(VCToolPath32);
			}
			if(Platform == CppPlatform.Win32)
			{
				AddDirectoryToPath(VCToolPath32);
				AddDirectoryToPath(VCToolPath64);
			}

			// Get all the system include paths
			SetupEnvironment(Platform);
		}

		/// <summary>
		/// Add a directory to the PATH environment variable
		/// </summary>
		/// <param name="ToolPath">The path to add</param>
		static void AddDirectoryToPath(DirectoryReference ToolPath)
		{
            string PathEnvironmentVariable = Environment.GetEnvironmentVariable("PATH") ?? "";
            if (!PathEnvironmentVariable.Split(';').Any(x => String.Compare(x, ToolPath.FullName, true) == 0))
            {
                PathEnvironmentVariable = ToolPath.FullName + ";" + PathEnvironmentVariable;
                Environment.SetEnvironmentVariable("PATH", PathEnvironmentVariable);
            }
		}

		/// <summary>
		/// Gets the path to the 32bit tool binaries.
		/// </summary>
		/// <param name="Compiler">The compiler version</param>
		/// <param name="VCToolChainDir">Base directory for the VC toolchain</param>
		/// <returns>Directory containing the 32-bit toolchain binaries</returns>
		static DirectoryReference GetVCToolPath32(WindowsCompiler Compiler, DirectoryReference VCToolChainDir)
		{
			if (Compiler >= WindowsCompiler.VisualStudio2017)
			{
				FileReference NativeCompilerPath = FileReference.Combine(VCToolChainDir, "bin", "HostX64", "x86", "cl.exe");
				if (FileReference.Exists(NativeCompilerPath))
				{
					return NativeCompilerPath.Directory;
				}

				FileReference CrossCompilerPath = FileReference.Combine(VCToolChainDir, "bin", "HostX86", "x86", "cl.exe");
				if (FileReference.Exists(CrossCompilerPath))
				{
					return CrossCompilerPath.Directory;
				}

				throw new BuildException("No 32-bit compiler toolchain found in {0} or {1}", NativeCompilerPath, CrossCompilerPath);
			}
			else
			{
				FileReference CompilerPath = FileReference.Combine(VCToolChainDir, "bin", "cl.exe");
				if (FileReference.Exists(CompilerPath))
				{
					return CompilerPath.Directory;
				}
				throw new BuildException("No 32-bit compiler toolchain found in {0}", CompilerPath);
			}
		}

		/// <summary>
		/// Gets the path to the 64bit tool binaries.
		/// </summary>
		/// <param name="Compiler">The version of the compiler being used</param>
		/// <param name="VCToolChainDir">Base directory for the VC toolchain</param>
		/// <returns>Directory containing the 64-bit toolchain binaries</returns>
		static DirectoryReference GetVCToolPath64(WindowsCompiler Compiler, DirectoryReference VCToolChainDir)
		{
			if (Compiler >= WindowsCompiler.VisualStudio2017)
			{
				// Use the native 64-bit compiler if present
				FileReference NativeCompilerPath = FileReference.Combine(VCToolChainDir, "bin", "HostX64", "x64", "cl.exe");
				if (FileReference.Exists(NativeCompilerPath))
				{
					return NativeCompilerPath.Directory;
				}

				// Otherwise try the x64-on-x86 compiler. VS Express only includes the latter.
				FileReference CrossCompilerPath = FileReference.Combine(VCToolChainDir, "bin", "HostX86", "x64", "cl.exe");
				if (FileReference.Exists(CrossCompilerPath))
				{
					return CrossCompilerPath.Directory;
				}

				throw new BuildException("No 64-bit compiler toolchain found in {0} or {1}", NativeCompilerPath, CrossCompilerPath);
			}
			else
			{
				// Use the native 64-bit compiler if present
				FileReference NativeCompilerPath = FileReference.Combine(VCToolChainDir, "bin", "amd64", "cl.exe");
				if (FileReference.Exists(NativeCompilerPath))
				{
					return NativeCompilerPath.Directory;
				}

				// Otherwise use the amd64-on-x86 compiler. VS2012 Express only includes the latter.
				FileReference CrossCompilerPath = FileReference.Combine(VCToolChainDir, "bin", "x86_amd64", "cl.exe");
				if (FileReference.Exists(CrossCompilerPath))
				{
					return CrossCompilerPath.Directory;
				}

				throw new BuildException("No 64-bit compiler toolchain found in {0} or {1}", NativeCompilerPath, CrossCompilerPath);
			}
		}

		/// <summary>
		/// Gets the path to the compiler.
		/// </summary>
		static FileReference GetCompilerToolPath(CppPlatform Platform, WindowsCompiler Compiler, DirectoryReference CompilerDir)
		{
			if (Compiler == WindowsCompiler.Clang)
			{
				return FileReference.Combine(CompilerDir, "bin", "clang-cl.exe");
			}
			else if(Compiler == WindowsCompiler.Intel)
			{
				if(Platform == CppPlatform.Win32)
				{
					return FileReference.Combine(CompilerDir, "bin", "ia32", "icl.exe");
				}
				else
				{
					return FileReference.Combine(CompilerDir, "bin", "intel64", "icl.exe");
				}
			}
			else
			{
				if(Platform == CppPlatform.Win32)
				{
					return FileReference.Combine(GetVCToolPath32(Compiler, CompilerDir), "cl.exe");
				}
				else
				{
					return FileReference.Combine(GetVCToolPath64(Compiler, CompilerDir), "cl.exe");
				}
			}
		}

		/// <summary>
		/// Gets the path to the linker.
		/// </summary>
		static FileReference GetLinkerToolPath(CppPlatform Platform, WindowsCompiler Compiler, DirectoryReference DefaultLinkerDir)
		{
			// If we were asked to use Clang, then we'll redirect the path to the compiler to the LLVM installation directory
			if (Compiler == WindowsCompiler.Clang && WindowsPlatform.bAllowClangLinker)
			{
				FileReference LinkerPath = FileReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.ProgramFiles), "LLVM", "bin", "lld.exe");
				if (FileReference.Exists(LinkerPath))
				{
					return LinkerPath;
				}

				FileReference LinkerPathX86 = FileReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.ProgramFilesX86), "LLVM", "bin", "lld.exe");
				if (FileReference.Exists(LinkerPathX86))
				{
					return LinkerPathX86;
				}

				throw new BuildException("Clang was selected as the Windows compiler, but {0} and {1} were not found.", LinkerPath, LinkerPathX86);
			}
			else if(Compiler == WindowsCompiler.Intel && WindowsPlatform.bAllowICLLinker)
			{
				FileReference LinkerPath = FileReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.ProgramFilesX86), "IntelSWTools", "compilers_and_libraries", "windows", "bin", (Platform == CppPlatform.Win32)? "ia32" : "intel64", "xilink.exe");
				if (FileReference.Exists(LinkerPath))
				{
					return LinkerPath;
				}

				throw new BuildException("ICL was selected as the Windows compiler, but {0} was not found.", LinkerPath);
			}
			else
			{
				return FileReference.Combine(DefaultLinkerDir, "link.exe");
			}
		}

		/// <summary>
		/// Gets the path to the library linker.
		/// </summary>
		static FileReference GetLibraryLinkerToolPath(CppPlatform Platform, WindowsCompiler Compiler, DirectoryReference DefaultLinkerDir)
		{
			// Regardless of the target, if we're linking on a 64 bit machine, we want to use the 64 bit linker (it's faster than the 32 bit linker)
			if (Compiler == WindowsCompiler.Intel && WindowsPlatform.bAllowICLLinker)
			{
				FileReference LibPath = FileReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.ProgramFilesX86), "IntelSWTools", "compilers_and_libraries", "windows", "bin", Platform == CppPlatform.Win32 ? "ia32" : "intel64", "xilib.exe");
				if (FileReference.Exists(LibPath))
				{
					return LibPath;
				}

				throw new BuildException("ICL was selected as the Windows compiler, but does not appear to be installed.  Could not find: " + LibPath);
			}
			else
			{
				return FileReference.Combine(DefaultLinkerDir, "lib.exe");
			}
		}

		/// <summary>
		/// Gets the path to the resource compiler's rc.exe for the specified platform.
		/// </summary>
		static FileReference GetResourceCompilerToolPath(CppPlatform Platform, DirectoryReference WindowsSdkDir, VersionNumber WindowsSdkVersion)
		{
			// 64 bit -- we can use the 32 bit version to target 64 bit on 32 bit OS.
			if (Platform == CppPlatform.Win64)
			{
				FileReference ResourceCompilerPath = FileReference.Combine(WindowsSdkDir, "bin", WindowsSdkVersion.ToString(), "x64", "rc.exe");
				if(FileReference.Exists(ResourceCompilerPath))
				{
					return ResourceCompilerPath;
				}

				ResourceCompilerPath = FileReference.Combine(WindowsSdkDir, "bin", "x64", "rc.exe");
				if(FileReference.Exists(ResourceCompilerPath))
				{
					return ResourceCompilerPath;
				}
			}
			else
			{
				FileReference ResourceCompilerPath = FileReference.Combine(WindowsSdkDir, "bin", WindowsSdkVersion.ToString(), "x86", "rc.exe");
				if(FileReference.Exists(ResourceCompilerPath))
				{
					return ResourceCompilerPath;
				}

				ResourceCompilerPath = FileReference.Combine(WindowsSdkDir, "bin", "x86", "rc.exe");
				if(FileReference.Exists(ResourceCompilerPath))
				{
					return ResourceCompilerPath;
				}
			}
			throw new BuildException("Unable to find path to the Windows resource compiler under {0} (version {1})", WindowsSdkDir, WindowsSdkVersion);
		}

		/// <summary>
		/// Sets up the standard compile environment for the toolchain
		/// </summary>
		private void SetupEnvironment(CppPlatform Platform)
		{
			// Add the standard Visual C++ include paths
			IncludePaths.Add(DirectoryReference.Combine(ToolChainDir, "INCLUDE"));

			// Add the standard Visual C++ library paths
			if (ToolChain >= WindowsCompiler.VisualStudio2017)
			{
				if (Platform == CppPlatform.Win32)
				{
					LibraryPaths.Add(DirectoryReference.Combine(ToolChainDir, "lib", "x86"));
				}
				else
				{
					LibraryPaths.Add(DirectoryReference.Combine(ToolChainDir, "lib", "x64"));
				}
			}
			else
			{
				if (Platform == CppPlatform.Win32)
				{
					LibraryPaths.Add(DirectoryReference.Combine(ToolChainDir, "LIB"));
				}
				else
				{
					LibraryPaths.Add(DirectoryReference.Combine(ToolChainDir, "LIB", "amd64"));
				}
			}

			// If we're on Visual Studio 2015 and using pre-Windows 10 SDK, we need to find a Windows 10 SDK and add the UCRT include paths
			if(ToolChain >= WindowsCompiler.VisualStudio2015_DEPRECATED && WindowsSdkVersion < new VersionNumber(10))
			{
				KeyValuePair<VersionNumber, DirectoryReference> Pair = WindowsPlatform.FindUniversalCrtDirs().OrderByDescending(x => x.Key).FirstOrDefault();
				if(Pair.Key == null || Pair.Key < new VersionNumber(10))
				{
					throw new BuildException("{0} requires the Universal CRT to be installed.", WindowsPlatform.GetCompilerName(ToolChain));
				}

				DirectoryReference IncludeRootDir = DirectoryReference.Combine(Pair.Value, "include", Pair.Key.ToString());
				IncludePaths.Add(DirectoryReference.Combine(IncludeRootDir, "ucrt"));

				DirectoryReference LibraryRootDir = DirectoryReference.Combine(Pair.Value, "lib", Pair.Key.ToString());
				if(Platform == CppPlatform.Win64)
				{
					LibraryPaths.Add(DirectoryReference.Combine(LibraryRootDir, "ucrt", "x64"));
				}
				else
				{
					LibraryPaths.Add(DirectoryReference.Combine(LibraryRootDir, "ucrt", "x86"));
				}
			}

			// Add the NETFXSDK include path. We need this for SwarmInterface.
			DirectoryReference NetFxSdkDir;
			if(WindowsPlatform.TryGetNetFxSdkInstallDir(out NetFxSdkDir))
			{
				IncludePaths.Add(DirectoryReference.Combine(NetFxSdkDir, "include", "um"));
				if (Platform == CppPlatform.Win32)
				{
					LibraryPaths.Add(DirectoryReference.Combine(NetFxSdkDir, "lib", "um", "x86"));
				}
				else
				{
					LibraryPaths.Add(DirectoryReference.Combine(NetFxSdkDir, "lib", "um", "x64"));
				}
			}

			// Add the Windows SDK paths
			if (WindowsSdkVersion >= new VersionNumber(10))
			{
				DirectoryReference IncludeRootDir = DirectoryReference.Combine(WindowsSdkDir, "include", WindowsSdkVersion.ToString());
				IncludePaths.Add(DirectoryReference.Combine(IncludeRootDir, "ucrt"));
				IncludePaths.Add(DirectoryReference.Combine(IncludeRootDir, "shared"));
				IncludePaths.Add(DirectoryReference.Combine(IncludeRootDir, "um"));
				IncludePaths.Add(DirectoryReference.Combine(IncludeRootDir, "winrt"));

				DirectoryReference LibraryRootDir = DirectoryReference.Combine(WindowsSdkDir, "lib", WindowsSdkVersion.ToString());
				if(Platform == CppPlatform.Win64)
				{
					LibraryPaths.Add(DirectoryReference.Combine(LibraryRootDir, "ucrt", "x64"));
					LibraryPaths.Add(DirectoryReference.Combine(LibraryRootDir, "um", "x64"));
				}
				else
				{
					LibraryPaths.Add(DirectoryReference.Combine(LibraryRootDir, "ucrt", "x86"));
					LibraryPaths.Add(DirectoryReference.Combine(LibraryRootDir, "um", "x86"));
				}
			}
			else
			{
				DirectoryReference IncludeRootDir = DirectoryReference.Combine(WindowsSdkDir, "include");
				IncludePaths.Add(DirectoryReference.Combine(IncludeRootDir, "shared"));
				IncludePaths.Add(DirectoryReference.Combine(IncludeRootDir, "um"));
				IncludePaths.Add(DirectoryReference.Combine(IncludeRootDir, "winrt"));

				DirectoryReference LibraryRootDir = DirectoryReference.Combine(WindowsSdkDir, "lib", "winv6.3");
				if(Platform == CppPlatform.Win64)
				{
					LibraryPaths.Add(DirectoryReference.Combine(LibraryRootDir, "um", "x64"));
				}
				else
				{
					LibraryPaths.Add(DirectoryReference.Combine(LibraryRootDir, "um", "x86"));
				}
			}
		}

		/// <summary>
		/// Creates an environment with the given settings
		/// </summary>
		/// <param name="Compiler">The compiler version to use</param>
		/// <param name="Platform">The platform to target</param>
		/// <param name="CompilerVersion">The specific toolchain version to use</param>
		/// <param name="WindowsSdkVersion">Version of the Windows SDK to use</param>
		/// <returns>New environment object with paths for the given settings</returns>
		public static VCEnvironment Create(WindowsCompiler Compiler, CppPlatform Platform, string CompilerVersion, string WindowsSdkVersion)
		{
			// Get the compiler version info
			VersionNumber SelectedCompilerVersion;
			DirectoryReference SelectedCompilerDir;
			if(!WindowsPlatform.TryGetToolChainDir(Compiler, CompilerVersion, out SelectedCompilerVersion, out SelectedCompilerDir))
			{
				throw new BuildException("{0}{1} must be installed in order to build this target.", WindowsPlatform.GetCompilerName(Compiler), String.IsNullOrEmpty(CompilerVersion)? "" : String.Format(" ({0})", CompilerVersion));
			}

			// Get the toolchain info
			WindowsCompiler ToolChain;
			VersionNumber SelectedToolChainVersion;
			DirectoryReference SelectedToolChainDir;
			if(Compiler == WindowsCompiler.Clang || Compiler == WindowsCompiler.Intel)
			{
				ToolChain = WindowsCompiler.VisualStudio2017;
				if(!WindowsPlatform.TryGetToolChainDir(ToolChain, null, out SelectedToolChainVersion, out SelectedToolChainDir))
				{
					throw new BuildException("{0}{1} must be installed in order to build this target.", WindowsPlatform.GetCompilerName(Compiler), String.IsNullOrEmpty(CompilerVersion)? "" : String.Format(" ({0})", CompilerVersion));
				}
			}
			else
			{
				ToolChain = Compiler;
				SelectedToolChainVersion = SelectedCompilerVersion;
				SelectedToolChainDir = SelectedCompilerDir;
			}

			// Get the actual Windows SDK directory
			VersionNumber SelectedWindowsSdkVersion;
			DirectoryReference SelectedWindowsSdkDir;
			if(!WindowsPlatform.TryGetWindowsSdkDir(WindowsSdkVersion, out SelectedWindowsSdkVersion, out SelectedWindowsSdkDir))
			{
				throw new BuildException("Windows SDK{0} must be installed in order to build this target.", String.IsNullOrEmpty(WindowsSdkVersion) ? "" : String.Format(" ({0})", WindowsSdkVersion));
			}

			return new VCEnvironment(Platform, Compiler, SelectedCompilerDir, SelectedCompilerVersion, ToolChain, SelectedToolChainDir, SelectedToolChainVersion, SelectedWindowsSdkDir, SelectedWindowsSdkVersion);
		}
	}
}
