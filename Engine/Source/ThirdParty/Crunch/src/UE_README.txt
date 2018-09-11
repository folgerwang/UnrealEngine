Current version: 
	https://github.com/Unity-Technologies/crunch/tree/unity @ c1d8e8d
UE changes:
	- renames solution/project files to indicate UE specific
	- updated solution/project to VS2015
	- Runtime library set to 'Multi-threaded DLL (/MD)'
	- Added support for evenly sized mips (controlled by cCrnCompFlagUniformMips) and couple of utility functions. See 'UE4_BEGIN/UE4_END' comments inside code
	- disabled LTCG for crnlib so UE doesn't get forced into LTCG link
	- fixed 'potentially uninitialized use' of refinerResults in crn_dxt_hc.cpp