#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cutils/sockets.h>
#define LOG_TAG "VP8_DEC"
#include <utils/Log.h>
#ifdef AVS40
#undef ALOGV
#define ALOGV LOGV

#undef ALOGD
#define ALOGD LOGD

#undef ALOGI
#define ALOGI LOGI

#undef ALOGW
#define ALOGW LOGW

#undef ALOGE
#define ALOGE LOGE

#endif
//#undef printf
//#define printf ALOGD

#include "vp8decoder.h"
#include "vpu.h"
#define	swap32bit(x) ((x>>24)|((x>>8)&0xff00)|((x<<8)&0xff0000)|(x<<24))


static const RK_U8 MvUpdateProbs[2][VP8_MV_PROBS_PER_COMPONENT] = {
    { 237, 246, 253, 253, 254, 254, 254, 254, 254,
      254, 254, 254, 254, 254, 250, 250, 252, 254, 254
    },
    { 231, 243, 245, 253, 254, 254, 254, 254, 254,
      254, 254, 254, 254, 254, 251, 251, 254, 254, 254
    }
};

static const RK_U8 Vp8DefaultMvProbs[2][VP8_MV_PROBS_PER_COMPONENT] = {
    { 162, 128, 225, 146, 172, 147, 214, 39, 156,
      128, 129, 132, 75, 145, 178, 206, 239, 254, 254
    },
    { 164, 128, 204, 170, 119, 235, 140, 230, 228,
      128, 130, 130, 74, 148, 180, 203, 236, 254, 254
    }
};

static const RK_U8 Vp7DefaultMvProbs[2][VP7_MV_PROBS_PER_COMPONENT] = {
    { 162, 128, 225, 146, 172, 147, 214, 39, 156,
      247, 210, 135,  68, 138, 220, 239, 246
	},
	{ 164, 128, 204, 170, 119, 235, 140, 230, 228,
      244, 184, 201,  44, 173, 221, 239, 253
    }
};

