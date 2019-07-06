/* ------------------------------------------
 * Copyright (c) 2018, Synopsys, Inc. All rights reserved.

 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:

 * 1) Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.

 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.

 * 3) Neither the name of the Synopsys, Inc., nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
--------------------------------------------- */
#ifndef _DFSS_I2S_OBJ_H_
#define _DFSS_I2S_OBJ_H_

#include "ip/ip_hal/inc/dev_i2s.h"

#define DFSS_I2S_0_ID		0	/*!< I2S 0 ID macro */
#define DFSS_I2S_1_ID		1	/*!< I2S 1 ID macro */
#define DFSS_NO_I2S			(-1)/* in case no ID is in use, dfss_i2s_get_dev() should handle this value*/

#ifdef IO_I2S_TX_MST0_PRESENT
#define USE_DFSS_I2S_0		1	/*!< enable use DFSS I2S 0 as TX */
#else
#define USE_DFSS_I2S_0		0	/*!< disable use DFSS I2S 0 as TX */
#endif

#ifdef IO_I2S_RX_MST0_PRESENT
#define USE_DFSS_I2S_1		1	/*!< enable use DFSS I2S 1 as RX */
#else
#define USE_DFSS_I2S_1		0	/*!< disable use DFSS I2S 1 as RX */
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern DEV_I2S_PTR dfss_i2s_get_dev(int32_t i2s_id);

#ifdef __cplusplus
}
#endif

#endif /* _DFSS_I2S_OBJ_H_ */