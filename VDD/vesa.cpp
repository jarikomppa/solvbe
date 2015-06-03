///////////////////////////////////////////////
//
// SolVBE
// 
// "universal" VESA VBE for windows dos boxes
//
// Copyright 2004 Jari Komppa 
// http://iki.fi/sol
//
///////////////////////////////////////////////
// License
///////////////////////////////////////////////
// 
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
// 
// (ie. same as ZLIB license)
// 
///////////////////////////////////////////////
// Note
///////////////////////////////////////////////
//
// If you make any interesting additions / fixes, please send me patches,
// so that there's one "good" version of SolVBE.
//
// In order to build, SolVBE requires:
// - OpenWatcom 1.2 for DOS-side code
// - VC6 and Win2k DDK (other DDKs might also work)
//
// Some of the code might seem rather confusing. gRegs[AX] will alter the
// registers from host application (say, terra nova: strike force centauri),
// while setAX(n) alters the registers for the dos-side code of SolVBE.
//
///////////////////////////////////////////////
//
// This file:
// - VESA calls
//
///////////////////////////////////////////////

#include "SolVBE.h"

MODEINFO modeinfo[] = {
    // OEM modes
    {0x0003,   80,   25,  160,  1},
    {0x0004,  320,  200,  160,  4}, 
    {0x0005,  320,  200,  160,  4}, 
    {0x0009,  320,  200,  160,  4}, 
    {0x000a,  640,  200,  320,  4}, 
    {0x000d,  320,  200,  160,  4}, 
    {0x000e,  640,  200,  320,  4}, 
    {0x0010,  640,  350,  320,  4}, 
    {0x0012,  640,  480,  320,  4}, 
    {0x0013,  320,  200,  320,  8},
     // VBE 1.0+
    {0x0100,  640,  400,  640,  8},
    {0x0101,  640,  480,  640,  8},
    {0x0102,  800,  600,  400,  4},
    {0x0103,  800,  600,  800,  8},
    {0x0104, 1024,  768,  512,  4},
    {0x0105, 1024,  768, 1024,  8},
    {0x0106, 1280, 1024,  640,  4},
    {0x0107, 1280, 1024, 1280,  8},
    {0x0108,   80,   60,  160,  1},
    {0x0109,  132,   25,  264,  1},
    {0x010A,  132,   43,  264,  1},
    {0x010B,  132,   50,  264,  1},
    {0x010C,  132,   60,  264,  1},
    {0x81FF,    1,    1,    1,  8}, // special full-memory access mode
    // VBE 1.2+
    {0x010D,  320,  200,  640, 15},
    {0x010E,  320,  200,  640, 16},
    {0x010F,  320,  200,  960, 24},
    {0x0110,  640,  480, 1280, 15},
    {0x0111,  640,  480, 1280, 16},
    {0x0112,  640,  480, 1920, 24},
    {0x0113,  800,  600, 1600, 15},
    {0x0114,  800,  600, 1600, 16},
    {0x0115,  800,  600, 2400, 24},
    {0x0116, 1024,  768, 2048, 15},
    {0x0117, 1024,  768, 2048, 16},
    {0x0118, 1024,  768, 3072, 24},
    {0x0119, 1280, 1024, 2560, 15},
    {0x011A, 1280, 1024, 2560, 16},
    {0x011B, 1280, 1024, 3840, 24},
    // VBE 2.0+
    {0x0120, 1600, 1200, 1600, 8},
    {0x0121, 1600, 1200, 3200, 15},
    {0x0122, 1600, 1200, 3200, 16},
    // additional modes
    {0x0123, 1600, 1200, 4800, 24},
    
    {0x0124,  320,  400,  320,  8},
    {0x0125,  320,  400,  640, 15},
    {0x0126,  320,  400,  640, 16},
    {0x0127,  320,  400,  960, 24},

    {0x0128,  320,  200, 1280, 32},
    {0x0129,  320,  240, 1280, 32},
    {0x012A,  320,  400, 1280, 32},
    {0x012B,  640,  400, 2560, 32},
    {0x012C,  640,  480, 2560, 32},
    {0x012D,  800,  600, 3200, 32},
    {0x012E, 1024,  768, 4096, 32},
    {0x012F, 1280, 1024, 5120, 32},
    {0x0130, 1600, 1200, 6400, 32},
    {0xffff,    0,    0,    0,  0}
};

///////////////////////////////////////////////
// VESA info functions
///////////////////////////////////////////////

