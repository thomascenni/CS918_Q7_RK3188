/*
 * Copyright (C) 2016 Rockchip Electronics Co.Ltd
 * Authors:
 *      Zhiqin Wei <wzq@rock-chips.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
 
#ifndef _rockchip_rga_process_h_
#define _rockchip_rga_process_h_


#include <fcntl.h>
#include <string.h>

//#include <utils/Log.h>

/*hardware/libhardware/include/hardware/rga.h*/
#include <rga.h>

#define UNUSED(x) (void)(x)

int rgaGetBytesPerPixel(int format);
#endif
