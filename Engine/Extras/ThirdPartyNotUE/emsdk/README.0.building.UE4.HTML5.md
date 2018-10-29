---
layout: default
title: "Building UE4 HTML5"
---

* * *
# Building UE4 HTML5

UE4 calls this "Packaging for HTML5"

* * *
## Get Source Files

Steps I normally do when grabbing latest from Perforce for UE4.
(**TODO: _github instructions_**)

- get latest from **//UE4/Dev-Mobile**
- goto the folder where Perforce has pulled down your local storage
- **delete** your xcode or visual studio solution or Makefile (if it already exists)
	to ensure they are rebuilt (instead of assuming the following step completed successfully)
- regenerate the project files, run:
	- **GenerateProjectFiles.bat** -- on Windows
	- **GenerateProjectFiles.command** -- on OSX
	- **GenerateProjectFiles.sh** -- on Linux
- **open** the regenerated {xcode,solution}

* * *
## Compiling Support Programs

I like to "manually rebuild" the following in this order:

- in visual studio:
	- ensure **Solution Platform** is set to **Win64**
	- and **Solution Configuration** is set to **Development Editor**

- in **Solution Explorer-> Solution -> Programs**, build the following (again, in this order):
	- AutomationTool
	- AutomationToolLauncher
	- UnrealBuildTool
	- UnrealHeaderTool
	- HTML5LaunchHelper
	- ShaderCompileWorker
	- UnrealPak
	- UnrealLightmass
	- UnrealFileServer

Linux instructions are detailed in **.../Engine/Build/BatchFiles/Linux/READEME.md**

OSX instructions: **TODO**

* * *
## Compiling UE4 Editor

finally, build the Editor:
- **Solution Explorer-> Solution -> Engine -> UE4**
	- this is going to take while to complete...

* * *
## Run UE4 Editor

when the build completes, fire up the editor:
- .../Engine/Binaries/Win64/UE4Editor.exe
- .../Engine/Binaries/Mac/UE4Editor
- .../Engine/Binaries/Linux/UE4Editor

if you see the **shaders building** -- let it run to completion -- this is usually a one time build.

* * *
### Ensure Build Environment Is Working

in the **Unreal Project Browser** - create a new project:
- click on **New Project** tab
- then click on **Blueprint** subtab
- select (for example) **ThirdPerson**
- you can keep the **Desktop/Console, Maximum Quality** and **No Starter Content** options
- finally, select your **path** and make up a "**ProjectName**"
- click **Create Project** button


when the editor restarts, I like to have the following windows opened:

window #1
- Menu bar -> Window -> Developer Tools -> **Output Log**
	- you can tear the tab out of the box to (for example) another screen
	- this output log is where all prints are dumped
		- this is how I "debug" the HTML5 builds (we will go over this in [Emscripten And UE4](README.2.emscripten.and.UE4.md))

window #2
- Menu bar -> Edit -> Project Settings ...
	- click around here for EVERYTHING (including the kitchen sink)

	- here's a list I always click on **and** look at before packaging:
		- Project -> Maps & Modes
			- **Default Maps** -> Game Default Map
		- Project -> Packaging
			- Packaging -> **Use Pak File**
				- I normally **UN**check this to help the asset builds (called "cooking")
				go a little faster (by not having to build this additional step)
		- Engine -> Rendering
			- Mobile
		- Platforms -> HTML5
			- Emscripten
				- for now - **UN**CHECK Multithreading support

* * *
## Package The Project For HTML5

set the build type:
- Menu bar -> File -> **Package Project** -> Build Configuration
	- I normally select **Development** when ever I'm BugHunting

finally, package for HTML5:
- Menu bar -> File -> Package Project -> **HTML5**
	- select the folder where the final files will be **archived** to
		- this is going to take while to complete...  especially if it's the first time building shaders too.
		- goto lunch

* * *
## Test The HTML5 Packaged Project

open a command prompt to where the files were **archive** at:
- run **HTML5LaunchHelper.exe** (**RunMacHTML5LaunchHelper.command**, or your favorite web server)
- open browser to <http://localhost:8000/> (or to your favorite web server port & path)
	- click on the relevant HTML file

for more details on the generated HTML5 files, please see:
- **.../Engine/Build/HTML5/README.txt**
	- this folder contains the template files used to generate the html, js, and css files
	- you can change template files here to make them stick system-wide on every re-packaing
		- however, we recommend developers (i.e. game makers) putting their custom template file changes in the project's own folder
		(`.../<Project>/Build/HTML5/`)
- before the final copy to the **archive** folder (you've selected above), they are staged at:
	- **.../Engine/Binaries/HTML5/**
	- do not edit anything here, they will be stompped on after every packaging
	- i.e. treat this folder as READ ONLY

* * *

Next, [Debugging UE4 HTML5](README.1.debugging.UE4.HTML5.md)

* * *