void vesainfo()
{
/*
     Input:    AX   = 4F00h  Return VBE Controller Information
            ES:DI   =        Pointer to buffer in which to place
                             VbeInfoBlock structure
                             (VbeSignature should be set to 'VBE2' when
                             function is called to indicate VBE 2.0
                             information is desired and the information
                             block is 512 bytes in size.)

     Output:   AX   =         VBE Return Status  
*/
    void * ptr = (void *)VdmMapFlat(gRegs[ES],gRegs[DI],VDM_V86);
    VESAINFO * vi = (VESAINFO *)ptr;

    
    int vbe2 = 0;
    if (vi->Signature == '2EBV') vbe2 = 1; // application wants VBE2 info
    vi->Signature = 'ASEV'; 
    vi->Version[0] = 2;
    vi->Version[1] = 1;
    vi->OEMNameSegment = gRMDataSeg;
    vi->OEMNameOffset = gRMInfoOfs;
    vi->Capabilities = 1 | 2; // switchable DAC, vga compatible
    vi->SupportedModesSegment = gRMDataSeg;
    vi->SupportedModesOffset = gRMModeOfs;
    vi->TotalMemory = VIDMEM_KB / 64; 
    if (vbe2)
    {
        vi->OemSoftwareRev = 0x0100;
        // protected mode pointers:
        vi->OemVendorNamePtr = 0; 
        vi->OemProductNamePtr = 0;
        vi->OemProductRevPtr = 0;
    }

    VdmUnmapFlat(gRegs[ES], gRegs[DI], ptr, VDM_V86);

    unsigned short * modeptr = (unsigned short *)VdmMapFlat(gRMDataSeg,gRMModeOfs,VDM_V86);
    modeptr[0] = 0xffff;

    int i = 0;
    while (modeinfo[i].Modenumber != 0xffff)
    {
        modeptr[i] = modeinfo[i].Modenumber;
        i++;
    }
    modeptr[i] = 0xffff;

    VdmUnmapFlat(gRMDataSeg,gRMModeOfs,modeptr,VDM_V86);

    gRegs[AX] = 0x004f; // call successful
}

