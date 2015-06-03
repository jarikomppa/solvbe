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
// - int10h handler
// - other VGA-related functions
//
///////////////////////////////////////////////

#include "SolVBE.h"


VDD_IO_PORTRANGE gPortDefs[12] = 
{
    {0x3c7, 0x3c9}, // color regs
    {0x3ce, 0x3cf}, // graphics regs
    {0x3c4, 0x3c5}, // sequencer regs
    {0x3c0, 0x3c1}, // attribute regs
    {0x3b4, 0x3b5}, // crtc regs (monochrome)
    {0x3d4, 0x3d5}, // crtc regs (color)
    {0x3cc, 0x3cc}, // general regs
    {0x3c2, 0x3c2}, // general regs
    {0x3ca, 0x3ca}, // general regs
    {0x3ba, 0x3ba}, // general regs (monochrome)
    {0x3da, 0x3da}, // general regs
};

VDD_IO_PORTRANGE gSimplePortDefs[1] = 
{
    {0x3c7, 0x3c9} // color regs
};

void vgareg_inb(WORD iport, BYTE * data);
void vgareg_outb(WORD iport, BYTE data);


VDD_IO_HANDLERS gPortHandlers[1] = 
{
    {vgareg_inb, NULL, NULL, NULL, vgareg_outb, NULL, NULL, NULL}
};


void copyfromtweak(int plane)
{
    int i, p;
    p = plane;
    for (i = 0; i < TWEAK_COPY_BYTES; i++)
    {
        gVideoMemoryPtr[p] = gRMA000Ptr[i];
        p += 4;
    }
}

void copytotweak(int plane)
{
    int i, p;
    p = plane;
    for (i = 0; i < TWEAK_COPY_BYTES; i++)
    {
        gRMA000Ptr[i] = gVideoMemoryPtr[p];
        p += 4;
    }    
}

void capturePorts()
{
    if (!gPortsCaptured)
    {
        log_tick();
        log("Capturing VGA ports\r\n");
        // Attach inb & outb functions to correct ports
        if (gOptionSimpleVGARegUse)
            VDDInstallIOHook(gDLLHandle, 1, gSimplePortDefs, gPortHandlers);
        else
            VDDInstallIOHook(gDLLHandle, 12, gPortDefs, gPortHandlers);
        gPortsCaptured = 1;
    }
}

void releasePorts()
{
    if (gPortsCaptured)
    {
        log_tick();
        log("Releasing VGA ports\r\n");
        // Free IO ports
        if (gOptionSimpleVGARegUse)
            VDDDeInstallIOHook(gDLLHandle, 1, gSimplePortDefs);
        else
            VDDDeInstallIOHook(gDLLHandle, 12, gPortDefs);
        gPortsCaptured = 0;
    }
}


