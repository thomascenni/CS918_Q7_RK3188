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

/*
@func rgaScaleAndRotate

@param rot: rotate orientation 2:x mirror;3:y mirror;0:copy;90 180 270

@param src: the source address of data
@param dst: the target address of data

@param sx,sy: source offset,please set to zero in this func
@param sw,sh: source width and source height
@param ss   : source stride,common to sw(source width)
@param sf   : source format

@param dx,dy: target offset,please set to zero in this func
@param dw,dh: target width and target height
@param ds   : target stride,common to sw(target width)
@param df   : target format
*/

int rgaScaleAndRotate(int rot, void* src, void* dst,
                            int sx, int sy, int sw, int sh, int ss, int sf,
                            int dx, int dy, int dw, int dh, int ds, int df);

/*
@func rgaLargeScale

@param src: the source address of data
@param dst: the target address of data

@param sx,sy: source offset,please set to zero in this func
@param sw,sh: source width and source height
@param ss   : source stride,common to sw(source width)
@param sf   : source format

@param dx,dy: target offset,please set to zero in this func
@param dw,dh: target width and source height
@param ds   : target stride,common to sw(source width)
@param df   : target format
*/
int rgaLargeScale(void* src, void* dst,
                            int sx, int sy, int sw, int sh, int ss, int sf,
                            int dx, int dy, int dw, int dh, int ds, int df);
#endif
