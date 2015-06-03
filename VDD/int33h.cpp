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
// - int33h handler
// - other mouse-related functions
//
///////////////////////////////////////////////

#include "SolVBE.h"

void triggerMouseIRQ()
{
    if (gOptionMouseIRQCall)
        VDDSimulateInterrupt(ICA_SLAVE, 12 - 8, 1);
}

void captureMouse()
{
    SetCapture(gWindowHandle);
    ShowCursor(FALSE);
    gMouseCaptured = 1;
    RECT rect;
    GetWindowRect(gWindowHandle,&rect);
    int centerx = (rect.left+rect.right) / 2;
    int centery = (rect.top+rect.bottom) / 2;
    SetCursorPos(centerx, centery);
}

void releaseMouse()
{
    ReleaseCapture();
    ShowCursor(TRUE);
    gMouseCaptured = 0;
    if (gMouseButtons & 1)
        gMouseEventNeeded |= MOUSE_LBUP;
    if (gMouseButtons & 2)
        gMouseEventNeeded |= MOUSE_RBUP;
    gMouseButtons = 0;

    if (gFullscreen)
    {
        initWindow(0);
    }
}

void mousesetmotioncallback()
{
/*
    INT 33 - MS MOUSE v1.0+ - DEFINE INTERRUPT SUBROUTINE PARAMETERS
	    AX = 000Ch
	    CX = call mask (see #03171)
	    ES:DX -> FAR routine (see #03172)
    SeeAlso: AX=0018h

    Bitfields for mouse call mask:
    Bit(s)	Description	(Table 03171)
     0	call if mouse moves
     1	call if left button pressed
     2	call if left button released
     3	call if right button pressed
     4	call if right button released
     5	call if middle button pressed (Mouse Systems/Logitech/Genius mouse)
     6	call if middle button released (Mouse Systems/Logitech/Genius mouse)
     7-15	unused
    Note:	some versions of the Microsoft documentation incorrectly state that CX
	      bit 0 means call if mouse cursor moves
*/
    unsigned short * fcall = (unsigned short *)VdmMapFlat(gRMCodeSeg, gRMMouseTrapOfs, VDM_V86);

    fcall[0] = gRegs[DX]; // SHOULD be in this order
    fcall[1] = gRegs[ES]; 

    gMouseEventMask = gRegs[CX];

    VdmUnmapFlat(gRMCodeSeg, gRMMouseTrapOfs, fcall, VDM_V86);
}


void domouseevent()
{
/*
(Table 03172)
Values interrupt routine is called with:
	AX = condition mask (same bit assignments as call mask)
	BX = button state
	CX = cursor column
	DX = cursor row
	SI = horizontal mickey count
	DI = vertical mickey count
*/

    setAX(0); // By default, don't call mouse handler.
    
    if (gMouseEventNeeded & gMouseEventMask)
    {
        if (gMouseEventMask != 0)
        {
            setAX(gMouseEventNeeded & gMouseEventMask);
            setBX(gMouseButtons);
            setCX(gMouseX);
            setDX(gMouseY);
/*
            setSI(gMickeyX); // here's hoping these aren't needed
            setDI(gMickeyY);            
            gMickeyX = 0;
            gMickeyY = 0;
*/
        }
    }
    if (!gOptionConstantMouseIRQs)
    {
        gMouseEventNeeded = 0;
    }
}