void vgareg_inb(WORD iport, BYTE * data) 
{
    if (!gPortsCaptured) return;
    if (iport == 0x3c7 ||
        iport == 0x3c8 ||
        iport == 0x3c9)
    {
        if (gLogmode != LOG_PALETTE)
        {
            log_tick();
            log("Port 0x%x: Palette events",iport);
            gLogmode = LOG_PALETTE;
            gLogmodestart = tick();
        }

        if (iport == 0x3c7)
        {
            // read-only
            *data = gPalIndex / 3;
            return;
        }

        if (iport == 0x3c8)
        {
            *data = gPalIndex / 3;
            return;
        }

        int pi = gPalIndex / 3;
        int pf = (2 - (gPalIndex % 3)) * 8;
    
        *data = ((gPalette[pi] >> pf) & 0xff) >> (8 - gPalWidth);
    
        gPalIndex++;
        if (gPalIndex >= 768)
            gPalIndex = 0;
        return;
    }

    if (iport == 0x3da || iport == 0x3ba)
    {
        // retrace check
        if (gLogmode != LOG_VRC)
        {
            log_tick();
            log("Port 0x%x: Waiting for retrace",iport);
            gLogmode = LOG_VRC;
            gLogmodestart = tick();
        }
        int vr;
        vr = gVRC;
        static int hr = 1; hr = 1 - hr;
        if (vr) hr = 1;
        *data = (vr << 3) | (hr << 0);
        return;
    }
    if (iport == 0x3d5 || iport == 0x3b5)
    {
        log_tick();
        log("Port 0x%x: Read CRTC register (%x)",iport,gCRTCAddress);
        if (gCRTCAddress == 0x17)
        {
            log(" mode control register\r\n");
            *data = gCRTCReg17h;
            return;
        }
        if (gCRTCAddress == 0x09)
        {
            log(" max scanline register\r\n");
            *data = gCRTCReg09h;
            return;
        }
        if (gCRTCAddress == 0x14)
        {
            log(" underline location register\r\n");
            *data = gCRTCReg14h;
            return;
        }
        log(" unknown\r\n");
        *data = 0;
        return;
    }

    log_tick();
    log("Port 0x%x: inb ",iport);
    
    switch(iport)
    {
    case 0x3c7:
        log("(Palette read / status reg)\r\n");
        break;
    case 0x3ce:
        log("(Graphics address reg)\r\n");
        break;
    case 0x3cf:
        log("(Graphics data reg)\r\n");
        break;
    case 0x3c4:
        log("(Sequencer address reg)\r\n");
        break;
    case 0x3c5:
        log("(Sequencer data reg)\r\n");
        break;
    case 0x3c0:
        log("(Attribute address reg)\r\n");
        break;
    case 0x3c1:
        log("(Attribute data reg)\r\n");
        break;
    case 0x3b4:
        log("(CRTC monochrome address reg)\r\n");
        break;
    case 0x3b5:
        log("(CRTC monochrome data reg)\r\n");
        break;
    case 0x3d4:
        log("(CRTC color address reg)\r\n");
        break;
    case 0x3d5:
        log("(CRTC color data reg)\r\n");
        break;
    case 0x3cc:
        log("(General regs)\r\n");
        break;
    case 0x3c2:
        log("(General regs)\r\n");
        break;
    case 0x3ca:
        log("(General regs)\r\n");
        break;
    case 0x3ba:
        log("(General regs (monochrome))\r\n");
        break;
    case 0x3da:
        log("(General regs (color))\r\n");
        break;
    default:
        log("(unknown)\r\n");
    }
    *data = 0;
}

