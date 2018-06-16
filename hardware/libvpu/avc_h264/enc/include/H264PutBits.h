/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : Bit stream handling
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: H264PutBits.h,v $
--  $Date: 2007/07/16 09:28:42 $
--  $Revision: 1.1 $
--
------------------------------------------------------------------------------*/

#ifndef __H264_PUT_BITS_H__
#define __H264_PUT_BITS_H__

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "basetype.h"
#include "enccommon.h"
/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
 
#define H264NalBits(stream, val, num) H264PutNalBits(stream, val, num)

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/
bool_e H264SetBuffer(stream_s * buffer, u8 * stream, i32 size);
void H264PutBits(stream_s *, i32, i32);
void H264PutNalBits(stream_s *, i32, i32);
void H264ExpGolombUnsigned(stream_s * stream, u32 val);
void H264ExpGolombSigned(stream_s * stream, i32 val);
void H264RbspTrailingBits(stream_s * stream);
void H264Comment(char *comment);

#endif