static const RK_U8 CoeffUpdateProbs[4][8][3][11] =
{
  {
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
      {249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
      {234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255, },
      {250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255, },
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
  },
  {
    {
      {217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255, },
      {234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255, },
    },
    {
      {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
  },
  {
    {
      {186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255, },
      {234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255, },
      {251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255, },
    },
    {
      {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
  },
  {
    {
      {248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255, },
      {248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
      {248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
  },
};


static const RK_U8 DefaultCoeffProbs [4][8][3][11] =
{
{
{
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
},
{
{ 253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128},
{ 189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128},
{ 106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128}
},
{
{ 1, 98, 248, 255, 236, 226, 255, 255, 128, 128, 128},
{ 181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128},
{ 78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128}
},
{
{ 1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128},
{ 184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128},
{ 77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128}
},
{
{ 1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128},
{ 170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128},
{ 37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128}
},
{
{ 1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128},
{ 207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128},
{ 102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128}
},
{
{ 1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128},
{ 177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128},
{ 80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128}
},
{
{ 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 246, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
}
},
{
{
{ 198, 35, 237, 223, 193, 187, 162, 160, 145, 155, 62},
{ 131, 45, 198, 221, 172, 176, 220, 157, 252, 221, 1},
{ 68, 47, 146, 208, 149, 167, 221, 162, 255, 223, 128}
},
{
{ 1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128},
{ 184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128},
{ 81, 99, 181, 242, 176, 190, 249, 202, 255, 255, 128}
},
{
{ 1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128},
{ 99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128},
{ 23, 91, 163, 242, 170, 187, 247, 210, 255, 255, 128}
},
{
{ 1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128},
{ 109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128},
{ 44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128}
},
{
{ 1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128},
{ 94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128},
{ 22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128}
},
{
{ 1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128},
{ 124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128},
{ 35, 77, 181, 251, 193, 211, 255, 205, 128, 128, 128}
},
{
{ 1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128},
{ 121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128},
{ 45, 99, 188, 251, 195, 217, 255, 224, 128, 128, 128}
},
{
{ 1, 1, 251, 255, 213, 255, 128, 128, 128, 128, 128},
{ 203, 1, 248, 255, 255, 128, 128, 128, 128, 128, 128},
{ 137, 1, 177, 255, 224, 255, 128, 128, 128, 128, 128}
}
},
{
{
{ 253, 9, 248, 251, 207, 208, 255, 192, 128, 128, 128},
{ 175, 13, 224, 243, 193, 185, 249, 198, 255, 255, 128},
{ 73, 17, 171, 221, 161, 179, 236, 167, 255, 234, 128}
},
{
{ 1, 95, 247, 253, 212, 183, 255, 255, 128, 128, 128},
{ 239, 90, 244, 250, 211, 209, 255, 255, 128, 128, 128},
{ 155, 77, 195, 248, 188, 195, 255, 255, 128, 128, 128}
},
{
{ 1, 24, 239, 251, 218, 219, 255, 205, 128, 128, 128},
{ 201, 51, 219, 255, 196, 186, 128, 128, 128, 128, 128},
{ 69, 46, 190, 239, 201, 218, 255, 228, 128, 128, 128}
},
{
{ 1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128},
{ 223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128},
{ 141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128}
},
{
{ 1, 16, 248, 255, 255, 128, 128, 128, 128, 128, 128},
{ 190, 36, 230, 255, 236, 255, 128, 128, 128, 128, 128},
{ 149, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128}
},
{
{ 1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128}
},
{
{ 1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128},
{ 213, 62, 250, 255, 255, 128, 128, 128, 128, 128, 128},
{ 55, 93, 255, 128, 128, 128, 128, 128, 128, 128, 128}
},
{
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
}
},
{
{
{ 202, 24, 213, 235, 186, 191, 220, 160, 240, 175, 255},
{ 126, 38, 182, 232, 169, 184, 228, 174, 255, 187, 128},
{ 61, 46, 138, 219, 151, 178, 240, 170, 255, 216, 128}
},
{
{ 1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128},
{ 166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128},
    { 39, 77, 162, 232, 172, 180, 245, 178, 255, 255, 128}
},
{
{ 1, 52, 220, 246, 198, 199, 249, 220, 255, 255, 128},
{ 124, 74, 191, 243, 183, 193, 250, 221, 255, 255, 128},
{ 24, 71, 130, 219, 154, 170, 243, 182, 255, 255, 128}
},
{
{ 1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128},
{ 149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128},
{ 28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128}
},
{
{ 1, 81, 230, 252, 204, 203, 255, 192, 128, 128, 128},
{ 123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128},
{ 20, 95, 153, 243, 164, 173, 255, 203, 128, 128, 128}
},
{
{ 1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128},
{ 168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128},
{ 47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128}
},
{
{ 1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128},
{ 141, 84, 213, 252, 201, 202, 255, 219, 128, 128, 128},
{ 42, 80, 160, 240, 162, 185, 255, 205, 128, 128, 128}
},
{
{ 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 244, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 238, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128}
}
}
};


/* VP7 QP LUTs */
static const RK_U16 YDcQLookup[128] =
{
     4,    4,    5,    6,    6,    7,    8,    8,
     9,   10,   11,   12,   13,   14,   15,   16,
    17,   18,   19,   20,   21,   22,   23,   23,
    24,   25,   26,   27,   28,   29,   30,   31,
    32,   33,   33,   34,   35,   36,   36,   37,
    38,   39,   39,   40,   41,   41,   42,   43,
    43,   44,   45,   45,   46,   47,   48,   48,
    49,   50,   51,   52,   53,   53,   54,   56,
    57,   58,   59,   60,   62,   63,   65,   66,
    68,   70,   72,   74,   76,   79,   81,   84,
    87,   90,   93,   96,  100,  104,  108,  112,
   116,  121,  126,  131,  136,  142,  148,  154,
   160,  167,  174,  182,  189,  198,  206,  215,
   224,  234,  244,  254,  265,  277,  288,  301,
   313,  327,  340,  355,  370,  385,  401,  417,
   434,  452,  470,  489,  509,  529,  550,  572
};

static const RK_U16 YAcQLookup[128] =
{
     4,    4,    5,    5,    6,    6,    7,    8,
     9,   10,   11,   12,   13,   15,   16,   17,
    19,   20,   22,   23,   25,   26,   28,   29,
    31,   32,   34,   35,   37,   38,   40,   41,
    42,   44,   45,   46,   48,   49,   50,   51,
    53,   54,   55,   56,   57,   58,   59,   61,
    62,   63,   64,   65,   67,   68,   69,   70,
    72,   73,   75,   76,   78,   80,   82,   84,
    86,   88,   91,   93,   96,   99,  102,  105,
   109,  112,  116,  121,  125,  130,  135,  140,
   146,  152,  158,  165,  172,  180,  188,  196,
   205,  214,  224,  234,  245,  256,  268,  281,
   294,  308,  322,  337,  353,  369,  386,  404,
   423,  443,  463,  484,  506,  529,  553,  578,
   604,  631,  659,  688,  718,  749,  781,  814,
   849,  885,  922,  960, 1000, 1041, 1083, 1127

};

static const RK_U16 Y2DcQLookup[128] =
{
     7,    9,   11,   13,   15,   17,   19,   21,
    23,   26,   28,   30,   33,   35,   37,   39,
    42,   44,   46,   48,   51,   53,   55,   57,
    59,   61,   63,   65,   67,   69,   70,   72,
    74,   75,   77,   78,   80,   81,   83,   84,
    85,   87,   88,   89,   90,   92,   93,   94,
    95,   96,   97,   99,  100,  101,  102,  104,
   105,  106,  108,  109,  111,  113,  114,  116,
   118,  120,  123,  125,  128,  131,  134,  137,
   140,  144,  148,  152,  156,  161,  166,  171,
   176,  182,  188,  195,  202,  209,  217,  225,
   234,  243,  253,  263,  274,  285,  297,  309,
   322,  336,  350,  365,  381,  397,  414,  432,
   450,  470,  490,  511,  533,  556,  579,  604,
   630,  656,  684,  713,  742,  773,  805,  838,
   873,  908,  945,  983, 1022, 1063, 1105, 1148
};

static const RK_U16 Y2AcQLookup[128] =
{
     7,    9,   11,   13,   16,   18,   21,   24,
    26,   29,   32,   35,   38,   41,   43,   46,
    49,   52,   55,   58,   61,   64,   66,   69,
    72,   74,   77,   79,   82,   84,   86,   88,
    91,   93,   95,   97,   98,  100,  102,  104,
   105,  107,  109,  110,  112,  113,  115,  116,
   117,  119,  120,  122,  123,  125,  127,  128,
   130,  132,  134,  136,  138,  141,  143,  146,
   149,  152,  155,  158,  162,  166,  171,  175,
   180,  185,  191,  197,  204,  210,  218,  226,
   234,  243,  252,  262,  273,  284,  295,  308,
   321,  335,  350,  365,  381,  398,  416,  435,
   455,  476,  497,  520,  544,  569,  595,  622,
   650,  680,  711,  743,  776,  811,  848,  885,
   925,  965, 1008, 1052, 1097, 1144, 1193, 1244,
  1297, 1351, 1407, 1466, 1526, 1588, 1652, 1719
};

static const RK_U16 UvDcQLookup[128] =
{
     4,    4,    5,    6,    6,    7,    8,    8,
     9,   10,   11,   12,   13,   14,   15,   16,
    17,   18,   19,   20,   21,   22,   23,   23,
    24,   25,   26,   27,   28,   29,   30,   31,
    32,   33,   33,   34,   35,   36,   36,   37,
    38,   39,   39,   40,   41,   41,   42,   43,
    43,   44,   45,   45,   46,   47,   48,   48,
    49,   50,   51,   52,   53,   53,   54,   56,
    57,   58,   59,   60,   62,   63,   65,   66,
    68,   70,   72,   74,   76,   79,   81,   84,
    87,   90,   93,   96,  100,  104,  108,  112,
   116,  121,  126,  131,  132,  132,  132,  132,
   132,  132,  132,  132,  132,  132,  132,  132,
   132,  132,  132,  132,  132,  132,  132,  132,
   132,  132,  132,  132,  132,  132,  132,  132,
   132,  132,  132,  132,  132,  132,  132,  132
};


static const RK_U16 UvAcQLookup[128] =
{
     4,    4,    5,    5,    6,    6,    7,    8,
     9,   10,   11,   12,   13,   15,   16,   17,
    19,   20,   22,   23,   25,   26,   28,   29,
    31,   32,   34,   35,   37,   38,   40,   41,
    42,   44,   45,   46,   48,   49,   50,   51,
    53,   54,   55,   56,   57,   58,   59,   61,
    62,   63,   64,   65,   67,   68,   69,   70,
    72,   73,   75,   76,   78,   80,   82,   84,
    86,   88,   91,   93,   96,   99,  102,  105,
   109,  112,  116,  121,  125,  130,  135,  140,
   146,  152,  158,  165,  172,  180,  188,  196,
   205,  214,  224,  234,  245,  256,  268,  281,
   294,  308,  322,  337,  353,  369,  386,  404,
   423,  443,  463,  484,  506,  529,  553,  578,
   604,  631,  659,  688,  718,  749,  781,  814,
   849,  885,  922,  960, 1000, 1041, 1083, 1127
};

#define CLIP3(l, h, v) ((v) < (l) ? (l) : ((v) > (h) ? (h) : (v)))
#define SCAN(i)         HWIF_SCAN_MAP_ ## i

static const RK_U32 ScanTblRegId[16] = { 0 /* SCAN(0) */ ,
    SCAN(1), SCAN(2), SCAN(3), SCAN(4), SCAN(5), SCAN(6), SCAN(7), SCAN(8),
    SCAN(9), SCAN(10), SCAN(11), SCAN(12), SCAN(13), SCAN(14), SCAN(15)
};

#define TAP(i, j)       HWIF_PRED_BC_TAP_ ## i ## _ ## j

static const RK_U32 TapRegId[8][4] = {
    {TAP(0, 0), TAP(0, 1), TAP(0, 2), TAP(0, 3)},
    {TAP(1, 0), TAP(1, 1), TAP(1, 2), TAP(1, 3)},
    {TAP(2, 0), TAP(2, 1), TAP(2, 2), TAP(2, 3)},
    {TAP(3, 0), TAP(3, 1), TAP(3, 2), TAP(3, 3)},
    {TAP(4, 0), TAP(4, 1), TAP(4, 2), TAP(4, 3)},
    {TAP(5, 0), TAP(5, 1), TAP(5, 2), TAP(5, 3)},
    {TAP(6, 0), TAP(6, 1), TAP(6, 2), TAP(6, 3)},
    {TAP(7, 0), TAP(7, 1), TAP(7, 2), TAP(7, 3)}
};

static const RK_U32 mcFilter[8][6] =
{
    { 0,  0,  128,    0,   0,  0 },
    { 0, -6,  123,   12,  -1,  0 },
    { 2, -11, 108,   36,  -8,  1 },
    { 0, -9,   93,   50,  -6,  0 },
    { 3, -16,  77,   77, -16,  3 },
    { 0, -6,   50,   93,  -9,  0 },
    { 1, -8,   36,  108, -11,  2 },
    { 0, -1,   12,  123,  -6,  0 } };

#define BASE(i)         HWIF_DCT_STRM ## i ## _BASE
static const RK_U32 DctBaseId[] = { HWIF_VP8HWPART2_BASE,
    BASE(1), BASE(2), BASE(3), BASE(4), BASE(5), BASE(6), BASE(7) };

#define OFFSET(i)         HWIF_DCT ## i ## _START_BIT
static const RK_U32 DctStartBit[] = { HWIF_STRM_START_BIT,
    OFFSET(1), OFFSET(2), OFFSET(3), OFFSET(4),
    OFFSET(5), OFFSET(6), OFFSET(7) };

vp8decoder::vp8decoder()
{
	memset(&probTbl,0,sizeof(VPUMemLinear_t));
	memset(&bitstream,0,sizeof(VPUMemLinear_t));
	memset(&segmentMap,0,sizeof(VPUMemLinear_t));
	memset(this, 0, sizeof(vp8decoder));
    vpumem_ctx = NULL;
}

vp8decoder::~vp8decoder()
{
	if (probTbl.phy_addr)
		VPUFreeLinear(&probTbl);
	if (bitstream.phy_addr)
		VPUFreeLinear(&bitstream);
	if (segmentMap.phy_addr)
		VPUFreeLinear(&segmentMap);

	delete vpu_pp;
	delete regproc;
	delete frmanager;

	memset(this, 0, sizeof(vp8decoder));
}

void vp8decoder::vp8hwdBoolStart(RK_U8 *buffer, RK_U32 len)
{
	bitstr.lowvalue = 0;
	bitstr.range = 255;
	bitstr.count = 8;
	bitstr.buffer = buffer;
	bitstr.pos = 0;

	bitstr.value = (bitstr.buffer[0] << 24) + (bitstr.buffer[1] << 16) + (bitstr.buffer[2] << 8) + (bitstr.buffer[3]);

	bitstr.pos += 4;

	bitstr.streamEndPos = len;
	bitstr.strmError = bitstr.pos > bitstr.streamEndPos;
}

RK_U32 vp8decoder::vp8hwdDecodeBool(RK_S32 probability)
{
	RK_U32	bit = 0;
	RK_U32	split;
	RK_U32	bigsplit;
	RK_U32	count = bitstr.count;
	RK_U32	range = bitstr.range;
	RK_U32	value = bitstr.value;

	split = 1 + (((range - 1) * probability) >> 8);
	bigsplit = (split << 24);
	range = split;

	if(value >= bigsplit)
	{
		range = bitstr.range - split;
		value = value - bigsplit;
		bit = 1;
	}

	if(range >= 0x80)
	{
		bitstr.value = value;
		bitstr.range = range;
		return bit;
	}
	else
	{
		do
		{
			range += range;
			value += value;

			if(!--count)
			{
				/* no more stream to read? */
				if (bitstr.pos >= bitstr.streamEndPos)
				{
					bitstr.strmError = 1;
					break;
				}
				count = 8;
				value |= bitstr.buffer[bitstr.pos];
				bitstr.pos++;
			}
		}while(range < 0x80);
	}


	bitstr.count = count;
	bitstr.value = value;
	bitstr.range = range;

	return bit;
}

RK_U32 vp8decoder::vp8hwdDecodeBool128()
{
	RK_U32 bit = 0;
	RK_U32 split;
	RK_U32 bigsplit;
	RK_U32 count = bitstr.count;
	RK_U32 range = bitstr.range;
	RK_U32 value = bitstr.value;

	split = (range + 1) >> 1;
	bigsplit = (split << 24);
	range = split;

	if(value >= bigsplit)
	{
		range = (bitstr.range - split);
		value = (value - bigsplit);
		bit = 1;
	}

	if (range >= 0x80)
	{
		bitstr.value = value;
		bitstr.range = range;
		return bit;
	}
	else
	{
		range <<= 1;
		value <<= 1;

		if(!--count)
		{
			/* no more stream to read? */
			if (bitstr.pos >= bitstr.streamEndPos)
			{
				bitstr.strmError = 1;
				return 0; /* any value, not valid */
			}
			count = 8;
			value |= bitstr.buffer[bitstr.pos];
			bitstr.pos++;
		}
	}

	bitstr.count = count;
	bitstr.value = value;
	bitstr.range = range;

	return bit;
}

RK_U32 vp8decoder::vp8hwdReadBits(RK_S32 bits)
{
	RK_U32 z = 0;
	RK_S32 bit;

	for(bit = bits - 1; bit >= 0; bit--)
	{
		z |= (vp8hwdDecodeBool128() << bit);
	}

	return z;
}

RK_U32 vp8decoder::ScaleDimension( RK_U32 orig, RK_U32 scale )
{
    switch(scale)
    {
        case 0:
            return orig;
            break;
        case 1: /* 5/4 */
            return (5*orig)/4;
            break;
        case 2: /* 5/3 */
            return (5*orig)/3;
            break;
        case 3: /* 2 */
            return 2*orig;
            break;
    }
	return orig;
}

RK_S32 vp8decoder::DecodeQuantizerDelta()
{
	RK_S32	result = 0;

	if(vp8hwdDecodeBool128())
	{
		result = vp8hwdReadBits(4);
		if (vp8hwdDecodeBool128())
			result = -result;
	}
	return result;
}


void vp8decoder::vp8hwdResetProbs()
{
	RK_U32 i, j, k, l;
	static const RK_U32 Vp7DefaultScan[] = {
		0,  1,  4,  8,  5,  2,  3,  6,
		9, 12, 13, 10,  7, 11, 14, 15,
	};

	if (segmentMap.vir_addr)
		memset(segmentMap.vir_addr,0,segmentMap.size);

	for( i = 0 ; i < 16 ; ++i )
		vp7ScanOrder[i] = Vp7DefaultScan[i];


	/* Intra-prediction modes */
	entropy.probLuma16x16PredMode[0] = 112;
	entropy.probLuma16x16PredMode[1] = 86;
	entropy.probLuma16x16PredMode[2] = 140;
	entropy.probLuma16x16PredMode[3] = 37;
	entropy.probChromaPredMode[0] = 162;
	entropy.probChromaPredMode[1] = 101;
	entropy.probChromaPredMode[2] = 204;

	for (i=0; i<MAX_NBR_OF_MB_REF_LF_DELTAS; i++)
		mbRefLfDelta[i] = 0;

	for (i=0; i<MAX_NBR_OF_MB_MODE_LF_DELTAS; i++)
		mbModeLfDelta[i] = 0;

	/* MV context */
	k = 0;
	if(decMode == VP8HWD_VP8)
	{
		for( i = 0 ; i < 2 ; ++i )
			for( j = 0 ; j < VP8_MV_PROBS_PER_COMPONENT ; ++j, ++k )
				entropy.probMvContext[i][j] = Vp8DefaultMvProbs[i][j];
	}
	else
	{
		for( i = 0 ; i < 2 ; ++i )
			for( j = 0 ; j < VP7_MV_PROBS_PER_COMPONENT ; ++j, ++k )
				entropy.probMvContext[i][j] = Vp7DefaultMvProbs[i][j];
	}

	/* Coefficients */
	for( i = 0 ; i < 4 ; ++i )
		for( j = 0 ; j < 8 ; ++j )
			for( k = 0 ; k < 3 ; ++k )
				for( l = 0 ; l < 11 ; ++l )
					entropy.probCoeffs[i][j][k][l] = DefaultCoeffProbs[i][j][k][l];
}

void vp8decoder::vp8hwdDecodeCoeffUpdate()
{
	RK_U32 i, j, k, l;

	for( i = 0; i < 4; i++ )
	{
		for ( j = 0; j < 8; j++ )
		{
			for ( k = 0; k < 3; k++ )
			{
				for ( l = 0; l < 11; l++ )
				{
					if (vp8hwdDecodeBool(CoeffUpdateProbs[i][j][k][l]))
						entropy.probCoeffs[i][j][k][l] = vp8hwdReadBits(8);
				}
			}
		}
	}
}
int	vp8decoder::decoder_frame_header(RK_U8 *pbase, RK_U32 size)
{
#define VP8_KEY_FRAME_START_CODE    0x9d012a
	RK_U32	tmp;
	int		i,j;

	keyFrame = !(pbase[0]&1);
	vpVersion = (pbase[0]>>1)&7;
	showFrame = 1;
	if (decMode == VP8HWD_VP7)
	{
		offsetToDctParts = (pbase[0]>>4)|(pbase[1]<<4)|(pbase[2]<<12);
		frameTagSize = vpVersion>=1? 3:4;
	}
	else
	{
		offsetToDctParts = (pbase[0]>>5)|(pbase[1]<<3)|(pbase[2]<<11);
		showFrame = (pbase[0]>>4)&1;
		frameTagSize = 3;
	}
	pbase += frameTagSize;
	size -= frameTagSize;
	if (keyFrame)
		vp8hwdResetProbs();
	if (decMode == VP8HWD_VP8)
	{
		if (keyFrame)
		{
			tmp = (pbase[0]<<16)|(pbase[1]<<8)|(pbase[2]<<0);
			if (tmp != VP8_KEY_FRAME_START_CODE)
				return VP8_Dec_Unsurport;
			tmp = (pbase[3]<<0)|(pbase[4]<<8);
			width = tmp&0x3fff;
			scaledWidth = ScaleDimension(width, tmp>>14);
			tmp = (pbase[5]<<0)|(pbase[6]<<8);
			height = tmp&0x3fff;
			scaledHeight = ScaleDimension(height, tmp>>14);
			pbase += 7;
			size -= 7;
		}
		vp8hwdBoolStart(pbase,size);
		if (keyFrame)
		{
			colorSpace = (vpColorSpace_e)vp8hwdDecodeBool128();
			clamping = vp8hwdDecodeBool128();
		}
		segmentationEnabled = vp8hwdDecodeBool128();
        segmentationMapUpdate = 0;
		if (segmentationEnabled)
		{
			segmentationMapUpdate = vp8hwdDecodeBool128();
			if (vp8hwdDecodeBool128())		/* Segmentation map update */
			{
				segmentFeatureMode = vp8hwdDecodeBool128();
				memset(&segmentQp[0],0,MAX_NBR_OF_SEGMENTS*sizeof(RK_S32));
				memset(&segmentLoopfilter[0],0,MAX_NBR_OF_SEGMENTS*sizeof(RK_S32));
				for (i=0; i<MAX_NBR_OF_SEGMENTS; i++)
				{
					if (vp8hwdDecodeBool128())
					{
						segmentQp[i] = vp8hwdReadBits(7);
						if (vp8hwdDecodeBool128())
							segmentQp[i] = -segmentQp[i];
					}
				}
				for (i=0; i<MAX_NBR_OF_SEGMENTS; i++)
				{
					if (vp8hwdDecodeBool128())
					{
						segmentLoopfilter[i] = vp8hwdReadBits(6);
						if (vp8hwdDecodeBool128())
							segmentLoopfilter[i] = -segmentLoopfilter[i];
					}
				}
			}
			if (segmentationMapUpdate)
			{
				probSegment[0] = 255;
				probSegment[1] = 255;
				probSegment[2] = 255;
				for (i=0; i<3; i++)
				{
					if (vp8hwdDecodeBool128())
					{
						probSegment[i] = vp8hwdReadBits(8);
					}
				}
			}
			if (bitstr.strmError)
				return VP8_Dec_Unsurport;
		}
		loopFilterType = vp8hwdDecodeBool128();
		loopFilterLevel = vp8hwdReadBits(6);
		loopFilterSharpness = vp8hwdReadBits(3);
		modeRefLfEnabled = vp8hwdDecodeBool128();
		if (modeRefLfEnabled)
		{
			if (vp8hwdDecodeBool128())
			{
				for (i=0; i<MAX_NBR_OF_MB_REF_LF_DELTAS; i++)
				{
					if (vp8hwdDecodeBool128())
					{
						mbRefLfDelta[i] = vp8hwdReadBits(6);
						if (vp8hwdDecodeBool128())
							mbRefLfDelta[i] = -mbRefLfDelta[i];
					}
				}
				for (i=0; i<MAX_NBR_OF_MB_MODE_LF_DELTAS; i++)
				{
					if (vp8hwdDecodeBool128())
					{
						mbModeLfDelta[i] = vp8hwdReadBits(6);
						if (vp8hwdDecodeBool128())
							mbModeLfDelta[i] = -mbModeLfDelta[i];
					}
				}
			}
		}
		if (bitstr.strmError)
			return VP8_Dec_Unsurport;
		nbrDctPartitions = 1<<vp8hwdReadBits(2);
		qpYAc = vp8hwdReadBits(7);
		qpYDc = DecodeQuantizerDelta();
		qpY2Dc = DecodeQuantizerDelta();
		qpY2Ac = DecodeQuantizerDelta();
		qpChDc = DecodeQuantizerDelta();
		qpChAc = DecodeQuantizerDelta();
		if (keyFrame)
		{
			refreshGolden          = 1;
			refreshAlternate       = 1;
			copyBufferToGolden     = 0;
			copyBufferToAlternate  = 0;

			/* Refresh entropy probs */
			refreshEntropyProbs = vp8hwdDecodeBool128();

			refFrameSignBias[0] = 0;
			refFrameSignBias[1] = 0;
			refreshLast = 1;
		}
		else
		{
			 /* Refresh golden */
			refreshGolden = vp8hwdDecodeBool128();
			/* Refresh alternate */
			refreshAlternate = vp8hwdDecodeBool128();
			if( refreshGolden == 0 )
			{
				/* Copy to golden */
				copyBufferToGolden = vp8hwdReadBits(2);
			}
			else
				copyBufferToGolden = 0;

			if( refreshAlternate == 0 )
			{
				/* Copy to alternate */
				copyBufferToAlternate = vp8hwdReadBits(2);
			}
			else
				copyBufferToAlternate = 0;

			/* Sign bias for golden frame */
			refFrameSignBias[0] = vp8hwdDecodeBool128();
			/* Sign bias for alternate frame */
			refFrameSignBias[1] = vp8hwdDecodeBool128();
			/* Refresh entropy probs */
			refreshEntropyProbs = vp8hwdDecodeBool128();
			/* Refresh last */
			refreshLast = vp8hwdDecodeBool128();
		}

		 /* Make a "backup" of current entropy probabilities if refresh is not set */
		if(refreshEntropyProbs == 0)
		{
			memcpy((void*)&entropyLast, (void*)&entropy, (unsigned long)sizeof(vp8EntropyProbs_t));
			memcpy( (void*)vp7PrevScanOrder, (void*)vp7ScanOrder, (unsigned long)sizeof(vp7ScanOrder));
		}

	 	vp8hwdDecodeCoeffUpdate();
		coeffSkipMode =  vp8hwdDecodeBool128();
		if (!keyFrame)
		{
			RK_U32	mvProbs;

			probMbSkipFalse = vp8hwdReadBits(8);
			probIntra = vp8hwdReadBits(8);
			probRefLast = vp8hwdReadBits(8);
			probRefGolden = vp8hwdReadBits(8);
			if (vp8hwdDecodeBool128())
			{
				for(i=0; i<4; i++)
					entropy.probLuma16x16PredMode[i] = vp8hwdReadBits(8);
			}
			if (vp8hwdDecodeBool128())
			{
				for(i=0; i<3; i++)
					entropy.probChromaPredMode[i] = vp8hwdReadBits(8);
			}
			mvProbs = VP8_MV_PROBS_PER_COMPONENT;
			for( i = 0 ; i < 2 ; ++i )
			{
				for( j = 0 ; j < mvProbs ; ++j )
				{
					if(vp8hwdDecodeBool(MvUpdateProbs[i][j])==1)
					{
						tmp = vp8hwdReadBits(7);
						if( tmp )
							tmp = tmp << 1;
						else
							tmp = 1;
						entropy.probMvContext[i][j] = tmp;
					}
				}
			}
		}
		else
		{
			probMbSkipFalse = vp8hwdReadBits(8);
		}
		if (bitstr.strmError)
			return VP8_Dec_Unsurport;

	}
	else
	{
		vp8hwdBoolStart(pbase,size);
		if (keyFrame)
		{
			width = vp8hwdReadBits(12);
			height = vp8hwdReadBits(12);
			tmp = vp8hwdReadBits(2);
			scaledWidth = ScaleDimension(width, tmp);
			tmp = vp8hwdReadBits(2);
			scaledHeight = ScaleDimension(height, tmp);
		}
		{
			const RK_U32 vp70FeatureBits[4] = { 7, 6, 0, 8 };
			const RK_U32 vp71FeatureBits[4] = { 7, 6, 0, 5 };
			const RK_U32 *featureBits;
			if(vpVersion == 0)
				featureBits = vp70FeatureBits;
			else
				featureBits = vp71FeatureBits;
			for (i=0; i<MAX_NBR_OF_VP7_MB_FEATURES; i++)
			{
				if (vp8hwdDecodeBool128())
				{
					tmp = vp8hwdReadBits(8);
					for (j=0; j<3; j++)
					{
						if (vp8hwdDecodeBool128())
							tmp = vp8hwdReadBits(8);
					}
					if (featureBits[i])
					{
						for (j=0; j<4; j++)
						{
							if (vp8hwdDecodeBool128())
								tmp = vp8hwdReadBits(featureBits[i]);
						}
					}
					return VP8_Dec_Unsurport;
				}

			}
			nbrDctPartitions = 1;
		}
		qpYAc = vp8hwdReadBits(  7 );
		qpYDc  = vp8hwdReadBits(  1 ) ? vp8hwdReadBits(  7 ) : qpYAc;
		qpY2Dc = vp8hwdReadBits(  1 ) ? vp8hwdReadBits(  7 ) : qpYAc;
		qpY2Ac = vp8hwdReadBits(  1 ) ? vp8hwdReadBits(  7 ) : qpYAc;
		qpChDc = vp8hwdReadBits(  1 ) ? vp8hwdReadBits(  7 ) : qpYAc;
		qpChAc = vp8hwdReadBits(  1 ) ? vp8hwdReadBits(  7 ) : qpYAc;
		if (!keyFrame)
		{
			refreshGolden = vp8hwdDecodeBool128();
			if (vpVersion >= 1)
			{
				refreshEntropyProbs = vp8hwdDecodeBool128();
				refreshLast = vp8hwdDecodeBool128();
			}
			else
			{
				refreshEntropyProbs = 1;
				refreshLast = 1;
			}
		}
		else
		{
			refreshGolden = 1;
			refreshAlternate = 1;
			copyBufferToGolden = 0;
			copyBufferToAlternate = 0;
			if (vpVersion >= 1)
				refreshEntropyProbs = vp8hwdDecodeBool128();
			else
				refreshEntropyProbs = 1;
			refFrameSignBias[0] = 0;
			refFrameSignBias[1] = 0;
			refreshLast = 1;
		}

		if (!refreshEntropyProbs)
		{
			memcpy(&entropyLast, &entropy, (unsigned long)sizeof(vp8EntropyProbs_t));
			memcpy(vp7PrevScanOrder, vp7ScanOrder, (unsigned long)sizeof(vp7ScanOrder));
		}
		if (refreshLast)
		{
			if (vp8hwdDecodeBool128())
			{
				tmp = vp8hwdReadBits(8);
				tmp = vp8hwdReadBits(8);
				return VP8_Dec_Unsurport;
			}
		}
		if (vpVersion == 0)
		{
			loopFilterType = vp8hwdDecodeBool128();
		}
		if (vp8hwdDecodeBool128())
		{
			static const RK_U32 Vp7DefaultScan[] = {
				0,  1,  4,  8,  5,  2,  3,  6,
				9, 12, 13, 10,  7, 11, 14, 15,
				};
			vp7ScanOrder[0] = 0;
			for (i=1; i<16; i++)
				vp7ScanOrder[i] = Vp7DefaultScan[vp8hwdReadBits(4)];
		}
		if (vpVersion >= 1)
			loopFilterType = vp8hwdDecodeBool128();
		loopFilterLevel = vp8hwdReadBits(6);
		loopFilterSharpness = vp8hwdReadBits(3);
		vp8hwdDecodeCoeffUpdate();
		if (!keyFrame)
		{
			probIntra = vp8hwdReadBits(8);
			probRefLast = vp8hwdReadBits(8);
			if (vp8hwdDecodeBool128())
			{
				for (i=0; i<4; i++)
					entropy.probLuma16x16PredMode[i] = vp8hwdReadBits(8);
			}
			if (vp8hwdDecodeBool128())
			{
				for (i=0; i<3; i++)
					entropy.probChromaPredMode[i] = vp8hwdReadBits(8);
			}
			for( i = 0 ; i < 2 ; ++i )
			{
				for( j = 0 ; j < VP7_MV_PROBS_PER_COMPONENT ; ++j )
				{
					if(vp8hwdDecodeBool(MvUpdateProbs[i][j]))
					{
						tmp = vp8hwdReadBits(7);
						if( tmp )
							tmp = tmp << 1;
						else
							tmp = 1;
						entropy.probMvContext[i][j] = tmp;
					}
				}
			}
		}
	}
	if (bitstr.strmError)
		return VP8_Dec_Unsurport;
	mb_width = (width+15)>>4;
	mb_height = (height+15)>>4;
	return VP8_Dec_OK;
}

int vp8decoder::vp8hwdSetPartitionOffsets(RK_U8 *stream, RK_U32 len)
{
	RK_U32 i = 0;
	RK_U32 offset = 0;
	RK_U32 baseOffset;
	RK_U32 extraBytesPacked = 0;

	if(decMode == VP8HWD_VP8 && keyFrame)
		extraBytesPacked += 7;

	stream += frameTagSize;

	baseOffset = frameTagSize + offsetToDctParts + 3*(nbrDctPartitions-1);

	stream += offsetToDctParts + extraBytesPacked;
	for( i = 0 ; i < nbrDctPartitions - 1 ; ++i )
	{
		RK_U32	tmp;

		dctPartitionOffsets[i] = baseOffset + offset;
		tmp = stream[0]|(stream[1]<<8)|(stream[2]<<16);
		offset += tmp;
		stream += 3;
	}
	dctPartitionOffsets[i] = baseOffset + offset;

	return (dctPartitionOffsets[i] < len ? VP8_Dec_OK : VP8_Dec_Unsurport);
}

void vp8decoder::VP8HwdAsicProbUpdate(void)
{
	RK_U8	*dst;
	RK_U32	i, j, k;

	/* first probs */
	dst = (RK_U8*)probTbl.vir_addr;

	dst[0] = probMbSkipFalse;
	dst[1] = probIntra;
	dst[2] = probRefLast;
	dst[3] = probRefGolden;
	dst[4] = probSegment[0];
	dst[5] = probSegment[1];
	dst[6] = probSegment[2];
	dst[7] = 0; /*unused*/

	dst += 8;
	dst[0] = entropy.probLuma16x16PredMode[0];
	dst[1] = entropy.probLuma16x16PredMode[1];
	dst[2] = entropy.probLuma16x16PredMode[2];
	dst[3] = entropy.probLuma16x16PredMode[3];
	dst[4] = entropy.probChromaPredMode[0];
	dst[5] = entropy.probChromaPredMode[1];
	dst[6] = entropy.probChromaPredMode[2];
	dst[7] = 0; /*unused*/

	/* mv probs */
	dst += 8;
	dst[0] = entropy.probMvContext[0][0]; /* is short */
	dst[1] = entropy.probMvContext[1][0];
	dst[2] = entropy.probMvContext[0][1]; /* sign */
	dst[3] = entropy.probMvContext[1][1];
	dst[4] = entropy.probMvContext[0][8+9];
	dst[5] = entropy.probMvContext[0][9+9];
	dst[6] = entropy.probMvContext[1][8+9];
	dst[7] = entropy.probMvContext[1][9+9];
	dst += 8;
	for( i = 0 ; i < 2 ; ++i )
	{
		for( j = 0 ; j < 8 ; j+=4 )
		{
			dst[0] = entropy.probMvContext[i][j+9+0];
			dst[1] = entropy.probMvContext[i][j+9+1];
			dst[2] = entropy.probMvContext[i][j+9+2];
			dst[3] = entropy.probMvContext[i][j+9+3];
			dst += 4;
		}
	}
	for( i = 0 ; i < 2 ; ++i )
	{
		dst[0] = entropy.probMvContext[i][0+2];
		dst[1] = entropy.probMvContext[i][1+2];
		dst[2] = entropy.probMvContext[i][2+2];
		dst[3] = entropy.probMvContext[i][3+2];
		dst[4] = entropy.probMvContext[i][4+2];
		dst[5] = entropy.probMvContext[i][5+2];
		dst[6] = entropy.probMvContext[i][6+2];
		dst[7] = 0; /*unused*/
		dst += 8;
	}

	/* coeff probs (header part) */
	dst = (RK_U8*)probTbl.vir_addr;
	dst += (8*7);
	for( i = 0 ; i < 4 ; ++i )
	{
		for( j = 0 ; j < 8 ; ++j )
		{
			for( k = 0 ; k < 3 ; ++k )
			{
				dst[0] = entropy.probCoeffs[i][j][k][0];
				dst[1] = entropy.probCoeffs[i][j][k][1];
				dst[2] = entropy.probCoeffs[i][j][k][2];
				dst[3] = entropy.probCoeffs[i][j][k][3];
				dst += 4;
			}
		}
	}

	/* coeff probs (footer part) */
	dst = (RK_U8*)probTbl.vir_addr;
	dst += (8*55);
	for( i = 0 ; i < 4 ; ++i )
	{
		for( j = 0 ; j < 8 ; ++j )
		{
			for( k = 0 ; k < 3 ; ++k )
			{
				dst[0] = entropy.probCoeffs[i][j][k][4];
				dst[1] = entropy.probCoeffs[i][j][k][5];
				dst[2] = entropy.probCoeffs[i][j][k][6];
				dst[3] = entropy.probCoeffs[i][j][k][7];
				dst[4] = entropy.probCoeffs[i][j][k][8];
				dst[5] = entropy.probCoeffs[i][j][k][9];
				dst[6] = entropy.probCoeffs[i][j][k][10];
				dst[7] = 0; /*unused*/
				dst += 8;
			}
		}
	}
}

int	vp8decoder::decinit_hwcfg()
{
	regproc->SetRegisterFile(HWIF_DEC_AXI_RD_ID, 0);
	regproc->SetRegisterFile(HWIF_DEC_TIMEOUT_E, 1);
	regproc->SetRegisterFile(HWIF_DEC_STRSWAP32_E, 1);
	regproc->SetRegisterFile(HWIF_DEC_STRENDIAN_E, 1);
	regproc->SetRegisterFile(HWIF_DEC_INSWAP32_E, 1);
	regproc->SetRegisterFile(HWIF_DEC_OUTSWAP32_E, 1);

	regproc->SetRegisterFile(HWIF_DEC_CLK_GATE_E, 1);
	regproc->SetRegisterFile(HWIF_DEC_IN_ENDIAN, 1);
	regproc->SetRegisterFile(HWIF_DEC_OUT_ENDIAN, 1);

	regproc->SetRegisterFile(HWIF_DEC_OUT_TILED_E, 0);
	regproc->SetRegisterFile(HWIF_DEC_MAX_BURST, 16);
	regproc->SetRegisterFile(HWIF_DEC_SCMD_DIS, 0);
	regproc->SetRegisterFile(HWIF_DEC_ADV_PRE_DIS, 0);
	regproc->SetRegisterFile(HWIF_DEC_LATENCY, 0);
	regproc->SetRegisterFile(HWIF_DEC_DATA_DISC_E, 0);

	regproc->SetRegisterFile(HWIF_DEC_IRQ, 0);
	regproc->SetRegisterFile(HWIF_DEC_AXI_WR_ID, 0);

	regproc->SetRegisterFile(HWIF_DEC_MODE, decMode == VP8HWD_VP7 ?  DEC_MODE_VP7 : DEC_MODE_VP8);

	regproc->SetRegisterFile(HWIF_SEGMENT_BASE,segmentMap.phy_addr);
    regproc->SetRegisterFile(HWIF_VP8HWPROBTBL_BASE,probTbl.phy_addr);

	return VP8_Dec_OK;
}

int	vp8decoder::frame_hwcfg()
{
	if (!Init_memed)
	{
		RK_U32	memory_size;

		Init_memed = 1;
		memory_size = (mb_width*mb_height+3)>>2;
		memory_size = 64*((memory_size + 63) >> 6);
		int err = VPUMallocLinear(&segmentMap, memory_size);
        if(err)
        {
           ALOGE("VPU_mem allco fail \n");
           return -1;
        }
		memset(segmentMap.vir_addr,0,segmentMap.size);
		regproc->SetRegisterFile(HWIF_SEGMENT_BASE, segmentMap.phy_addr);
	} else {
		regproc->SetRegisterFile(HWIF_SEGMENT_BASE, segmentMap.phy_addr);
	}
	regproc->SetRegisterFile(HWIF_PIC_MB_WIDTH, mb_width & 0x1FF);
	regproc->SetRegisterFile(HWIF_PIC_MB_HEIGHT_P, mb_height & 0xFF);
	regproc->SetRegisterFile(HWIF_PIC_MB_W_EXT, mb_width >> 9);
	regproc->SetRegisterFile(HWIF_PIC_MB_H_EXT, mb_height >> 8);

	regproc->SetRegisterFile(HWIF_DEC_OUT_DIS, 0);

	regproc->SetRegisterFile(HWIF_DEC_OUT_BASE,frame_out->vpumem.phy_addr);
	if (keyFrame){
        if(!VPUMemJudgeIommu()){
		    regproc->SetRegisterFile(HWIF_REFER0_BASE, frame_out->vpumem.phy_addr+((mb_width*mb_height)<<8));
        }else{
            if((mb_width*mb_height)<<8 > 0x400000){
                ALOGE("mb_width*mb_height is big then 0x400000,iommu err");
            }
		    regproc->SetRegisterFile(HWIF_REFER0_BASE, frame_out->vpumem.phy_addr |((mb_width*mb_height)<<18));
        }
	}
	else if (frame_ref&&frame_ref->vpumem.phy_addr){
		regproc->SetRegisterFile(HWIF_REFER0_BASE, frame_ref->vpumem.phy_addr);
	}else{
		regproc->SetRegisterFile(HWIF_REFER0_BASE, frame_out->vpumem.phy_addr);
	}

	/* golden reference */
    RK_U32 golden_phyaddr = 0;
	if (frame_golden&&frame_golden->vpumem.phy_addr){
        golden_phyaddr = frame_golden->vpumem.phy_addr;
		//regproc->SetRegisterFile(HWIF_REFER4_BASE, frame_golden->vpumem.phy_addr);
	}else{
        golden_phyaddr = frame_out->vpumem.phy_addr;
		//regproc->SetRegisterFile(HWIF_REFER4_BASE, frame_out->vpumem.phy_addr);
	}
    if(!VPUMemJudgeIommu()){
		regproc->SetRegisterFile(HWIF_REFER4_BASE,golden_phyaddr);
	    regproc->SetRegisterFile(HWIF_GREF_SIGN_BIAS, refFrameSignBias[0]);
    }else{
        //regproc->SetRegisterFile(HWIF_GREF_SIGN_BIAS, refFrameSignBias[0]);
		regproc->SetRegisterFile(HWIF_REFER4_BASE,golden_phyaddr |(refFrameSignBias[0] << 10));
    }

	/* alternate reference */
    RK_U32 alternate_phyaddr = 0;
	if (frame_alternate&&frame_alternate->vpumem.phy_addr){
        alternate_phyaddr = frame_alternate->vpumem.phy_addr;
		//regproc->SetRegisterFile(HWIF_REFER5_BASE, frame_alternate->vpumem.phy_addr);
	}
	else{
        alternate_phyaddr = frame_out->vpumem.phy_addr;
		//regproc->SetRegisterFile(HWIF_REFER5_BASE, frame_out->vpumem.phy_addr);
	}

    if(!VPUMemJudgeIommu()){
		regproc->SetRegisterFile(HWIF_REFER5_BASE,alternate_phyaddr);
	    regproc->SetRegisterFile(HWIF_GREF_SIGN_BIAS, refFrameSignBias[1]);
    }else{
	    //regproc->SetRegisterFile(HWIF_AREF_SIGN_BIAS, refFrameSignBias[1]);
		regproc->SetRegisterFile(HWIF_REFER5_BASE,alternate_phyaddr | (refFrameSignBias[1] << 10));
    }

	regproc->SetRegisterFile( HWIF_PIC_INTER_E, !keyFrame);

	/* mb skip mode [Codec X] */
	regproc->SetRegisterFile( HWIF_SKIP_MODE, !coeffSkipMode );

	/* loop filter */
	regproc->SetRegisterFile( HWIF_FILT_TYPE, loopFilterType);
	regproc->SetRegisterFile( HWIF_FILT_SHARPNESS, loopFilterSharpness);

	if (!segmentationEnabled)
		regproc->SetRegisterFile( HWIF_FILT_LEVEL_0, loopFilterLevel);
	else if (segmentFeatureMode) /* absolute mode */
	{
		regproc->SetRegisterFile( HWIF_FILT_LEVEL_0, segmentLoopfilter[0]);
		regproc->SetRegisterFile( HWIF_FILT_LEVEL_1, segmentLoopfilter[1]);
		regproc->SetRegisterFile( HWIF_FILT_LEVEL_2, segmentLoopfilter[2]);
		regproc->SetRegisterFile( HWIF_FILT_LEVEL_3, segmentLoopfilter[3]);
	}
	else /* delta mode */
	{
		regproc->SetRegisterFile( HWIF_FILT_LEVEL_0, CLIP3(0, 63, loopFilterLevel + segmentLoopfilter[0]));
		regproc->SetRegisterFile( HWIF_FILT_LEVEL_1, CLIP3(0, 63, loopFilterLevel + segmentLoopfilter[1]));
		regproc->SetRegisterFile( HWIF_FILT_LEVEL_2, CLIP3(0, 63, loopFilterLevel + segmentLoopfilter[2]));
		regproc->SetRegisterFile( HWIF_FILT_LEVEL_3, CLIP3(0, 63, loopFilterLevel + segmentLoopfilter[3]));
	}

	if (!VPUMemJudgeIommu()) {
		regproc->SetRegisterFile( HWIF_SEGMENT_E, segmentationEnabled );
		regproc->SetRegisterFile( HWIF_SEGMENT_UPD_E, segmentationMapUpdate );
	} else {
		/*regproc->SetRegisterFile( HWIF_SEGMENT_E, segmentationEnabled );
		regproc->SetRegisterFile( HWIF_SEGMENT_UPD_E, segmentationMapUpdate );*/
		regproc->SetRegisterFile( HWIF_SEGMENT_BASE, segmentMap.phy_addr | ((segmentationEnabled + (segmentationMapUpdate << 1)) << 10));
	}

	/* TODO: seems that ref dec does not disable filtering based on version,
	* check */
	/*regproc->SetRegisterFile( HWIF_FILTERING_DIS, vpVersion >= 2);*/
	regproc->SetRegisterFile( HWIF_FILTERING_DIS, loopFilterLevel == 0);

	/* full pell chroma mvs for VP8 version 3 */
	regproc->SetRegisterFile( HWIF_CH_MV_RES, decMode == VP8HWD_VP7 || vpVersion != 3);

	regproc->SetRegisterFile( HWIF_BILIN_MC_E, decMode == VP8HWD_VP8 && (vpVersion & 0x3));

	/* first bool decoder status */
	regproc->SetRegisterFile( HWIF_BOOLEAN_VALUE, (bitstr.value >> 24) & (0xFFU));

	regproc->SetRegisterFile( HWIF_BOOLEAN_RANGE, bitstr.range & (0xFFU));

	/* QP */
	if (decMode == VP8HWD_VP7)
	{
		/* LUT */
		regproc->SetRegisterFile( HWIF_QUANT_0, YDcQLookup[qpYDc]);
		regproc->SetRegisterFile( HWIF_QUANT_1, YAcQLookup[qpYAc]);
		regproc->SetRegisterFile( HWIF_QUANT_2, Y2DcQLookup[qpY2Dc]);
		regproc->SetRegisterFile( HWIF_QUANT_3, Y2AcQLookup[qpY2Ac]);
		regproc->SetRegisterFile( HWIF_QUANT_4, UvDcQLookup[qpChDc]);
		regproc->SetRegisterFile( HWIF_QUANT_5, UvAcQLookup[qpChAc]);
	}
	else
	{
		if (!segmentationEnabled)
			regproc->SetRegisterFile( HWIF_QUANT_0, qpYAc);
		else if (segmentFeatureMode) /* absolute mode */
		{
			regproc->SetRegisterFile( HWIF_QUANT_0, segmentQp[0]);
			regproc->SetRegisterFile( HWIF_QUANT_1, segmentQp[1]);
			regproc->SetRegisterFile( HWIF_QUANT_2, segmentQp[2]);
			regproc->SetRegisterFile( HWIF_QUANT_3, segmentQp[3]);
		}
		else /* delta mode */
		{
			regproc->SetRegisterFile( HWIF_QUANT_0, CLIP3(0, 127, qpYAc + segmentQp[0]));
			regproc->SetRegisterFile( HWIF_QUANT_1, CLIP3(0, 127, qpYAc + segmentQp[1]));
			regproc->SetRegisterFile( HWIF_QUANT_2, CLIP3(0, 127, qpYAc + segmentQp[2]));
			regproc->SetRegisterFile( HWIF_QUANT_3, CLIP3(0, 127, qpYAc + segmentQp[3]));
		}
		regproc->SetRegisterFile( HWIF_QUANT_DELTA_0, qpYDc);
		regproc->SetRegisterFile( HWIF_QUANT_DELTA_1, qpY2Dc);
		regproc->SetRegisterFile( HWIF_QUANT_DELTA_2, qpY2Ac);
		regproc->SetRegisterFile( HWIF_QUANT_DELTA_3, qpChDc);
		regproc->SetRegisterFile( HWIF_QUANT_DELTA_4, qpChAc);

		if (modeRefLfEnabled)
		{
			regproc->SetRegisterFile( HWIF_FILT_REF_ADJ_0,  mbRefLfDelta[0]);
			regproc->SetRegisterFile( HWIF_FILT_REF_ADJ_1,  mbRefLfDelta[1]);
			regproc->SetRegisterFile( HWIF_FILT_REF_ADJ_2,  mbRefLfDelta[2]);
			regproc->SetRegisterFile( HWIF_FILT_REF_ADJ_3,  mbRefLfDelta[3]);
			regproc->SetRegisterFile( HWIF_FILT_MB_ADJ_0,  mbModeLfDelta[0]);
			regproc->SetRegisterFile( HWIF_FILT_MB_ADJ_1,  mbModeLfDelta[1]);
			regproc->SetRegisterFile( HWIF_FILT_MB_ADJ_2,  mbModeLfDelta[2]);
			regproc->SetRegisterFile( HWIF_FILT_MB_ADJ_3,  mbModeLfDelta[3]);
		}
		else
		{
			regproc->SetRegisterFile( HWIF_FILT_REF_ADJ_0,  0);
			regproc->SetRegisterFile( HWIF_FILT_REF_ADJ_1,  0);
			regproc->SetRegisterFile( HWIF_FILT_REF_ADJ_2,  0);
			regproc->SetRegisterFile( HWIF_FILT_REF_ADJ_3,  0);
			regproc->SetRegisterFile( HWIF_FILT_MB_ADJ_0,  0);
			regproc->SetRegisterFile( HWIF_FILT_MB_ADJ_1,  0);
			regproc->SetRegisterFile( HWIF_FILT_MB_ADJ_2,  0);
			regproc->SetRegisterFile( HWIF_FILT_MB_ADJ_3,  0);
		}
	}

	/* scan order */
	if (decMode == VP8HWD_VP7)
	{
		int	i;

		for(i = 1; i < 16; i++)
		{
			regproc->SetRegisterFile( ScanTblRegId[i], vp7ScanOrder[i]);
		}
	}

	/* prediction filter taps */
	/* normal 6-tap filters */
	if ((vpVersion & 0x3) == 0 || decMode == VP8HWD_VP7)
	{
		int i, j;

		for(i = 0; i < 8; i++)
		{
			for(j = 0; j < 4; j++)
			{
				regproc->SetRegisterFile( TapRegId[i][j], mcFilter[i][j+1]);
			}
			if (i == 2)
			{
				regproc->SetRegisterFile( HWIF_PRED_TAP_2_M1, mcFilter[i][0]);
				regproc->SetRegisterFile( HWIF_PRED_TAP_2_4, mcFilter[i][5]);
			}
			else if (i == 4)
			{
				regproc->SetRegisterFile( HWIF_PRED_TAP_4_M1, mcFilter[i][0]);
				regproc->SetRegisterFile( HWIF_PRED_TAP_4_4, mcFilter[i][5]);
			}
			else if (i == 6)
			{
				regproc->SetRegisterFile( HWIF_PRED_TAP_6_M1, mcFilter[i][0]);
				regproc->SetRegisterFile( HWIF_PRED_TAP_6_4, mcFilter[i][5]);
			}
		}
	}
	/* TODO: tarviiko tapit bilinear caselle? */

	if (decMode == VP8HWD_VP7)
	{
		regproc->SetRegisterFile( HWIF_INIT_DC_COMP0, dcPred[0]);
		regproc->SetRegisterFile( HWIF_INIT_DC_COMP1, dcPred[1]);
		regproc->SetRegisterFile( HWIF_INIT_DC_MATCH0, dcMatch[0]);
		regproc->SetRegisterFile( HWIF_INIT_DC_MATCH1, dcMatch[1]);
		regproc->SetRegisterFile( HWIF_VP7_VERSION, vpVersion != 0);
	}
	{
		RK_U32 i, tmp;
		RK_U32 hwBitPos;
		RK_U32 tmpAddr;
		RK_U32 tmp2;
		RK_U32 byteOffset;
		RK_U32 extraBytesPacked = 0;

		RK_U32	strmBusAddress=bitstream.phy_addr;
		/* TODO: miksi bitin tarkkuudella (count) kun kuitenki luetaan tavu
		* kerrallaan? Vai lukeeko HW eri lailla? */
		/* start of control partition */
		tmp = (bitstr.pos) * 8 + (8 - bitstr.count);

		if (frameTagSize == 4)
			tmp+=8;

		if (decMode == VP8HWD_VP8 && keyFrame)
			extraBytesPacked += 7;

		tmp += extraBytesPacked*8;

		tmpAddr = tmp/8;
		hwBitPos = (tmpAddr & DEC_8190_ALIGN_MASK) * 8;
		tmpAddr &= (~DEC_8190_ALIGN_MASK);  /* align the base */

		hwBitPos += tmp & 0x7;

        byteOffset = tmpAddr;
        if(!VPUMemJudgeIommu()){
            tmpAddr +=strmBusAddress;
        }else{
            tmpAddr = strmBusAddress|(tmpAddr << 10);
        }

		/* control partition */
        regproc->SetRegisterFile(HWIF_VP8HWPART1_BASE, tmpAddr);
		regproc->SetRegisterFile(HWIF_STRM1_START_BIT, hwBitPos);

		/* total stream length */
		/*tmp = bitstr.streamEndPos - (tmpAddr - strmBusAddress);*/

		/* calculate dct partition length here instead */
		tmp = bitstr.streamEndPos + frameTagSize - dctPartitionOffsets[0];
		tmp += (nbrDctPartitions-1)*3;
		tmp2 = extraBytesPacked + dctPartitionOffsets[0];
		tmp += (tmp2 & 0x7);

		regproc->SetRegisterFile(HWIF_STREAM_LEN, tmp);

		/* control partition length */
		tmp = offsetToDctParts + frameTagSize - (byteOffset - extraBytesPacked);
		if(decMode == VP8HWD_VP7) /* give extra byte for VP7 to pass test cases */
			tmp ++;

		regproc->SetRegisterFile(HWIF_STREAM1_LEN, tmp);

		/* number of dct partitions */
		regproc->SetRegisterFile(HWIF_COEFFS_PART_AM,
		nbrDctPartitions-1);

		/* base addresses and bit offsets of dct partitions */
		for (i = 0; i < nbrDctPartitions; i++)
		{

            if(!VPUMemJudgeIommu()){
                tmpAddr = strmBusAddress + extraBytesPacked + dctPartitionOffsets[i];
			    byteOffset = tmpAddr & 0x7;
    			regproc->SetRegisterFile(DctBaseId[i], tmpAddr & 0xFFFFFFF8);
    			regproc->SetRegisterFile(DctStartBit[i], byteOffset * 8);
            }else{
                tmpAddr = extraBytesPacked + dctPartitionOffsets[i];
			    byteOffset = tmpAddr & 0x7;
                tmpAddr = tmpAddr & 0xFFFFFFF8;
                regproc->SetRegisterFile(DctBaseId[i], strmBusAddress |(tmpAddr << 10));
    			regproc->SetRegisterFile(DctStartBit[i], byteOffset * 8);
            }
		}
	}
	return VP8_Dec_OK;
}

int	vp8decoder::decoder_int()
{
	frame_cnt = 0;
	Init_memed = 0;
    int err;
	decMode = VP8HWD_VP8;

    socket = VPUClientInit(VPU_DEC);
	if (socket >= 0)
        ALOGV("vp8: VPUSendMsg VPU_CMD_REGISTER success\n");
    else
    {
        ALOGV("vp8: VPUSendMsg VPU_CMD_REGISTER failed\n");
		return -1;
    }
    err = VPUMallocLinear(&probTbl, PROB_TABLE_SIZE);
	if(err)
	{
        ALOGV("err = 0x%x\n", err);
        return -1;
    }
    err = VPUMallocLinear(&bitstream, 0x80000);
	if(err)
	{
       ALOGV("err = 0x%x\n", err);
       return -1;
    }
	vpu_pp = new postprocess;
	regproc = new rkregister;
	regproc->SetRegisterMapAddr(regBase);
	vpu_pp->setup_reg(regproc);

	vpu_pp->pp_pipeline_enable();
	vpu_pp->pp_scale_enable();
	vpu_pp->pp_set_bright_adjust(0);
	vpu_pp->pp_set_colorsat_adjust(0);
	vpu_pp->pp_enable();
	decinit_hwcfg();
	frmanager = new framemanager;
	frmanager->init(8);

	return VP8_Dec_OK;
}

int	vp8decoder::decoder_frame(RK_U8 *pbuff, RK_U32 size, VPU_POSTPROCESSING *ppcfg)//input_stream *str)
{
	RK_U8			*pstr;
	RK_U32			i;
	VPU_BITSTREAM	*strinfo = (VPU_BITSTREAM*)pbuff;
	RK_U32			frame_dec_error=0;

	strinfo->SliceLength = swap32bit(strinfo->SliceLength);
	if (size < strinfo->SliceLength+sizeof(VPU_BITSTREAM))
		return -1;
	if (size > bitstream.size+sizeof(VPU_BITSTREAM))
	{
		VPUFreeLinear(&bitstream);
		int err = VPUMallocLinear(&bitstream, (size-sizeof(VPU_BITSTREAM)+1023)&(~1023));
        if(err)
        {
            ALOGE("VPU_mem allco fail \n");
            return -1;
        }
	}
	pbuff += sizeof(VPU_BITSTREAM);
	pstr = (RK_U8*)bitstream.vir_addr;
	memcpy((void*)pstr, (void*)pbuff, (unsigned long)strinfo->SliceLength);
    if(VPUMemClean(&bitstream))
    {
        ALOGE("vp8 VPUMemClean error");
        return -1;
    }
	decoder_frame_header(pstr, strinfo->SliceLength);
   	vp8hwdSetPartitionOffsets(pstr, strinfo->SliceLength);
   	VP8HwdAsicProbUpdate();
   	frame_out = frmanager->get_frame(mb_width*mb_height*384,vpumem_ctx);
    if(!frame_out)
    {
        return -1;
    }
	frame_out->ShowTime = strinfo->SliceTime;
	frame_out->FrameWidth = mb_width<<4;
	frame_out->FrameHeight = mb_height<<4;
	frame_out->OutputWidth = width;
	frame_out->OutputHeight = height;
	frame_out->DisplayWidth = width;
	frame_out->DisplayHeight = height;
	frame_out->ColorType = PP_IN_FORMAT_YUV420SEMI;
	frame_out->DecodeFrmNum = ++frame_cnt;
	frame_out->FrameBusAddr[0] = frame_out->vpumem.phy_addr;
	frame_out->FrameBusAddr[1] = frame_out->FrameBusAddr[0]+((mb_width*mb_height)<<8);
    frame_hwcfg();
    if(VPUMemClean(&probTbl))
    {
        ALOGE("vp8 VPUMemClean error");
        return -1;
    }
	if (ppcfg)
	{

	}
	//vpu_pp->setup_pp(&frame_out, &pp_outframe);
	//vpu_pp->start_pp();
	{


      /*  reg[1] = 0;
		for(i=2;i<100;i++)
			reg[i] = regBase[i];
		reg[1] = 1;
		while((reg[1]&0x1000)==0);*/

        VPU_CMD_TYPE cmd;
        RK_U32  tempreg[100];

        regproc->SetRegisterFile(HWIF_DEC_E, 1);
        {
            RK_U32 *p = (RK_U32*)&regBase[0];
            for(int i = 0; i< VPU_REG_NUM_DEC;i++){
                ALOGV("reg[%d] = 0x%x",i,p[i]);
            }
        }
        VPUClientSendReg(socket, (RK_U32*)&regBase[0], VPU_REG_NUM_DEC);
		{
			RK_S32 len;
			if(VPUClientWaitResult(socket, (RK_U32*)&tempreg[0], VPU_REG_NUM_DEC,
								&cmd, &len))
			{
                 			ALOGV("VPUClientWaitResult error \n");
            			}
			frame_dec_error = tempreg[1];
		}
        /* Rollback entropy probabilities if refresh is not set */
		if(refreshEntropyProbs == 0)
		{
			memcpy((void*)&entropy, (void*)&entropyLast, (unsigned long)sizeof(vp8EntropyProbs_t));
			memcpy((void*)vp7ScanOrder, (void*)vp7PrevScanOrder, (unsigned long)sizeof(vp7ScanOrder));
		}
		if (decMode == VP8HWD_VP7)
		{
			dcPred[0] = regproc->GetRegisterFile(HWIF_INIT_DC_COMP0);
			dcPred[1] = regproc->GetRegisterFile(HWIF_INIT_DC_COMP1);
			dcMatch[0] = regproc->GetRegisterFile(HWIF_INIT_DC_MATCH0);
			dcMatch[1] = regproc->GetRegisterFile(HWIF_INIT_DC_MATCH1);
		}
		if (decMode != VP8HWD_WEBP)
		{
			if (copyBufferToAlternate == 1)
			{
				frmanager->free_frame(frame_alternate);
				frame_alternate = frame_ref;
				frmanager->employ_frame(frame_alternate);
			}
			else if (copyBufferToAlternate == 2)
			{
				frmanager->free_frame(frame_alternate);
				frame_alternate = frame_golden;
				frmanager->employ_frame(frame_alternate);
			}

			if (copyBufferToGolden == 1)
			{
				frmanager->free_frame(frame_golden);
				frame_golden = frame_ref;
				frmanager->employ_frame(frame_golden);
			}
			else if (copyBufferToGolden == 2)
			{
				frmanager->free_frame(frame_golden);
				frame_golden = frame_alternate;
				frmanager->employ_frame(frame_golden);
			}


			if((frame_dec_error&0x1000) == 0x1000 )//if (1)//!corrupted)
			{
                if(refreshGolden)
				{
					frmanager->free_frame(frame_golden);
					frame_golden = frame_out;
					frmanager->employ_frame(frame_golden);
				}

				if(refreshAlternate)
				{
					frmanager->free_frame(frame_alternate);
					frame_alternate = frame_out;
					frmanager->employ_frame(frame_alternate);
				}

				if (refreshLast)
				{
					frmanager->free_frame(frame_ref);
					frame_ref = frame_out;
					frmanager->employ_frame(frame_ref);
				}
			}
            if(showFrame)
			frmanager->push_display(frame_out);
			frmanager->free_frame(frame_out);
		}
		//while(1);
	}
	return VP8_Dec_OK;
}

void	vp8decoder::get_displayframe(VPU_FRAME *frame)
{
	VPU_FRAME	*fr;
	fr = frmanager->get_display();
	if (fr)
	{
		*frame = *fr;
		if(VPUMemDuplicate(&frame->vpumem, &fr->vpumem))
		{
            ALOGE("VPUMemDuplicate fail\n");
        }
		frmanager->free_frame(fr);
	}
	else
		memset(frame,0,sizeof(VPU_FRAME));
}

void vp8decoder::reset_keyframestate()
{
	VPU_FRAME	*fr;
	while(fr = frmanager->get_display())
	{
		frmanager->free_frame(fr);
	}
    if(frame_ref&&frame_ref->vpumem.phy_addr)
	    frmanager->free_frame(frame_ref);
	if (frame_alternate&&frame_alternate->vpumem.phy_addr){
       frmanager->free_frame(frame_alternate);
	}
    if (frame_golden&&frame_golden->vpumem.phy_addr){
       frmanager->free_frame(frame_golden);
	}
    frame_golden = NULL;
	frame_ref = NULL;
	frame_alternate = NULL;
}
