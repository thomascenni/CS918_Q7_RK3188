#ifndef _BITSTREAM_H_
#define _BITSTREAM_H_

#include "vpu_global.h"

class bitstream
{

public:
    bitstream();
    ~bitstream();

    void    init(RK_U8 *vir_addr, RK_U32 phy_addr, RK_U32 size);
    RK_U32  skipbits(RK_U32 bits);
    RK_S32  leftbit();
    void    bytealign();
    RK_S32  testslice(RK_U32 bytes_addr, RK_S32 vop);
    RK_U32  showbits(RK_U32 bits);
    RK_S32  getslice();
    RK_U32  getbits(RK_U32 bits);

    /***************************************************************************************************
    	Func:
    		getreadbits
    	Description:
    		get the consumed bits
    	Author:
    		Jian Huan
    	Date:
    		2010-12-3 10:26:08
     **************************************************************************************************/
    RK_U32 getreadbits()
    {
        return readbits;
    }
    /***************************************************************************************************
    	Func:
    		getbaseviraddr
    	Description:
    		get base virtual address of bitstream buffer
    	Author:
    		Jian Huan
    	Date:
    		2010-12-3 10:26:08
     **************************************************************************************************/
    RK_U8* getbaseviraddr()
    {
    	return strbase;
    }

    /***************************************************************************************************
    	Func:
    		getbasephyaddr
    	Description:
    		get base physical address of bitstream buffer
    	Author:
    		Jian Huan
    	Date:
    		2010-12-3 10:26:08
     **************************************************************************************************/
    RK_U32 getbasephyaddr()
    {
    	return phybase;
    }

    /***************************************************************************************************
    	Func:
    		getchunksize
    	Description:
    		get buffer size
    	Author:
    		Jian Huan
    	Date:
    		2010-12-3 10:32:16
     **************************************************************************************************/
    RK_U32 getchunksize()
    {
    	return chunksize;
    }
    /***************************************************************************************************
        Func:
            getslicetime
        Description:
            get the current slice time
        Author:
            Jian Huan
        Date:
            2010-12-3 16:32:33
     **************************************************************************************************/
    RK_U32 getslicetime()
    {
        return vpuBits.SliceTime.TimeLow;
    }
    /***************************************************************************************************
        Func:
            getframeID
        Description:
            get the current frame ID
        Author:
            Jian Huan
        Date:
            2010-12-3 16:59:34
     **************************************************************************************************/
    RK_U32 getframeID()
    {
        return vpuBits.SliceNum;
    }
    /***************************************************************************************************
        Func:
            getoffset8
       Author:
            Jian Huan
        Date:
            2010-12-4 16:05:51
     **************************************************************************************************/        
    RK_U32 getoffset8()
    {
        return offset8;
    }

private:
    RK_U8   *strbase;           //base virtual address of bitstream buffer
    RK_U32  phybase;            //base physical address of bitstream buffer
    RK_U8   *curstr;            //current address in bitstream buffer
    RK_U32  chunksize;          //
    RK_U32  pos;                //offset in byte
    RK_S32  leftbits;           //the rest bits of current buffer
    RK_S32  readbits;           //the consumed bits of current buffer
    RK_U32  offset8;            //MPEG-4 debug only

    VPU_BITSTREAM   vpuBits;
};

#endif  /*_BITSTREAM_H_*/
