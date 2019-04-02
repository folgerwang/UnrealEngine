#pragma once
/* Copyright (c) 2014-2018 by Mercer Road Corp
*
* Permission to use, copy, modify or distribute this software in binary or source form
* for any purpose is allowed only under explicit prior consent in writing from Mercer Road Corp
*
* THE SOFTWARE IS PROVIDED "AS IS" AND MERCER ROAD CORP DISCLAIMS
* ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL MERCER ROAD CORP
* BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
* DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
* PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
* ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
* SOFTWARE.
*/
#include "VxcResponses.h"
#include "VxcEvents.h"

#include "vivoxclientapi/clientconnection.h"

/// \mainpage 
///
/// Over the past years, game programmers have accessed Vivox services from their game client using the Vivox Client SDK.
///
/// The Vivox Client SDK implements a "C" message passing interface. The developer is responsible for creating requests and issuing those requests to the SDK using the vx_issue_request() method, and then retrieving event and response messages from the SDK using the vx_get_message() method. The developer is also required to maintain state based on those responses and events.
///
/// While powerful and portable, this type of interface requires a considerable amount of both code and understanding of some rather subtle interface semantics on behalf of the developer. In short, we've made the job of integrating Vivox technology into games harder than it needs to be.
///
/// To address this, we are shipping the first version of a source code distribution that presents a simpler, more well documented interface to the game programmer. This interface is part of the Vivox Simplified Sample Applications package - and it is exposed as a handful of classes that are used by the game application and a single callback interface that the game programmer implements. This collection of classes is called the VivoxClientAPI library, and its source is included in the Vivox Simplified Sample Applications package.
///
/// With this interface, rather than sending and receiving messages, the game programmer will call methods, and handle callbacks. The library will handle all the state management for the game programmer, and significantly reduce the amount of code required to get into a channel.
///
/// In this release there are some significant functional gaps in the interface relative to the Client SDK "C" interface . These include positional channel support and person to person calling (as well as some less frequently used features).
///
/// We are working quickly to achieve significant feature parity relative to the Client SDK - if there are things missing in the VivoxClientAPI library that you'd like especially, please let us know. We are positioned to move quickly to address any shortcomings that developers uncover.
///
/// Your feedback is valued - please feel free to contact us at developer@vivox.com with any thoughts or suggestions for making this a better product for game developers.
///
/// Regards,
///
/// Mike Skrzypczak
///
/// VP Engineering, Vivox/Mercer Rd Corp
///
/// mikes@vivox.com
///
/// http://www.mercerrd.com, http://www.vivox.com, http://www.downloadc3.com
///
///