void vgareg_outb(WORD iport, BYTE data) 
{
    if (!gPortsCaptured) return;
    if (iport == 0x3c7 ||
        iport == 0x3c8 ||
        iport == 0x3c9)
    {
        if (gLogmode != LOG_PALETTE)
        {
            log_tick();
            log("Port 0x%x: Palette events",iport);
            gLogmode = LOG_PALETTE;
            gLogmodestart = tick();
        }

        if (iport == 0x3c8)
        {
            // palette index for writing
            gPalIndex = data * 3;
            return;
        }

        if (iport == 0x3c7)
        {
            // palette index for reading
            gPalIndex = data * 3;
            return;
        }
        
        int pi = gPalIndex / 3;
        int pf = (2 - (gPalIndex % 3)) * 8;

        gPalette[pi] = (gPalette[pi] & ~(0xff << pf)) |
                                 (data << (8 - gPalWidth + pf));

        gPalIndex++;
        if (gPalIndex >= 768)
            gPalIndex = 0;
        return;
    }
    if (iport == 0x3c4) 
    {
        log_tick();
        log("Port 0x%x: Sequencer address (0x%x)\r\n",iport,data);
        gSeqAddress = data;
        return;
    }
    if (iport == 0x3d4 || iport == 0x3b4) 
    {
        log_tick();
        log("Port 0x%x: CRT Controller address (0x%x)\r\n",iport,data);
        gCRTCAddress = data;
        return;
    }
    if (iport == 0x3ce) 
    {
        log_tick();
        log("Port 0x%x: Graphics address (0x%x)\r\n",iport,data);
        gGraphicsAddress = data;
        return;
    }
    if (iport == 0x3c5)
    {
        log_tick();
        log("Port 0x%x: Sequencer data (addr 0x%x, data 0x%x)",iport, gSeqAddress, data);
        if (gSeqAddress == 4)
        {
            gSeqReg04h = data;
            if ((data & 8) == 0) // unchained mode
            {
                gTweakedMode = 1;
                log(" unchained mode (considering mode tweaked from now on)\r\n");
            }
            else
            {
                log(" unknown effect\r\n");
            }
            return;
        }
        if (gSeqAddress == 2)
        {
            log(" map mask register\r\n");
            EnterCriticalSection(&gThreadBusy);

            if (gOptionReadOnlyVidmem && gVidmemReadOnlyState)
            {
                gVidmemReadOnlyState = 0;
            }
            else
            {
                int i;
                for (i = 0; i < 4; i++)
                    if (gSeqReg02h & (1 << i))
                        copyfromtweak(i);
            }
            
            gSeqReg02h = data;

            if (!gOptionWriteOnlyVidmem)
            {
                int i;
                for (i = 0; i < 4; i++)
                    if (gSeqReg02h & (1 << i))
                        copytotweak(i);
            }
            
            LeaveCriticalSection(&gThreadBusy);
            return;
        }
        log(" unknown.");
        return;
    }
    if (iport == 0x3d5 || iport == 0x3b5)
    {
        log_tick();
        log("Port 0x%x: CRTC data (addr 0x%x, data 0x%x)",iport, gCRTCAddress, data);
        if (gCRTCAddress == 0x17)
        {
            gCRTCReg17h = data;
            log(" mode control register\r\n");
            return;
        }
        if (gCRTCAddress == 0x14)
        {
            gCRTCReg14h = data;
            log(" underline location register\r\n");
            return;
        }
        if (gCRTCAddress == 0x09)
        {
            gCRTCReg09h = data;            
            if ((gCRTCReg09h & 128) == 0)
            {
                // 400 line mode
                EnterCriticalSection(&gThreadBusy);

                deinitFrameBuffer();
                initFrameBuffer(320, 400);
                
                LeaveCriticalSection(&gThreadBusy);

                log(" max scanline register - 400 scans\r\n");
            }
            else
            {
                // 200 line mode
                EnterCriticalSection(&gThreadBusy);

                deinitFrameBuffer();
                initFrameBuffer(320, 200);

                LeaveCriticalSection(&gThreadBusy);
                
                log(" max scanline register - 200 scans\r\n");
            }
            return;
        }
        log(" unknown.");
        return;
    }
    if (iport == 0x3cf)
    {
        log_tick();
        log("Port 0x%x: graphics data (addr 0x%x, data 0x%x)",iport, gGraphicsAddress, data);
        if (gGraphicsAddress == 0x4)
        {
            gGraphicsReg04h = data;
            log(" layer read register\r\n");
            copytotweak(gGraphicsReg04h);
            if (gOptionReadOnlyVidmem)
                gVidmemReadOnlyState = 1;
            return;
        }
        log(" unknown.");
        return;
    }

    log_tick();
    log("Port 0x%x: outb(0x%02x) ",iport,data);
    switch(iport)
    {
    case 0x3c7:
        log("(Palette read / status reg)\r\n");
        break;
    case 0x3ce:
        log("(Graphics address reg)\r\n");
        break;
    case 0x3cf:
        log("(Graphics data reg)\r\n");
        break;
    case 0x3c4:
        log("(Sequencer address reg)\r\n");
        break;
    case 0x3c5:
        log("(Sequencer data reg)\r\n");
        break;
    case 0x3c0:
        log("(Attribute address reg)\r\n");
        break;
    case 0x3c1:
        log("(Attribute data reg)\r\n");
        break;
    case 0x3b4:
        log("(CRTC monochrome address reg)\r\n");
        break;
    case 0x3b5:
        log("(CRTC monochrome data reg)\r\n");
        break;
    case 0x3d4:
        log("(CRTC color address reg)\r\n");
        break;
    case 0x3d5:
        log("(CRTC color data reg)\r\n");
        break;
    case 0x3cc:
        log("(General regs)\r\n");
        break;
    case 0x3c2:
        log("(General regs)\r\n");
        break;
    case 0x3ca:
        log("(General regs)\r\n");
        break;
    case 0x3ba:
        log("(General regs (monochrome))\r\n");
        break;
    case 0x3da:
        log("(General regs (color))\r\n");
        break;
    default:
        log("(unknown)\r\n");
    }
}


