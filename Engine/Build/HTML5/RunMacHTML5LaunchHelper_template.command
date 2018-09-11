#!/bin/sh

# Find the Unreal Mono install if it exists, we'll use that if possible
# Otherwise we fallback to mono and hope it exists
EPIC_MONO=""
if [ -f "${unreal_mono_pkg_path}" ]; then
    pushd "${unreal_mono_pkg_path_base}"
	source SetupMono.sh
    popd
else
	EPIC_MONO="NOTE: It seems this $0 was built from another platform and copied here.\nNOTE: Was expecting to setup mono via: "'${unreal_mono_pkg_path}';
fi

# Give some warning if mono isn't valid.
command -v mono > /dev/null 2>&1 || { echo "Mono is required to run this script. If it's already installed please make sure it exists on the path"; echo $EPIC_MONO; exit 1; }

# Set the current directory to where the script is being run from
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
pushd "$DIR"
mono HTML5LaunchHelper.exe
popd
