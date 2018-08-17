The libs here come from the Windows 10 SDK, at least version 17134:
d3d11.lib
OneCore.Lib

This lib comes from a separate VS solution to get around not being able to build cppwinrt inside of Unreal.
This lib must be build with the same Windows 10 SDK you got the above libs from.
MixedRealityInterop.lib