void vesamodeinfo()
{
/*
    Input:
        AX = 4F01h
        CX = SuperVGA video mode (see #04082 for bitfields)
        ES:DI -> 256-byte buffer for mode information (see #00079)

    Return:
        AL = 4Fh if function supported
        AH = status
             00h successful
                 ES:DI buffer filled
             01h failed
*/
    int mode = gRegs[CX];
    int modenumber = mode & 0x1ff;
    
    // illegal flags..
    if (
        //(mode & (1 << 15)) == 1 || // preserve display memory on mode change
        (mode & (1 << 14)) == 1 || // use LFB (not supported yet)
        (mode & (1 << 13)) == 1 || // VBE/AF initialize hardware
        //(mode & (1 << 12)) == 1 || // reserved for VBE/AF
        //(mode & (1 << 11)) == 1 || // VBE 3.0 use custom refresh rates
        0)
    {
        gRegs[AX] = 0x014F;
        return;
    }

    // find mode in mode info list..
    int mi = 0;
    while (modeinfo[mi].Modenumber != 0xffff && modeinfo[mi].Modenumber != modenumber) mi++;
    
    if (modeinfo[mi].Modenumber == 0xffff)
    {
        // mode not found
        gRegs[AX] = 0x014F;
        return;
    }

    // text modes not supported..
    if (modeinfo[mi].Bits == 1)
    {
        gRegs[AX] = 0x014F;
        return;
    }

    void * ptr = (void *)VdmMapFlat(gRegs[ES],gRegs[DI],VDM_V86);
    VESAMODEDATA * vi = (VESAMODEDATA *)ptr;

    vi->ModeAttributes = 1 | 2 | 8 | 16; 
        // + D0 = Mode supported in hardware (1=yes)
        // + D1 = 1 (Reserved)
        // - D2 = Output functions supported by BIOS (1=yes)
        // + D3 = Monochrome/color mode (1=color)
        // + D4 = Mode type (0=text, 1=graphics)
        // + D5 = VGA compatible mode (0=yes)
        // + D6 = VGA compatible windowed mode available (0=yes)
        // - D7 = Linear framebuffer available (1=yes)
        // - D8-D15 = Reserved 
    vi->WindowAAttributes = 1 | 2 | 4;
    vi->WindowBAttributes = 0; //1 | 2 | 4;
        // + D0 = Relocatable windows supported (0=single non-reloc win only)
        // + D1 = Window readable  (0=not readable)
        // + D2 = Window writeable (0=not writeable);
        // - D3-D7 = Reserved 
    vi->WindowGranularity = 64;
    vi->WindowSize = 64;
    vi->StartSegmentWindowA = 0xa000;
    vi->StartSegmentWindowB = 0;
    vi->BankSwitchFunctionOffset = gRMBankSwitchOfs;
    vi->BankSwitchFunctionSegment = gRMCodeSeg;
    vi->BytesPerScanLine = modeinfo[mi].Pitch;
    vi->PixelWidth = modeinfo[mi].Width;
    vi->PixelHeight = modeinfo[mi].Height;
    vi->CharacterCellPixelWidth = 8;
    vi->CharacterCellPixelHeight = 8;
    vi->NumberOfMemoryPlanes = 1; //(modeinfo[mi].Bits == 4) ? 4 : 1;
    vi->BitsPerPixel = modeinfo[mi].Bits;
    vi->NumberOfBanks = 1;//VIDMEM_KB / 64;
    vi->MemoryModelType = (modeinfo[mi].Bits == 4) ? 4 : (modeinfo[mi].Bits == 8) ? 4 : 6;
        //   00h =           Text mode
        //   01h =           CGA graphics
        //   02h =           Hercules graphics
        //   03h =           4-plane planar
        // + 04h =           Packed pixel
        //   05h =           Non-chain 4, 256 color
        // + 06h =           Direct Color
        //   07h =           YUV
        //   08h-0Fh =       Reserved, to be defined by VESA
        //   10h-FFh =       To be defined by OEM 
    vi->SizeOfBank = 0;
    vi->NumberOfImagePages = (VIDMEM_KB * 1024 / (modeinfo[mi].Height * modeinfo[mi].Pitch)) - 1;
    vi->Reserved1 = 0; // 1 for vbe 3.0
    
    // VBE 1.2+
    
    if (modeinfo[mi].Bits == 15)
    {
        vi->RedMaskSize = 5;
        vi->RedFieldPosition = 10;
        vi->GreenMaskSize = 5;
        vi->GreenFieldPosition = 5;
        vi->BlueMaskSize = 5;
        vi->BlueFieldPosition = 0;
        vi->ReservedMaskSize = 1;
        vi->ReservedFieldPosition = 15;
    }
    else
    if (modeinfo[mi].Bits == 16)
    {
        vi->RedMaskSize = 5;
        vi->RedFieldPosition = 11;
        vi->GreenMaskSize = 6;
        vi->GreenFieldPosition = 5;
        vi->BlueMaskSize = 5;
        vi->BlueFieldPosition = 0;
        vi->ReservedMaskSize = 0;
        vi->ReservedFieldPosition = 0;
    }
    else
    if (modeinfo[mi].Bits == 24)
    {
        vi->RedMaskSize = 8;
        vi->RedFieldPosition = 16;
        vi->GreenMaskSize = 8;
        vi->GreenFieldPosition = 8;
        vi->BlueMaskSize = 8;
        vi->BlueFieldPosition = 0;
        vi->ReservedMaskSize = 0;
        vi->ReservedFieldPosition = 0;
    }
    else
    if (modeinfo[mi].Bits == 32)
    {
        vi->RedMaskSize = 8;
        vi->RedFieldPosition = 16;
        vi->GreenMaskSize = 8;
        vi->GreenFieldPosition = 8;
        vi->BlueMaskSize = 8;
        vi->BlueFieldPosition = 0;
        vi->ReservedMaskSize = 8;
        vi->ReservedFieldPosition = 24;
    }
    vi->DirectColourModeInfo = 2;
        // - D0 = Color ramp is fixed/programmable (0=fixed)
        // + D1 = Bits in Rsvd field are usable/reserved (0=reserved) 
    
    // VBE 2.0+ 
    
    vi->PhysBasePtr = 0;
    vi->OffScreenMemOffset = 0;
    vi->OffScreenMemSize = vi->NumberOfImagePages * (modeinfo[mi].Height * modeinfo[mi].Pitch) / 1024;

    VdmUnmapFlat(gRegs[ES], gRegs[DI], ptr, VDM_V86);

    gRegs[AX] = 0x004F; // success

    return;
}

