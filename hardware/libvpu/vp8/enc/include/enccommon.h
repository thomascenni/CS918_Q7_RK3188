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
--  Description : Encoder common definitions for control code and system model
--
------------------------------------------------------------------------------*/

#ifndef __ENC_COMMON_H__
#define __ENC_COMMON_H__

/*------------------------------------------------------------------------------
    1. External compiler flags
------------------------------------------------------------------------------*/

/* Encoder global definitions
 *
 * _ASSERT_USED     # Asserts enabled
 * _DEBUG_PRINT     # Prints debug information on stdout
 * TRACE_STREAM     # Creates stream trace file
 * TEST_DATA        # Creates test data files
 *
 * Can be defined here or using compiler flags */

/*------------------------------------------------------------------------------
    2. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "ewl.h"

/* Test data generation requires stream trace */
#ifdef TEST_DATA
#ifndef TRACE_STREAM
#define TRACE_STREAM
#endif
#endif

/* Stream tracing requires encdebug.h */
#ifdef TRACE_STREAM
#ifndef H1_HAVE_ENCDEBUG_H
#define H1_HAVE_ENCDEBUG_H
#endif
#endif

#ifdef H1_HAVE_ENCDEBUG_H
#include "encdebug.h"
#else
#define ASSERT(expr)
#define DEBUG_PRINT(args)
#define COMMENT(x)
#define COMMENTMBTYPE(x,y)
#define TRACE_BIT_STREAM(v,n)
#endif

#ifdef H1_HAVE_ENCTRACE_H
#include "enctrace.h"
#endif

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

typedef enum
{
    ENCHW_NOK = -1,
    ENCHW_OK = 0
} bool_e;

typedef enum
{
    ENCHW_NO = 0,
    ENCHW_YES = 1
} true_e;

typedef enum
{
    NONIDR = 1, /* Coded slice of a non-IDR picture */
    IDR = 5,    /* Coded slice of an IDR picture */
    SEI = 6,    /* SEI message */
    SPSET = 7,  /* Sequence parameter set */
    PPSET = 8,  /* Picture parameter set */
    ENDOFSEQUENCE = 10, /* End of sequence */
    ENDOFSTREAM = 11,   /* End of stream */
    FILLERDATA = 12, /* Filler data */
    PREFIX = 14,    /* Prefix */
    SSPSET = 15,    /* Subset sequence parameter set */
    MVC = 20        /* Coded slice of a view picture */
} nalUnitType_e;

/* VLC TABLE */
typedef struct
{
    i32 value;  /* Value of bits  */
    i32 number; /* Number of bits */
} table_s;

/* used in stream buffer handling */
typedef struct
{
    u8 *stream; /* Pointer to next byte of stream */
    u32 size;   /* Byte size of stream buffer */
    u32 byteCnt;    /* Byte counter */
    u32 bitCnt; /* Bit counter */
    u32 byteBuffer; /* Byte buffer */
    u32 bufferedBits;   /* Amount of bits in byte buffer, [0-7] */
    u32 zeroBytes;  /* Amount of consecutive zero bytes */
    i32 overflow;    /* This will signal a buffer overflow */
    u32 emulCnt; /* Counter for emulation_3_byte, needed in SEI */
    i32 *table;         /* Video packet or Gob sizes */
    i32 tableSize;      /* Size of above table */
    i32 tableCnt;       /* Table counter of above table */
} stream_s;

/* General tools */
#define ABS(x)          ((x) < (0) ? -(x) : (x))
#define MAX(a, b)       ((a) > (b) ?  (a) : (b))
#define MIN(a, b)       ((a) < (b) ?  (a) : (b))
#define SIGN(a)         ((a) < (0) ? (-1) : (1))
#define OUT_OF_RANGE(x,a,b) ((((i32)x)<(a)?(1):(0)) || ((x)>(b)?(1):(0)))
#define CLIP3(v, min, max)  ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))

#endif
