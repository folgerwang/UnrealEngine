#pragma once
#if !defined(__ORBIS__) && !defined(_XBOX) && !defined(_XBOX_ONE) && !TARGET_OS_IPHONE && !defined(__android__) && !defined(_UAP)
/* Copyright (c) 2013-2018 by Mercer Road Corp
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

#include "VxcTypes.h"
/**
 * This function sets two test callbacks into vx_sdk_config_t structure.
 */
extern void vx_test_set_udp_frame_callbacks( vx_sdk_config_t* config );

#include <stdlib.h>

// --------------------- UDP CALLBACKS TEST ------------------------------------------------
/**
 * This callback will be called before rtp/rtcp/sip packet transmitted.
 * VX_TEST_UDP_HEADER environment variable defines the length in bytes of the header that will be added to the packet.
 * VX_TEST_UDP_TRAILER environment variable defines the length in bytes of the trailer that will be added to the packet.
 * The server should cut out this data.
 * Server must have the same header/trailer length settings as the client.
 */
static void test_on_before_udp_frame_transmitted(
  void*  /* callback_handle */,  // the handle passed in the vx_sdk_config_t structure 
  vx_udp_frame_type    /* frame_type */,
  void*  /* payload_data */,     // the data to be transmitted to the network 
  int    /* payload_data_len */, // the len of that data 
  void** header_out,       // callback set - pointer to header data (NULL if no header) 
  int*   header_len_out,   // callback set - length of the header data (0 if no header) 
  void** trailer_out,      // callback set - pointer to trailer data (NULL if no trailer) 
  int*   trailer_len_out   // callback set - length of the trailer data (0 if no trailer) 
) {
	static int hdrSize = -1;
	static int trlSize = 0;

  /**
   * Check the VX_TEST_UDP_HEADER and VX_TEST_UDP_TRAILER variabels only one time.
   */
  if (hdrSize < 0) {

    hdrSize = 0;

    char *pVX_TEST_UDP_HEADER = getenv("VX_TEST_UDP_HEADER");
    if (pVX_TEST_UDP_HEADER)
      hdrSize = atoi(pVX_TEST_UDP_HEADER);

    char *pVX_TEST_UDP_TRAILER = getenv("VX_TEST_UDP_TRAILER");
    if (pVX_TEST_UDP_TRAILER)
      trlSize = atoi(pVX_TEST_UDP_TRAILER);

  }

  if (0 == hdrSize && 0 == trlSize)
    return;

  char* hdr = new char[hdrSize];
  char* trl = new char[trlSize];
  char* tmp = NULL;

  /**
   * Fill the header with increasing integer values, trailer with decreasing values.
   * So, you can use Wireshark to see how packets are looks like with the header and trailer.
   */
  tmp = hdr; for (int i = 0; i < hdrSize; i++, tmp++) { *tmp = i % 256; }
  tmp = trl; for (int i = 0; i < trlSize; i++, tmp++) { *tmp = 255 - (i % 256); }

  *header_out = hdr;
  *trailer_out = trl;

  *header_len_out = hdrSize;
  *trailer_len_out = trlSize;
}

/**
 * This callback will be called after rtp/rtcp/sip packet transmitted.
 * Just clean up an allocated memory here.
 */
static void test_on_after_udp_frame_transmitted(
  void* /* callback_handle */,  // the handle passed in the vx_sdk_config_t structure 
  vx_udp_frame_type   /* frame_type */,
  void* /* payload_data */,     // the data to be transmitted to the network 
  int   /* payload_data_len */, // the len of that data 
  void* header,           // the header data passed in pf_on_before_udp_frame_transmitted 
  int   /* header_len */,       // length of the header data 
  void* trailer,          // the trailer data passed in pf_on_before_udp_frame_transmitted 
  int   /* trailer_len */,      // length of the trailer data 
  int   /* sent_bytes */        // the total number of bytes transmitted - < 0 indicates error 
) {
  if (NULL != header) delete [] (char*)header;
  if (NULL != trailer) delete [] (char*)trailer;
}

/**
 * This function sets up the vx_sdk_config_t structure with test callbacks.
 */
void vx_test_set_udp_frame_callbacks(vx_sdk_config_t* config) {
  if (NULL == config)
    return;

  config->pf_on_before_udp_frame_transmitted = test_on_before_udp_frame_transmitted;
  config->pf_on_after_udp_frame_transmitted = test_on_after_udp_frame_transmitted;
}
//-----------------------------------------------------------------------------------------        
#define VX_HAS_UDP_CALLBACKS
#endif
