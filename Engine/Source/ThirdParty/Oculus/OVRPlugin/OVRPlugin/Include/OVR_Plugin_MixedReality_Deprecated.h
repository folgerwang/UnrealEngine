/************************************************************************************

Copyright (c) Facebook Technologies, LLC and its affiliates.  All rights reserved.

Licensed under the Oculus SDK License Version 3.5 (the "License");
you may not use the Oculus SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

https://developer.oculus.com/licenses/sdk-3.5/

Unless required by applicable law or agreed to in writing, the Oculus SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_Plugin_MixedReality_Deprecated_h
#define OVR_Plugin_MixedReality_Deprecated_h

#include "OVR_Plugin_MixedReality.h"

#ifdef __cplusplus
extern "C" {
#endif

OVRP_EXPORT ovrpBool ovrp_IsCameraDeviceAvailable(ovrpCameraDevice camera);
OVRP_EXPORT ovrpBool ovrp_HasCameraDeviceOpened(ovrpCameraDevice camera);
OVRP_EXPORT ovrpBool ovrp_IsCameraDeviceColorFrameAvailable(ovrpCameraDevice camera);

#ifdef __cplusplus
}
#endif

#endif
