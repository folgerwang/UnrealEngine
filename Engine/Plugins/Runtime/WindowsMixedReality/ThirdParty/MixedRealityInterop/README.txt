This solution builds a lib with Windows Mixed Reality cppwinrt types available in Windows 10 SDK's 17134+.

Build this solution for Release x64 and x86 and copy MixedRealityInterop.lib to the matching architecture destination under 
Engine\Plugins\Runtime\WindowsMixedReality\Source\ThirdParty\Lib

If you update MixedRealityInterop.h, copy to 
Engine\Plugins\Runtime\WindowsMixedReality\Source\ThirdParty\Include

Run copy_lib.cmd to automatically copy libs and header to the correct location.