void handle_int33()
{
    log_tick();
    log("INT33h "
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
    setAX(0); // By default, don't call mouse handler.
  
    if (gRegs[AX] == 0x0000)
    {
        log("mouse init\r\n");
        // install check
        setAX(0); // don't chain interrupt
        gRegs[AX] = 0xffff;
        gRegs[BX] = 2; // let's say we have two buttons for now
        return;
    }
    
    if (gRegs[AX] == 0x0003)
    {
        log("mouse getpos\r\n");
        // return position and status
        setAX(0); // don't chain interrupt
        gRegs[BX] = gMouseButtons;
        gRegs[CX] = gMouseX;
        gRegs[DX] = gMouseY;
        return;
    }
    if (gRegs[AX] == 0x0004)
    {
        log("mouse setpos\r\n");
        // set mouse cursor position
        setAX(0); // don't chain interrupt
        gMouseX = gRegs[CX];
        gMouseY = gRegs[DX];
        return;
    }
    
    if (gRegs[AX] == 0x0005)
    {
        // mouse press data
        setAX(0); // don't chain interrupt
        log("mouse release data\r\n");
        int b = 0;
        if (gRegs[BX] == 1)
        {
            b = 1;
        }
        else
        {
            b = 0;
        }
        if (gMouseClickCount[b] == 0)
        {
            gRegs[BX] = 0;
            gRegs[CX] = gMouseX;
            gRegs[DX] = gMouseX;
            return;
        }
        gRegs[BX] = gMouseClickCount[b];
        gRegs[CX] = gMouseClickX[b];
        gRegs[DX] = gMouseClickY[b];
        gMouseClickCount[b] = 0;
        return;

    }
    if (gRegs[AX] == 0x0006)
    {
        // mouse release data
        setAX(0); // don't chain interrupt
        log("mouse release data\r\n");
        int b = 0;
        if (gRegs[BX] == 1)
        {
            b = 1;
        }
        else
        {
            b = 0;
        }
        if (gMouseReleaseCount[b] == 0)
        {
            gRegs[BX] = 0;
            gRegs[CX] = gMouseX;
            gRegs[DX] = gMouseX;
            return;
        }
        gRegs[BX] = gMouseReleaseCount[b];
        gRegs[CX] = gMouseReleaseX[b];
        gRegs[DX] = gMouseReleaseY[b];
        gMouseReleaseCount[b] = 0;
        return;
    }
    
    if (gRegs[AX] == 0x0007)
    {
        log("mouse horiz range\r\n");
        // mouse horizontal range
        setAX(0); // don't chain interrupt
        gMouseMinX = gRegs[CX];
        gMouseMaxX = gRegs[DX];
        if (gMouseX < gMouseMinX)
            gMouseX = gMouseMinX;
        if (gMouseX > gMouseMaxX)
            gMouseX = gMouseMaxX;
        return;
    }
    if (gRegs[AX] == 0x0008)
    {
        log("mouse vert range\r\n");
        // mouse vertical range
        setAX(0); // don't chain interrupt
        gMouseMinY = gRegs[CX];
        gMouseMaxY = gRegs[DX];
        if (gMouseY < gMouseMinY)
            gMouseY = gMouseMinY;
        if (gMouseY > gMouseMaxY)
            gMouseY = gMouseMaxY;
        return;
    }

    if (gRegs[AX] == 0x000b)
    {
        log("mouse read motion counters\r\n");
        // read mouse motion counters
        setAX(0); // don't chain interrupt
        gRegs[CX] = gMickeyX;
        gRegs[DX] = gMickeyY;
        gMickeyX = 0;
        gMickeyY = 0;
        return;
    }

    if (gRegs[AX] == 0x000c)
    {
        log("mouse callback setup\r\n");
        // set up mouse motion callback
        setAX(0); // don't chain interrupt
        mousesetmotioncallback();
        return;
    }
    if (gRegs[AX] == 0x000f)
    {
        log("mouse set accuracy\r\n");
        // set mouse accuracy
        setAX(0); // don't chain interrupt
        gMouseXSpeed = gRegs[CX]; // motion * 8 / speed
        gMouseYSpeed = gRegs[DX]; // motion * 8 / speed
        return;
    }
/*
    if (gRegs[AX] == 0x0010)
    {
        log("mouse define screen region\r\n");
        // define screen region
        setAX(0); // don't chain interrupt
        // TODO
    }
    */
    if (gRegs[AX] == 0x0015)
    {
        log("mouse driver storage req\r\n");
        // request driver state storage requirements
        setAX(0); // don't chain interrupt
        gRegs[BX] = 180; // 180B is what cutemouse needs
        return;
    }

    if (gRegs[AX] == 0x0021)
    {
        log("mouse soft reset\r\n");
        // mouse soft reset
        setAX(0); // don't chain interrupt
        gRegs[AX] = 0xffff; // mouse installed
        gRegs[BX] = 0xffff; // 2 buttons
        return;
    }

    if (gRegs[AX] == 0x0024 && gRegs[BX] == 0)
    {
        log("mouse detect (microsoft)\r\n");
        // mouse soft reset
        setAX(0); // don't chain interrupt
        gRegs[CX] = 0x0400; // type / irq (ps2)
        gRegs[BX] = 0x0700; // version 
        return;
    }

  // Interrupts to block..

    if (
        gRegs[AX] == 0x0001 || // hide mouse cursor
        gRegs[AX] == 0x0002 || // show mouse cursor
//        gRegs[AX] == 0x0009 || // define graphical cursor
//        gRegs[AX] == 0x000a || // define text cursor
        gRegs[AX] == 0x000d || // light pen emulation on
        gRegs[AX] == 0x000e || // light pen emulation off
        gRegs[AX] == 0x0013 || // define mouse acceleration
        0)
    {
        setAX(0); // don't chain interrupt
        log("Blocked\r\n");
        return;
    }
    
/*
    // Interrupts to pass through..

    if (
        gRegs[AX] == 0x0000            || // bar
        0)
    {
        setAX(1); // do chain interrupt
        log("Chained\r\n");
        return;
    }
*/
	if (gOptionDebugLevel >= DEBUG_EXTLOG)
	{
		char temp[1024];
		sprintf(temp,"Debug call - unknown int33h\n"
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
}
