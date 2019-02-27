/* (c) Copyright 2016-2018 Mercer Road Corp. All rights reserved.
 * Mercer Road Corp. hereby grants you ("User"), under its copyright rights, the right to distribute,
 * reproduce and/or prepare derivative works of this software source (or binary) code ("Software")
 * solely to the extent expressly authorized in writing by Mercer Road Corp.  If you do not have a written
 * license from Mercer Road Corp., you have no right to use the Software except for internal testing and review.
 *
 * No other rights are granted and no other use is authorized. The availability of the Software does not provide
 * any license by implication, estoppel, or otherwise under any patent rights or other rights owned or
 * controlled by Mercer Road or others covering any use of the Software herein.
 *
 * USER AGREES THAT MERCER ROAD ASSUMES NO LIABILITY FOR ANY DAMAGES, WHETHER DIRECT OR OTHERWISE,
 * WHICH THE USER MAY SUFFER DUE TO USER'S USE OF THE SOFTWARE.  MERCER ROAD PROVIDES THE SOFTWARE "AS IS,"
 * MAKES NO EXPRESS OR IMPLIED REPRESENTATIONS OR WARRANTIES OF ANY TYPE, AND EXPRESSLY DISCLAIMS THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT.
 * USER ACKNOWLEDGES THAT IT ASSUMES TOTAL RESPONSIBILITY AND RISK FOR USER'S USE OF THE SOFTWARE.
 *
 * Except for any written authorization, license or agreement from or with Mercer Road Corp.
 * the foregoing terms and conditions shall constitute the entire agreement between User and Mercer Road Corp.
 * with respect to the subject matter hereof and shall not be modified or superceded
 * without the express written authorization of Mercer Road Corp.
 *
 * Any copies or derivative works must include this and all other proprietary notices contained on or in the Software as received by the User.
 */

#pragma once

#ifdef __APPLE__
#include <TargetConditionals.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#define _strcmp(a,b) strcmp(a,b)p
#define _strcmpi(a,b) strcasecmp(a,b)
#define strcpy_s(a,b) strcpy(a,b)
#define sprintf_s(a, b, ...) sprintf(a, __VA_ARGS__)
#define Sleep(a) usleep(a*1000)
#endif
extern void debugPrint(const char *);