void vesasetmode()
{
/*
        AX = 4F02h
        BX = new video mode (see #04082,#00083,#00084)
        ES:DI -> (VBE 3.0+) CRTC information block, bit mode bit 11 set
        (see #04083)
 Return:AL = 4Fh if function supported
        AH = status
        00h successful
        01h failed
*/
    int mode = gRegs[BX];
    int modenumber = mode & 0x1ff;
    
    // illegal flags..
    if (
        //(mode & (1 << 15)) == 1 || // preserve display memory on mode change
        (mode & (1 << 14)) == 1 || // use LFB (not supported yet)
        (mode & (1 << 13)) == 1 || // VBE/AF initialize hardware
        //(mode & (1 << 12)) == 1 || // reserved for VBE/AF
        //(mode & (1 << 11)) == 1 || // VBE 3.0 use custom refresh rates
        0)
    {
        gRegs[AX] = 0x014F;
        return;
    }

    // find mode in mode info list..
    int mi = 0;
    while (modeinfo[mi].Modenumber != 0xffff && modeinfo[mi].Modenumber != modenumber) mi++;
    
    if (modeinfo[mi].Modenumber == 0xffff)
    {
        // mode not found
        gRegs[AX] = 0x014F;
        return;
    }

    // text modes not supported..
    if (modeinfo[mi].Bits == 1)
    {
        gRegs[AX] = 0x014F;
        return;
    }
    
    EnterCriticalSection(&gThreadBusy);
    deinitVideoMemory();
    deinitFrameBuffer();
    int bufmode;
    switch (modeinfo[mi].Bits)
    {
        case 4:
            bufmode = MODE_4BPAL;
            break;
        case 8:
            bufmode = MODE_8BPAL;
            break;
        case 15:
            bufmode = MODE_15B;
            break;
        case 16:
            bufmode = MODE_16B;
            break;
        case 24:
            bufmode = MODE_24B;
            break;
        case 32:
            bufmode = MODE_32B;
            break;
    }
    initVideoMemory(modeinfo[mi].Width, bufmode, (mode & (1 << 15)) == 0);
    initFrameBuffer(modeinfo[mi].Width,modeinfo[mi].Height);
    LeaveCriticalSection(&gThreadBusy);
    


    gRegs[AX] = 0x004F; // success


    // Reset all sorts of values..

    gPalWidth = 6; // palette width always resets to 6bit

    gMouseMinX = 0;
    gMouseMaxX = modeinfo[mi].Width;
    gMouseMinY = 0;
    gMouseMaxY = modeinfo[mi].Height;
    gSeqReg04h = (2+4+8);
    gCRTCReg17h = (128+64+32+8+4+2+1);
    gCRTCReg14h = (64 + 32);
    gCRTCReg09h = (128 + 64 + 32);

    gSeqAddress = 0;
    gCRTCAddress = 0;
    gTweakedMode = 0;
    gVidmemReadOnlyState = 0;
    
    if (gCurrentMode == 3)
    {
        capturePorts();
        SetForegroundWindow(gWindowHandle);
        SetFocus(gWindowHandle);
        setWindowSize(640, 480);
        ShowWindow(gWindowHandle, SW_SHOW);
    }

    gCurrentMode = modeinfo[mi].Modenumber;
}

void vesagetmode()
{
/*
        AX = 4F03h

 Return:AL = 4Fh if function supported
        AH = status
             00h successful
             01h failed
        BX = video mode (see #00083,#00084)
             bit 13:VBE/AF v1.0P accelerated video mode
             bit 14:Linear frame buffer enabled (VBE v2.0+)
             bit 15:Don't clear video memory
*/

    gRegs[AX] = 0x004F; // success
    gRegs[BX] = gCurrentMode;
}

void vesamemorycontrol()
{
/*
     Input:    AX   = 4F05h   VBE Display Window Control
               BH   = 00h          Set memory window
                    = 01h          Get memory window
               BL   =         Window number
                    = 00h          Window A
                    = 01h          Window B
               DX   =         Window number in video memory in window
                              granularity units  (Set Memory Window only)

     Output:   AX   =         VBE Return Status
               DX   =         Window number in window granularity units
                                   (Get Memory Window only)
*/
    // TODO: care about a/b window

    if (gRegs[BX] & 0xff00)
    {
        gRegs[DX] = gBankOffset >> 16;
        return;
    }
/*
    else
    {
        if (gRegs[DX] & 0xff > (VIDMEM_KB / 64))
        {
            gRegs[AX] = 0x014F; // failure: attempt to switch bank outside memory
            return;
        }
    }
*/    
    // update memory before switch..
    EnterCriticalSection(&gThreadBusy);

    if ((gBankOffset + 0x20000) < (VIDMEM_KB * 1024))
        memcpy(gVideoMemoryPtr + gBankOffset, gRMA000Ptr, 0x10000);

    gRegs[AX] = 0x004F; // success

//    gBankOffset = ((int)gRegs[DX] & 0xff) << 16;
    gBankOffset = (gRegs[DX] % (VIDMEM_KB / 64)) << 16;

    if (!gOptionWriteOnlyVidmem)
    {
        // update memory after switch..    
        if ((gBankOffset + 0x20000) < (VIDMEM_KB * 1024))
            memcpy(gRMA000Ptr, gVideoMemoryPtr + gBankOffset, 0x10000);
    }

    LeaveCriticalSection(&gThreadBusy);


}

