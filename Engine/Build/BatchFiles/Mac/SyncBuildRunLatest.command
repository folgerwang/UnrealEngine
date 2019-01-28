#!/bin/sh

#script to automatically sync and run most recent change in this stream in specified project
#	parameter 1: .uproject to sync/build/run
#	parameter 2: (optional) manually specified changelist 
#	example usage 1 (relative path from root stream directory): ./SyncBuildRunLatest.command QAGame/QAGame.uproject
#	example usage 2 (absolute disk path) : ./SyncBuildRunLatest.command \Users\your.user\UE4\Dev-Main\QAGame\QAGame.uproject
#
# Prerequisites:
# 1)	p4 executable (see https://www.perforce.com/perforce/r14.2/manuals/p4guide/chapter.install.html)
# 2)	p4 configuration (https://www.perforce.com/perforce/r15.1/manuals/p4guide/chapter.configuration.html)
# 2a)	Best way to set p4 configuration is with P4CONFIG in .bash_profile, because this lets you have separate settings for each workspace
#			Create a Bash Profile:
#			Open Terminal
#			In Terminal, enter the following:
#				cd ~/
#			In Terminal, enter the following:
#				touch .bash_profile
#			In Terminal, enter the following:
#				open -e .bash_profile
#			add line:
#				export P4CONFIG=.p4config
#			Create a file .p4config at the root of the workspace (make sure no other extension is added to the end) , add the following lines:
#				# Perforce Settings
#				P4PORT=YourLocalProxy
# 				P4CLIENT=YourWorkSpace
#				P4USER=YourUsername
# 			Save file
# 3) manually sync this workspace once (Engine, Project, GenerateProjectFiles and UE4Games.uprojectdirs)
# 4) SyncBuildRun script requires that your engine stream is inside a UE4 directory (like D:/UE4/Dev-Main/)

: ${1?"No project specified, exiting."}



project=$1
changelist=$2

if pgrep -x "UE4Editor" > /dev/null
then
    echo "UE4Editor already running, please close all UE4Editor processes..."
    killall -9 "UE4Editor"
    exit 1
else
    echo "No UE4Editor processes running."
fi

if [ ! -f ../../../Build/BatchFiles/Mac/SyncBuildRunLatest.command ]; then
	echo "RunUAT ERROR: The script does not appear to be located in the "
  echo "Engine/Build/BatchFiles/Mac directory.  This script must be run from within that directory."
	exit 1
fi


#echo $(p4 where ../../..)

#convert local current directory path to perforce server path
ue4_root_directory_server=$(p4 where ../../..)
arr=($ue4_root_directory_server)
ue4_root_directory_server=${arr[0]}
ue4_root_directory_local=${arr[2]}
suffix="Engine"

ue4_root_directory_server=${ue4_root_directory_server%"$suffix"}
ue4_root_directory_local=${ue4_root_directory_local%"$suffix"}


echo $ue4_root_directory_server


#have to force sync these two files after running SyncProject previously
p4 sync -f "$ue4_root_directory_server"Engine/Build/Build.version
p4 sync -f "$ue4_root_directory_server"Engine/Source/Programs/DotNETCommon/MetaData.cs

path_to_sync=$ue4_root_directory_server...
echo Synching $path_to_sync

if [ -z "$changelist" ]; then
    echo "changelist unset, getting latest"
	p4 changes -m 1 -s submitted "//UE4/Dev-VR/..."
	
	latest_change_string=$(p4 changes -m 1 -s submitted "$path_to_sync")
	echo $latest_change_string
	arr=($latest_change_string)
	echo ${arr[1]}
	
	changelist=${arr[1]}
else
	echo "Manually specifying changelist";
fi

echo changelist="$changelist"

if pgrep -x "UE4Editor" > /dev/null
then
    echo "UE4Editor already running, please close all UE4Editor processes..."
    exit 1
    #killall -9 "UE4Editor"
else
    echo "No UE4Editor processes running."
fi

echo $ue4_root_directory_local

../RunUAT.command SyncProject -CL=$changelist -Project="$project" -build -run

open -a "$ue4_root_directory_local"Engine/Binaries/Mac/UE4Editor.app --args "$project"

exit