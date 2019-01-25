---
layout: default
title: "Emscripten and UE4"
---

* * *
# Emscripten and UE4

these are the steps I use whenever emscripten is upgraded.  I will also show the C#
files to edit (to add/modify the emscripten command -- used during HTML5 packaging).


* * *
## Part 0 - EMSDK (precompiled)

I aways pull a fresh copy of emsdk into its own folder (i.e. I never `git pull` on my local emsdk weeks/months down the line).
I do this to ensure there are no extra files from outdated versions are getting checked in to the (new toolchain) UE4 repository.

```sh
git clone https://github.com/juj/emsdk emsdk.<version#>
cd emsdk.<version#>
./emsdk list
./emsdk install sdk-<version#>-64bit
./emsdk activate sdk-<version#>-64bit

# source in the new dev environment
. ./emsdk_env.sh
```

note: when building emscripten from source
(i.e. not the precompiled SDK - e.g. `incoming` branch) - yes - i would `git pull` -- but,
this is usually (again) in a seperate WIP folder...

* * *
## Part 1 - Emscripten toolchain and Thirdparty libraries

- **Engine/Extras/ThirdPartyNotUE/emsdk/emscripten/...**
	- where UE4 keeps the emscripten toolchain
	- we split the platform and emscripten itself to keep things separated
		- .../emsdk/emscripten/&lt;version#&gt;
		- .../emsdk/Linux/{clang,node}
		- .../emsdk/Mac/{clang,node}
		- .../emsdk/Win64/{clang,node,python}
	- these are usually the files pulled down during `./emsdk install &lt;version#&gt;` (and the respective platforms)
		- i.e. no source files
		- yes, do this for all 3 major desktop platforms
	- before checking in (see part 3 below):
		- we would also delete any *test* and *third_party* folders from the emscripten installs
			- NOTE: remember to delete any `*.pyc` files before checking in
				- `find . -name "*.pyc" -exec rm {} \;`
			- **WARNING**: check in the **.../emscripten/&lt;version#&gt;/...** files from OSX (or Linux)
				- this keeps the "executable" bit properly (Perforce on Windows doesn't care to check that "bit" in properly)


- **Engine/Source/ThirdParty/HTML5/Build_All_HTML5_libs.rc**
	- this **_used_** to setup my shell environment (where currently, I now use `source .../emsdk/emsdk_set_env.sh`)
	when I need to _rebuild the thirdparty libraries_ for HTML5 (**from the OSX or Linux commandline**)
	- **_now_**, this file has notes reminding me of the changes needed to the emscripten toolchain for UE4 purposes
		- please see the file if you're curious (especially the "**upgrading emsdk - REMEMBER TO DO THE FOLLOWING**" section)


- **Engine/Source/ThirdParty/HTML5/Build_All_HTML5_libs.sh**
	- after pulling down a new version of emscripten, we **have to rebuild** the (UE4's) thirdparty libs used with HTML5
		- i.e. **new toolchain --> new binaries**
	- this shell script spells out which libs need to be rebuilt for HTML5
	- scan the additional scripts called from this file for more details
		- every now and then, the Makefiles, CMake files, or something needs to change
		(new compiler warnings, depricated features, compiler/toolchain support level, etc.)
		- I like to build the thirdparty libs **one at a time** and even **one optimization level** at a time
		- YMMV, play with the (sh) scripts -- it's spelled out like a normal cookbook set of CLI instructions
	- **this(these) script(s) DOES NOT NEED to know about UE4's build environment**
	- that said, **it is recommended to build the thirdparty libs from OSX or Linux** (see the companion
	file **Build_All_HTML5_libs.rc** -- just mentioned above -- under the **WINDOWS NOTES** section for details on why)


- **Engine/Intermediate/Build/HTML5/EmscriptenCache**
	- every time you upgrade/change your toolchain version (or switch between them or make a local modification
	to **.../emsdk/emscripten/<version>/system/...** you will need to delete the respective *.bc file or the
	whole folder to ensure your changes are recompiled
	- NOTE: after doing this (changing toolchain versions) -- be sure to update your **HTML5SDKInfo.cs**
	version numbers (see next section for details)


- **emscripten ports**
	- while a lot of the UE4 Thirdparty libs (used with HTML5 builds) are also available as escripten ports,
	there are a number of custom UE4 changes that are required to make the engine functional for HTML5
	- for now, we recommend using the UE4 versions until this is revisited in the future
	- NOTE: when using ports, remember to "turn off" UE4's version
		- e.g. if you wish to use emscripten's version of SDL2 -- you will need to disable UE4's version:
			- edit: **.../Engine/Thirdparty/SDL2/SDL2.Build.cs**
			- delete/comment out the HTML5 section and save the file
			- this change will be automatically picked up at packaging time


* * *
## Part 2 - UE4 C# scripts

the following files **ARE** used (for example) by UE4 Editor to package for HTML5:
- **Engine/Source/Programs/UnrealBuildTool/Platform/HTML5/HTML5SDKInfo.cs**
	- the only thing of interest here are the string variables listed at the top of the HTML5SDKInfo class
		- `SDKVersion`
		- `LLVM_VER`
		- `NODE_VER`
		- `PYTHON_VER`

- **Engine/Source/Programs/UnrealBuildTool/Platform/HTML5/HTML5ToolChain.cs**
	- this is where everything is put (1) to setting up and (2) to run emscripten from UE4
	- THE functions that builds the emscripten command line options:
		- `GetSharedArguments_Global()` - used in both compilation and linking
		- `GetCLArguments_CPP()` - used in compilation only
		- `GetLinkArguments()` - used in linking only

- **Engine/Source/Programs/AutomationTool/HTML5/HTML5Platform.Automation.cs**
	- the top half of this C# file is where the files built are selected and archived
	(see [Building UE4 HTML5](README.0.building.UE4.HTML5.md) notes in section **Package The Project For HTML5**)
	- the rest of the file are used for UE4's Editor options that the HTML5 packaging supports


NOTE: it seems that latest Editor will rebuild the C# (AutomationTool) project automatically when packaging (for example) HTML5.
- **but**, every-now-and-then -- **this sometimes needs manual intervention:**
	- in the **Solution Explorer -> Solution -> Programs**, build (the respective "program"):
		- AutomationTool
		- UnrealBuildTool (!!! this still needs to be manually built on windows !!!)


* * *
## Part 3 - Test Build, Checking In, and CIS

be sure to package a "**SANITY CHECK"** build -- i.e. running the steps in: [Building UE4 HTML5](README.0.building.UE4.HTML5.md)
- no engine changes, just the build toolchain (emscripten) changes (part 2 above)

once you're satisfied with the toolchain - check everything in pulled/built in part 1 above;
- thirdparty libs
- emscripten toolchain
	- Windows binaries
	- OSX binaries
	- Linux binaries
	- emscripten

next, I like to check in the UE4 C# files changes (part 2) after "part 1" has been checked in.

this way, the stream/branch all are still functional (using the existing "older" toolchain) and
doesn't block anyone who may just happen to be syncing/pulling in the middle of all of this
(also, I tend to use a number of "changelists" especially from multiple platforms -- this is
more of a Perforce issue than git).

anyways, when I see Epic's CIS (continuous integration system) all green for HTML5 -- will
I "then" nuke the older toolchain from the stream/branch.  otherwise I would quickly revert
the files from part 2 to get CIS green again (and start the bug hunts).

* * *