void handle_int10()
{

    // Check for text output (different logging)
    if (
        (gRegs[AX] & 0xff00) == 0x0e00 || // teletype output
        (gRegs[AX] & 0xff00) == 0x0900 || // write -- "" --
        (gRegs[AX] & 0xff00) == 0x0a00 || // write char only
        (gRegs[AX] & 0xff00) == 0x0200 || // set cursor position
        0)
    {
        if (gLogmode != LOG_TEXTOUTPUT)
        {
            log_tick();
            log("Outputting text in console");
            gLogmode = LOG_TEXTOUTPUT;
            gLogmodestart = tick();
        }
        setAX(1); // do chain interrupt
        return;
    }

    if (gRegs[AX] == 0x4f05)
    {
        if (gLogmode != LOG_VESABANKS)
        {
            log_tick();
            log("Switching VESA banks");
            gLogmode = LOG_VESABANKS;
            gLogmodestart = tick();
        }
        setAX(0);
        vesamemorycontrol();
        return;
    }


    log_tick();
    log("INT10h "
         "ax:%04x "
         "bx:%04x "
         "cx:%04x "
         "dx:%04x "
         "es:%04x "
         "di:%04x "
         "ds:%04x "
         "si:%04x "
         "sp:%04x "
         "bp:%04x ",
         gRegs[AX],
         gRegs[BX],
         gRegs[CX],
         gRegs[DX],
         gRegs[ES],
         gRegs[DI],
         gRegs[DS],
         gRegs[SI],
         gRegs[SP],
         gRegs[BP] 
         );
    
    if (gRegs[AX] == 0x4f00)
    {
        log("vesainfo\r\n");
        setAX(0);
        vesainfo();
        return;
    }

    if (gRegs[AX] == 0x4f01)
    {
        log("vesamodeinfo\r\n");
        setAX(0);
        vesamodeinfo();
        return;
    }

    if ( // these should be enough text modes for now..
        gRegs[AX] == 0x0000 ||
        gRegs[AX] == 0x0001 ||
        gRegs[AX] == 0x0002 ||
        gRegs[AX] == 0x0003 ||
        gRegs[AX] == 0x0007 ||
        gRegs[AX] == 0x0008 ||
        gRegs[AX] == 0x0014 ||
        gRegs[AX] == 0x0017 ||
        gRegs[AX] == 0x0018 ||
        gRegs[AX] == 0x0019 ||
        gRegs[AX] == 0x001a ||
        gRegs[AX] == 0x001b ||
        gRegs[AX] == 0x001c ||
        gRegs[AX] == 0x001d ||
        gRegs[AX] == 0x001e ||
        gRegs[AX] == 0x001f ||
        gRegs[AX] == 0x0020 ||
        gRegs[AX] == 0x0021 ||
        gRegs[AX] == 0x0022 ||
        gRegs[AX] == 0x0023 ||
        gRegs[AX] == 0x0024 ||
        gRegs[AX] == 0x0025 ||
        gRegs[AX] == 0x0026 ||
        gRegs[AX] == 0x0027 ||
        gRegs[AX] == 0x0028 ||
        gRegs[AX] == 0x002a ||
        gRegs[AX] == 0x002f ||
        gRegs[AX] == 0x0032 ||
        gRegs[AX] == 0x0037 ||
        gRegs[AX] == 0x0040 ||
        gRegs[AX] == 0x0041 ||
        gRegs[AX] == 0x0042 ||
        gRegs[AX] == 0x0043 ||
        gRegs[AX] == 0x0045 ||
        gRegs[AX] == 0x0046 ||
        gRegs[AX] == 0x0047 ||
        gRegs[AX] == 0x0048 ||
        gRegs[AX] == 0x0049 ||
        gRegs[AX] == 0x004d ||
        gRegs[AX] == 0x004e ||
        gRegs[AX] == 0x004f ||
        gRegs[AX] == 0x0050 ||
        gRegs[AX] == 0x0051 ||
        gRegs[AX] == 0x0052 ||
        gRegs[AX] == 0x0053 ||
        gRegs[AX] == 0x0054 ||
        gRegs[AX] == 0x0055 ||
        gRegs[AX] == 0x0056 ||
        gRegs[AX] == 0x0057 ||
        gRegs[AX] == 0x0058 ||
        gRegs[AX] == 0x0059 ||
        gRegs[AX] == 0x005a ||
        gRegs[AX] == 0x005b ||
        gRegs[AX] == 0x005c ||
        gRegs[AX] == 0x005d ||
        0)
    {
        log("set text mode\r\n");
        setAX(0);
        releasePorts();
        gCurrentMode = 3;
        setWindowSize(128,128);
        deinitFrameBuffer();
        initFrameBuffer(128,128);
        gRegs[AX] = (gRegs[AX] & 0xff00) | 0x30;
        if (gConsoleWindow)
        {
            SetForegroundWindow(gConsoleWindow);
            SetFocus(gConsoleWindow);
        }
        return;
    }

    if (gRegs[AX] == 0x0013)
    {
        log("set mcga mode\r\n");
        setAX(0);
        int bxtemp = gRegs[BX];
        int axtemp = gRegs[AX];
        gRegs[BX] = 0x13;
        vesasetmode();
        gRegs[BX] = bxtemp;
        gRegs[AX] = (axtemp & 0xff00) | 0x20;
        return;
    }
    if (gRegs[AX] == 0x0012)
    {
        log("set vga mode\r\n");
        setAX(0);
        int bxtemp = gRegs[BX];
        int axtemp = gRegs[AX];
        gRegs[BX] = 0x12;
        vesasetmode();
        gRegs[BX] = bxtemp;
        gRegs[AX] = (axtemp & 0xff00) | 0x20;
        return;
    }

    if (gRegs[AX] == 0x4f02)
    {
        log("set vesa mode\r\n");
        setAX(0);
        vesasetmode();
        return;
    }
    if (gRegs[AX] == 0x4f03)
    {
        log("get vesa mode\r\n");
        setAX(0);
        vesagetmode();
        return;
    }
    if (gRegs[AX] == 0x4f06)
    {
        log("set scanline length\r\n");
        setAX(0);
        vesasetscanlinelength();
        return;
    }
    if (gRegs[AX] == 0x4f07)
    {
        log("set display start\r\n");
        setAX(0);
        vesasetdisplaystart();
        return;
    }
    if (gRegs[AX] == 0x4f08)
    {
        log("DAC width\r\n");
        setAX(0);
        vesadaccontrol();
        return;
    }
    
    if (gRegs[AX] == 0x4f09)
    {
        log("vesa set palette\r\n");
        setAX(1); // not implemented yet; pass through
        vesasetpalette();
        return;
    }

    if ((gRegs[AX] & 0xff00) == 0x0f00)
    {
        // get current video mode
        log("vga get video mode\r\n");
        setAX(0);
        gRegs[AX] = 0x5000 | (gCurrentMode & 0xff);
        gRegs[BX] = gRegs[BX] & 0xff;
        return;
    }

    if (gRegs[AX] == 0x101a)
    {
        log("vga get DAC mode\r\n");
        // get DAC mode (practically NOP)
        setAX(0);
        gRegs[BX] = 0;
        return;
    }

    if (gRegs[AX] == 0x1a00)
    {
        log("vga detect\r\n");
        // vga detect
        setAX(0);
        gRegs[AX] = 0x1a;
        gRegs[BX] = 8;
        return;
    }

    if (gRegs[AX] == 0x1010)
    {
        log("vga set single palette entry\r\n");
        // set pal
        setAX(1); // not done yet, pass through
        // TODO (ofs = bx, ch = g, cl = b, dh = r)
        return;
    }
    
    if (gRegs[AX] == 0x1012)
    {
        log("vga set palette\r\n");
        // set pal
        setAX(1); // not done yet, pass through
        // TODO (ofs = bx, count = cx, target = es:dx, 6bit byte triplets)
        return;
    }
    

    if (gRegs[AX] == 0x1017)
    {
        log("vga get palette\r\n");
        // read pal
        setAX(0); // not done yet, block
        // TODO (ofs = bx, count = cx, target = es:dx, 6bit byte triplets)
        return;
    }

    // Interrupts to block..

    if (
        gRegs[AX] == 0x1013 || // set dac mode 
        gRegs[AX] == 0x1000 || // set single pal register (not palette!)
        gRegs[AX] == 0x1001 || // set overscan (border) color
        gRegs[AX] == 0x1002 || // set pal registers (not palette!)
        gRegs[AX] == 0x1009 || // read pal + overscan
		gRegs[AX] == 0x6F00 || // extended bios installation check
        (gRegs[AX] & 0xff00) == 0x1200 || // get blanking attribute / video bios specific extensions
        // VESA calls to block..
        gRegs[AX] == 0x4F0A || // get protected mode access functions
        gRegs[AX] == 0x4F0B || // VESA VBE/AF
        gRegs[AX] == 0x4F10 || // VESA VBE/PM
        gRegs[AX] == 0x4F11 || // VESA VBE/FP
        gRegs[AX] == 0x4F12 || // VESA VBE/CI
        gRegs[AX] == 0x4F13 || // VESA VBE/AI
        gRegs[AX] == 0x4F14 || // VESA VBE OEM extensions
        gRegs[AX] == 0x4F15 || // VESA VBE/DC
        gRegs[AX] == 0x4F16 || // VESA VBE/GC
        gRegs[AX] == 0x4F17 || // VESA VBE/AF
        0)
    {
        setAX(0); // don't chain interrupt
        log("Blocked\r\n");
        return;
    }

    // Interrupts to pass through..

    if (
        (gRegs[AX] & 0xff00) == 0x0100 || // set text mode cursor shape
        (gRegs[AX] & 0xff00) == 0x0300 || // get cursor position and size
        (gRegs[AX] & 0xff00) == 0x0400 || // read light pen position / get cursor address
        (gRegs[AX] & 0xff00) == 0x0500 || // select active display page
        (gRegs[AX] & 0xff00) == 0x0600 || // scroll up window
        (gRegs[AX] & 0xff00) == 0x0700 || // scroll down window
        (gRegs[AX] & 0xff00) == 0x0800 || // read char and attrib at cursor pos
        (gRegs[AX] & 0xff00) == 0x0b00 || // set bg/border color, set palette, set palette entry (we'll trap these through ports)
        //(gRegs[AX] & 0xff00) == 0x0c00 || // write graphics pixel
        //(gRegs[AX] & 0xff00) == 0x0d00 || // read - "" -
        gRegs[AX] == 0x1130            || // font info, used by vbetest
        gRegs[AX] == 0x8f00            || // unknown interrupt, thrown by vbetest - likely to be univbe internal..
        0)
    {
        setAX(1); // do chain interrupt
        log("Chained\r\n");
        return;
    }

	if (gOptionDebugLevel >= DEBUG_EXTLOG)	
	{
		char temp[1024];
		sprintf(temp,"Debug call - unknown int10h\n"
					 "ax:%04x\n"
					 "bx:%04x\n"
					 "cx:%04x\n"
					 "dx:%04x\n"
					 "es:%04x\n"
					 "di:%04x\n"
					 "ds:%04x\n"
					 "si:%04x\n"
					 "sp:%04x\n"
					 "bp:%04x\n",
					 gRegs[AX],
					 gRegs[BX],
					 gRegs[CX],
					 gRegs[DX],
					 gRegs[ES],
					 gRegs[DI],
					 gRegs[DS],
					 gRegs[SI],
					 gRegs[SP],
					 gRegs[BP] 
					 );
		MessageBox(NULL,temp,"SolVBE Debug Call",MB_OK);
	}

	if (gOptionDebugLevel >= DEBUG_LOG)
	    log("Unknown\r\n");

    if ((gRegs[AX] & 0xff00) == 0x4f00)
        setAX(0); // don't chain VESA interrupts
}
