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
///////////////////////////////////////////////


///////////////////////////////////////////////
// Includes
///////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>

///////////////////////////////////////////////
// Globals
///////////////////////////////////////////////

typedef void (__interrupt __far * ivec)();
typedef void (__far * fcall)();
extern "C" ivec gOld10h = NULL;
extern "C" ivec gOld33h = NULL;
extern "C" ivec gOldMouseCallbackInterrupt = NULL;
// defined in solvbeasm.asm
extern "C" fcall gMouseTrap;
// defined in solvbeasm.asm
extern "C" void __far bankswitchproc();

int gVDDHandle;

///////////////////////////////////////////////
// Inline assembly
///////////////////////////////////////////////

// Loads and registers the VDD DLL
int VDDRegisterModule(char * aDLLName, char * aInitRoutine, char * aDispatchRoutine);
#pragma aux VDDRegisterModule=\
"push ds" \
"pop es" \
"db 0C4h, 0C4h, 058h, 0" \
"jnc @done" \
"xor ax,ax" \
"@done:" \
parm[SI][DI][BX] modify [ES] value [AX];

// Unloads the VDD DLL
void VDDUnregisterModule(int aHandle);
#pragma aux VDDUnregisterModule=\
"db 0C4h, 0C4h, 058h, 1" \
parm[AX];

// Makes a call to the VDD. 
// Parameter registers kept to more or less minimum since we need to
// juggle registers a bit.
int VDDDispatchCall(int aHandle, int aWhat, int aData1, int aData2);
#pragma aux VDDDispatchCall=\
"db 0C4h, 0C4h, 058h, 2" \
parm[AX][BX][CX][DX] value[AX];

// End a VDDSimulate16() call.
void VDDUnsimulate16();
#pragma aux VDDUnsimulate16=\
"db 0C4h, 0C4h, 0xFE"

// Helpers to get or set app registers in stack. 
// Mostly useless, as this manipulation is generally performed
// on the VDD side.

void int_setes(int v);
#pragma aux int_setes="mov word ptr [bp+4],ax" parm[ax];

void int_setds(int v);
#pragma aux int_setds="mov word ptr [bp+6],ax" parm[ax];

void int_setdi(int v);
#pragma aux int_setdi="mov word ptr [bp+8],ax" parm[ax];

void int_setsi(int v);
#pragma aux int_setsi="mov word ptr [bp+10],ax" parm[ax];

void int_setbp(int v);
#pragma aux int_setbp="mov word ptr [bp+12],ax" parm[ax];

void int_setsp(int v);
#pragma aux int_setsp="mov word ptr [bp+14],ax" parm[ax];

void int_setbx(int v);
#pragma aux int_setbx="mov word ptr [bp+16],ax" parm[ax];

void int_setdx(int v);
#pragma aux int_setdx="mov word ptr [bp+18],ax" parm[ax];

void int_setcx(int v);
#pragma aux int_setcx="mov word ptr [bp+20],ax" parm[ax];

void int_setax(int v);
#pragma aux int_setax="mov word ptr [bp+22],ax" parm[ax];

int int_getes();
#pragma aux int_getes="mov ax, word ptr [bp+4]" value[ax];

int int_getds();
#pragma aux int_getds="mov ax, word ptr [bp+6]" value[ax];

int int_getdi();
#pragma aux int_getdi="mov ax, word ptr [bp+8]" value[ax];

int int_getsi();
#pragma aux int_getsi="mov ax, word ptr [bp+10]" value[ax];

int int_getbp();
#pragma aux int_getbp="mov ax, word ptr [bp+12]" value[ax];

int int_getsp();
#pragma aux int_getsp="mov ax, word ptr [bp+14]" value[ax];

int int_getbx();
#pragma aux int_getbx="mov ax, word ptr [bp+16]" value[ax];

int int_getdx();
#pragma aux int_getdx="mov ax, word ptr [bp+18]" value[ax];

int int_getcx();
#pragma aux int_getcx="mov ax, word ptr [bp+20]" value[ax];

int int_getax();
#pragma aux int_getax="mov ax, word ptr [bp+22]" value[ax];


///////////////////////////////////////////////
// Interrupt handlers
///////////////////////////////////////////////


void  __interrupt __far videoint(unsigned int ax, unsigned int bx, unsigned int cx)
{
    if (VDDDispatchCall(gVDDHandle,0x10,0,0))
        _chain_intr(gOld10h);
}
#pragma aux videoint parm[ax][bx][cx];