void vesasetscanlinelength()
{
/*
     Input:    AX   = 4F06h   VBE Set/Get Logical Scan Line Length
               BL   = 00h          Set Scan Line Length in Pixels
                    = 01h          Get Scan Line Length
                    = 02h          Set Scan Line Length in Bytes
                    = 03h          Get Maximum Scan Line Length
               CX   =         If BL=00h  Desired Width in Pixels
                              If BL=02h  Desired Width in Bytes
                              (Ignored for Get Functions)

     Output:   AX   =         VBE Return Status
               BX   =         Bytes Per Scan Line
               CX   =         Actual Pixels Per Scan Line
                              (truncated to nearest complete pixel)
               DX   =         Maximum Number of Scan Lines
*/
    if ((gRegs[BX] & 0xff) > 1)
    {
        gRegs[AX] = 0x014F; // failure
        return;
    }
    if ((gRegs[BX] & 0xff) == 0)
    {
        // set mode
        if (gRegs[CX] < 320)
        {
            gRegs[AX] = 0x014F; // failure
            return;
        }
        EnterCriticalSection(&gThreadBusy);
        
        deinitVideoMemory();
        initVideoMemory(gRegs[CX], gSurfaceMode,0);

        LeaveCriticalSection(&gThreadBusy);
        // tbuf is left alone
    }
    if ((gRegs[BX] & 0xff) == 1)
    {
        // get mode
        gRegs[CX] = gVideoBufferWidth;
    }

    gRegs[AX] = 0x004F; // success
    gRegs[BX] = gRegs[CX];
    switch (gSurfaceMode)
    {
    case MODE_4BPAL:
        gRegs[BX] /= 2;
        break;
    case MODE_15B:
    case MODE_16B:
        gRegs[BX] *= 2;
        break;
    case MODE_24B:
        gRegs[BX] *= 3;
        break;
    case MODE_32B:
        gRegs[BX] *= 4;
        break;
    }
    gRegs[DX] = gVideoBufferHeight;

}

void vesasetdisplaystart()
{
/*
     Input:    AX   = 4F07h   VBE Set/Get Display Start Control
               BH   = 00h          Reserved and must be 00h
               BL   = 00h          Set Display Start
                    = 01h          Get Display Start
                    = 80h          Set Display Start during Vertical Retrace
               CX   =         First Displayed Pixel In Scan Line
                              (Set Display Start only)
               DX   =         First Displayed Scan Line (Set Display Start only)

     Output:   AX   =         VBE Return Status
               BH   =         00h Reserved and will be 0 (Get Display Start only)
               CX   =         First Displayed Pixel In Scan Line (Get Display
                              Start only)
               DX   =         First Displayed Scan Line (Get Display
                              Start only)
*/
    gRegs[AX] = 0x004F; // success
    if (gRegs[BX] & 1)
    {
        gRegs[CX] = gDisplayXOffset;
        gRegs[DX] = gDisplayYOffset;
    }
    else
    {
        gDisplayXOffset = gRegs[CX];
        gDisplayYOffset = gRegs[DX];
    }
}

void vesadaccontrol()
{
/*
     Input:    AX   = 4F08h   VBE Set/Get Palette Format
               BL   = 00h          Set DAC Palette Format
                    = 01h          Get DAC Palette Format
               BH   =         Desired bits of color per primary
                              (Set DAC Palette Format only)

     Output:   AX   =         VBE Return Status
               BH   =         Current number of bits of color per primary
*/
    if (gRegs[BX] & 1)
    {
        int bits = (gRegs[BX] >> 8);
        if (bits == 8 ||
            bits == 6)
        {
            gPalWidth = bits;
            gRegs[AX] = 0x004F; // success
        }
        else
        {
            gRegs[AX] = 0x014F; // failed (wrong bit width)
        }
    }
    else
    {
        gRegs[AX] = 0x004F; // success
        gRegs[BX] = (gRegs[BX] & 0xff) | (gPalWidth << 8);
    }
}

void vesasetpalette()
{
/*
        AX = 4F09h
        BL = subfunction
             00h set (primary) palette
             01h get (primary) palette
             02h set secondary palette data
             03h get secondary palette data
             80h set palette during vertical retrace
        CX = number of entries to change
        DX = starting palette index
        ES:DI -> palette buffer, array of DAC entries (see #00086)
 Return:AL = 4Fh if function supported
        AH = status
        00h successful
        01h failed
*/
    // TODO
}