void  __interrupt __far mouseint(unsigned int ax, unsigned int bx, unsigned int cx)
{
    if (VDDDispatchCall(gVDDHandle,0x33,0,0))
        _chain_intr(gOld33h);
}
#pragma aux mouseint parm[ax][bx][cx];

// restore DS, perform callback, restore our DS
void mousecallback();
#pragma aux mousecallback=\
"push ds"                     /* stack: ds          */ \
"push ax"                     /* stack: ds ax       */ \
"mov ax, word ptr [bp+6]"     /* ax=appds           */ \
"push ax"                     /* stack: ds ax appds */ \
"pop ds"                      /* stack: ds ax       */ \
"pop ax"                      /* stack: ds          */ \
"call dword far cs:gMouseTrap"\
"pop ds"                      /* stack empty        */;

void eoi();
#pragma aux eoi=\
"mov al, 20h"\
"out 0A0h, al"\
"out 020h, al"\
modify[al];

void  __interrupt __far mouseirq(unsigned int ax, unsigned int bx, unsigned int cx)
{
    if (VDDDispatchCall(gVDDHandle, 0x74, 0, 0))
    {
        mousecallback();
    }
    eoi();
}
#pragma aux mouseirq parm[ax][bx][cx];

void  __interrupt __far mousecbint(unsigned int ax, unsigned int bx, unsigned int cx)
{
    if (VDDDispatchCall(gVDDHandle, 0x74, 0, 0))
    {
        mousecallback();
    }
    _chain_intr(gOldMouseCallbackInterrupt);
}
#pragma aux mousecbint parm[ax][bx][cx];

///////////////////////////////////////////////
// Main
///////////////////////////////////////////////

void footer();
              // 12345678901234567890123456789012345678901234567890123456789012345678901234567890
char info[80] = "\tSolVBE DOS TSR Version 1.1b, March 10, 2004\n\tCopyright 2004 Jari Komppa\n";
int modetbl[256];

void main(int parc, char ** pars)
{
    setbuf(stdout, NULL);
    printf("SolVBE - VESA VBE for Win32 dos boxes - http://iki.fi/sol/\n\n");
    printf(info);
    printf("\nRegistering VDD..\n\n");
    gVDDHandle = VDDRegisterModule("SolVBE.dll","SolVBEInit","SolVBECall"); 
    if (gVDDHandle == 0)
    {
        cprintf("registerModule failed - is the DLL in the same directory?\n");
        return;
    }
    gMouseTrap = (fcall)&gMouseTrap;
    VDDDispatchCall(gVDDHandle, 0x0100, (int)&info,(int)&modetbl);
    VDDDispatchCall(gVDDHandle, 0x0200, (int)bankswitchproc,(int)&gMouseTrap);

    printf(info);

    int mode = 1;
    int i;
    for (i = 1; i < parc; i++)
        mode = VDDDispatchCall(gVDDHandle, 0x300, FP_SEG(pars[i]), (int)FP_OFF(pars[i]));

    printf("\nSetting video interrupt..\n");
    gOld10h = _dos_getvect(0x10);
    _dos_setvect(0x10,(ivec)videoint);
    printf("Setting mouse interrupt..\n");
    gOld33h = _dos_getvect(0x33);
    _dos_setvect(0x33,(ivec)mouseint);
    if (mode == 1)
    {
        printf("Setting irq12 interrupt (0x74)..\n");
        gOldMouseCallbackInterrupt = _dos_getvect(0x74);
        _dos_setvect(0x74,(ivec)mouseirq);
    }
    if (mode == 2)
    {
        printf("Setting irq12 interrupt (0x74)..\n");
        gOldMouseCallbackInterrupt = _dos_getvect(0x74);
        _dos_setvect(0x74,(ivec)mousecbint);
    }
    if (mode == 3)
    {
        printf("Setting irq0 interrupt (0x08)..\n");
        gOldMouseCallbackInterrupt = _dos_getvect(0x8);
        _dos_setvect(0x8,(ivec)mousecbint);
    }
    printf("Going resident..\n");
    _dos_keep(0, (FP_OFF(footer)+15 + 2048 + 8192) >> 4); // TODO: figure out the correct way to calculate this.
}

void footer() {}
