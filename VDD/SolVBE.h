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

///////////////////////////////////////////////
// System includes
///////////////////////////////////////////////

#define i386
#include "windows.h"
#include <gl\gl.h>
#include <gl\glu.h>

extern "C" 
{
#include <vddsvc.h>
}

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "resource.h"

///////////////////////////////////////////////
// Defines
///////////////////////////////////////////////

// How much video memory should there be?
#define VIDMEM_KB 2048
// How many bytes to copy from/to tweaked mode
#define TWEAK_COPY_BYTES (320 * 240 / 2)

///////////////////////////////////////////////
// Structures
///////////////////////////////////////////////

#pragma pack(1)
typedef struct 
{
    unsigned long int  Signature;      // "VESA" (0x41534556="ASEV") 
    char               Version[2];     // [0]=major, [1]=minor version. (0200=2.0)
    unsigned short int OEMNameOffset;
    unsigned short int OEMNameSegment; // RM-Pointer to OEM name 
    unsigned long int  Capabilities;
      // D0      = DAC is switchable  (0=fixed at 6b)
      // D1      = 0=vga compatible, 1=NOT vga compatible
      // D2      = 0=Normal RAMDAC, 1=*slow* RAMDAC (only change pal in vrc)
      // D3-31   = Reserved   
    unsigned short int SupportedModesOffset;  // RM Pointer to list of supported 
    unsigned short int SupportedModesSegment; // modes (terminated with 0xffff).
    unsigned short int TotalMemory; // in 64k blocks 
    // ---- vbe 2.0 ---- 
    unsigned short int OemSoftwareRev;  // OEM version 
    unsigned long int OemVendorNamePtr; // PM pointer to asciiz string
    unsigned long int OemProductNamePtr;// -"-
    unsigned long int OemProductRevPtr; // -"-
    char               Reserved1[222];  // OEM scatchpad
    char               Reserved2[256];
} VESAINFO;

typedef struct {
    unsigned short int       ModeAttributes;
        // D0 = Mode supported in hardware (1=yes)
        // D1 = 1 (Reserved)
        // D2 = Output functions supported by BIOS (1=yes)
        // D3 = Monochrome/color mode (1=color)
        // D4 = Mode type (0=text, 1=graphics)
        // D5 = VGA compatible mode (0=yes)
        // D6 = VGA compatible windowed mode available (0=yes)
        // D7 = Linear framebuffer available (1=yes)
        // D8-D15 = Reserved 
    char                     WindowAAttributes;
    char                     WindowBAttributes;
        // D0 = Relocatable windows supported (0=single non-reloc win only)
        // D1 = Window readable  (0=not readable)
        // D2 = Window writeable (0=not writeable);
        // D3-D7 = Reserved 
    unsigned short int       WindowGranularity;
    unsigned short int       WindowSize;
    unsigned short int       StartSegmentWindowA;
    unsigned short int       StartSegmentWindowB;
    unsigned short int       BankSwitchFunctionOffset;
    unsigned short int       BankSwitchFunctionSegment;
    unsigned short int       BytesPerScanLine;
    unsigned short int       PixelWidth;
    unsigned short int       PixelHeight;
    char                     CharacterCellPixelWidth;
    char                     CharacterCellPixelHeight;
    char                     NumberOfMemoryPlanes;
    char                     BitsPerPixel;
    char                     NumberOfBanks;
    char                     MemoryModelType;
        // 00h =           Text mode
        // 01h =           CGA graphics
        // 02h =           Hercules graphics
        // 03h =           4-plane planar
        // 04h =           Packed pixel
        // 05h =           Non-chain 4, 256 color
        // 06h =           Direct Color
        // 07h =           YUV
        // 08h-0Fh =       Reserved, to be defined by VESA
        // 10h-FFh =       To be defined by OEM 
    char                     SizeOfBank;
    char                     NumberOfImagePages;
    char                     Reserved1;
    char                     RedMaskSize;
    char                     RedFieldPosition;
    char                     GreenMaskSize;
    char                     GreenFieldPosition;
    char                     BlueMaskSize;
    char                     BlueFieldPosition;
    char                     ReservedMaskSize;
    char                     ReservedFieldPosition;
    char                     DirectColourModeInfo;
        // D0 = Color ramp is fixed/programmable (0=fixed)
        // D1 = Bits in Rsvd field are usable/reserved (0=reserved) 
    // VBE 2.0+ 
    long int                 PhysBasePtr;        // Physical address for flat frame buffer 
    long int                 OffScreenMemOffset; // PM pointer to start of off-screen memory. 
    unsigned short int       OffScreenMemSize;   // Ammount of off-screen memory in 1k units 
    char                     Reserved2[206];
} VESAMODEDATA;

#pragma pack()

typedef struct
{
    int Modenumber;
    int Width;
    int Height;
    int Pitch;
    int Bits;
} MODEINFO;


///////////////////////////////////////////////
// Enumerations
///////////////////////////////////////////////

// Used with gRegs.
enum STACKREGS
{
    AX = 11,
    BX = 8,
    CX = 10,
    DX = 9,
    ES = 2,
    DI = 4,
    DS = 3,
    SI = 5,
    SP = 7,
    BP = 6
};

// Mouse event flags.
enum MOUSEEVENTS
{
    MOUSE_MOVE   = 1,
    MOUSE_LBDOWN = 2,
    MOUSE_LBUP   = 4,
    MOUSE_RBDOWN = 8,
    MOUSE_RBUP   = 16
};

// video buffer modes    
enum VIDEOMODES
{
    MODE_4BPAL,
    MODE_8BPAL,
    MODE_15B,
    MODE_16B,
    MODE_24B,
    MODE_32B
};

enum LOGMODE
{
    LOG_NORMAL     = 0, // write all events
    LOG_PALETTE    = 1, // we're writing palette events
    LOG_TEXTOUTPUT = 2, // we're writing text on screen
    LOG_VESABANKS  = 3, // we're switching banks
    LOG_VRC        = 4, // we're waiting for retrace
};

enum LOGLEVEL
{
	DEBUG_NONE	= 0, // Ignore all noncritical errors, log nothing
	DEBUG_WARN  = 1, // Display warnings
	DEBUG_LOG   = 2, // Display warnings and write log
	DEBUG_EXTLOG= 3, // Display warnings and write extended log
};

///////////////////////////////////////////////
// Globals
///////////////////////////////////////////////

extern HANDLE gDLLHandle;             // DLL handle
extern HWND   gWindowHandle;          // Display window
extern HWND   gConsoleWindow;         // Console handle (for keyboard events)
extern HDC    gWindowDC;              // Window device context
extern HGLRC  gOpenGLRC;              // OpenGL rendering context
extern GLuint gTextureHandle;         // OpenGL texture handle

extern HDC            gVideoMemoryDC;    // Video memory buffer handle
extern HBITMAP        gVideoMemoryOldBM; // Old bitmap / video memory
extern HBITMAP        gVideoMemoryBM;    // Current video memory bitmap
extern unsigned char *gVideoMemoryPtr;   // Pointer to the video memory

extern HDC     gFrameBufferDC;        // Framebuffer buffer handle
extern HBITMAP gFrameBufferOldBM;     // Old bitmap handle / fb
extern HBITMAP gFrameBufferBM;        // Framebuffer bitmap
extern int    *gFrameBufferPtr;       // Pointer to the framebuffer

extern int gLogmode;                  // Log mode; don't log duplicate events
extern int gLogmodestart;             // Start tick for current log mode

extern volatile int gThreadAlive;     // Flag that keeps display update thread alive
extern volatile int gModeChanging;    // Flag that keeps SolVBE from shutting down when changing modes
extern int gLastVRC;                  // Last vertical retrace tick
extern int gLastRefresh;              // Last display refresh
extern volatile int gVRC;             // Flag: are we inside vertical retrace?
extern int gStartTick;                // Start tick for this SolVBE instance
extern CRITICAL_SECTION gThreadBusy;  // Rendering thread synchronization mutex
extern HANDLE gThreadHandle;          // Display update thread

extern int gVideoBufferWidth;         // Current video buffer width
extern int gVideoBufferPitch;         // Current pitch
extern int gVideoBufferHeight;        // Current video buffer height
extern int gScreenWidth;              // Current screen x resolution
extern int gScreenHeight;             // Current screen y resolution
extern int gWindowWidth;              // Current window x resolution
extern int gWindowHeight;             // Current window y resolution
extern int gTextureWidth;             // Current texture x resolution
extern int gTextureHeight;            // Current texture y resolution

// Explanation on the above:
// video buffer - The contents of video memory. This is most likely
//                something like 320x6553. Games may change the display
//                offset for page flipping effects.
// pitch        - How many bytes we need to add to the pointer to get
//                to the next scanline. Depends on pixel formats.
// Screen       - The visible area of the above.
// Window       - The window resolution. May be anything, and image is
//                stretched to the resolution.
// Texture      - Textures usually have to be in powers-of-two, so for
//                a screen sized 320x200 we need a 512x256 texture.

extern int gBankOffset;               // Current bank offset
extern int gDisplayXOffset;           // Current display x-offset
extern int gDisplayYOffset;           // Current display y-offset
extern int gSurfaceMode;              // Current surface mode. Needed for setscanlinelength call.

extern int gLinearFiltering;          // Flag: linear filtering
extern int gLockAspect;               // Flag: aspect ratio locked
extern int gFullscreen;               // Flag: fullscreen mode
extern int gPortsCaptured;            // Flag: are VGA registers captured?

extern int gPalette[];                // Current palette (4/8b modes)
extern unsigned int gPalIndex;        // Current palette index (inp/outp)
extern unsigned int gPalWidth;        // Current palette width (6 or 8 bit)

extern int gSeqReg04h;                // Sequencer register 4h (bit 8 = chain-4)
extern int gSeqReg02h;                // Sequencer register 2h (layer write mask)
extern int gCRTCReg17h;               // CRTC register 17h (bit 7 = byte addressing)
extern int gCRTCReg14h;               // CRTC register 14h (bit 6 = dword addresses)
extern int gCRTCReg09h;               // CRTC register 9h (bit 8 = scan doubling)
extern int gGraphicsReg04h;           // Graphics register 4h (read layer)

extern int gSeqAddress;               // Active sequencer register
extern int gCRTCAddress;              // Active CRTC address
extern int gGraphicsAddress;          // Active graphics register

extern int gTweakedMode;              // Flag: tweaked mode

extern int gCurrentMode;              // Current video mode (3 = text)

extern int gMouseEventNeeded;         // Bit flags on what kinds of things have happened since last event.
extern int gMouseCaptured;            // Flag: is the mouse captured?
extern int gMouseButtons;             // Currently depressed mouse buttons
extern int gMouseX;                   // Current mouse x coordinate
extern int gMouseY;                   // Current mouse y coordinate
extern int gMouseMinX;                // Mouse min. x coordinate
extern int gMouseMaxX;                // Mouse max. x coordinate
extern int gMouseMinY;                // Mouse min. y coordinate
extern int gMouseMaxY;                // Mouse max. y coordinate
extern int gMouseXSpeed;              // Mouse x speed (mickeys per pixel; lower = faster)
extern int gMouseYSpeed;              // Mouse y speed (mickeys per pixel; lower = faster)
extern int gMouseEventMask;           // Mouse event call mask

extern int gMouseClickCount[];        // Current mouse click count
extern int gMouseClickX[];            // X Coordinate of last mouse click
extern int gMouseClickY[];            // Y Coordinate of last mouse click
extern int gMouseReleaseCount[];      // Current mouse release count
extern int gMouseReleaseX[];          // X Coordinate of last mouse release
extern int gMouseReleaseY[];          // Y Coordinate of last mouse release
extern int gMickeyX;                  // Current X mickey count
extern int gMickeyY;                  // Current Y mickey count

extern unsigned short *gRegs;         // Array of registers. (valid while processing interrupts)
extern int gRMDataSeg;                // Real mode data segment
extern int gRMInfoOfs;                // Real mode info offset (in data seg)
extern int gRMModeOfs;                // Real mode mode list offset (in data seg)
extern int gRMCodeSeg;                // Real mode code segment
extern int gRMBankSwitchOfs;          // Real mode bank switch function offset (in code seg)
extern int gRMMouseTrapOfs;           // Real mode far call calling function offset (in code seg)
extern unsigned char *gRMA000Ptr;     // Pointer to the real-mode a000 segment


extern int gOptionMouseIRQCall;       // Option: perform mouse IRQ calls
extern int gOptionVRCFlipFlop;        // Option: VRC is a simple flip-flop
extern int gOptionHighPerfTimerUse;   // Option: Use high-performance timer
extern int gOptionSimpleVGARegUse;    // Option: Use simple VGA register set
extern int gOptionMouseMode;          // Option: Current mouse mode

extern int gOptionRefreshHz;          // Option: Screen refresh frequency
extern int gOptionVRCHz;              // Option: Vertical retrace frequency
extern int gOptionVRCSleep;           // Option: Vertical retrace sleep
extern int gOption16BTexture;         // Option: Try to use 16-bit textures
extern int gOptionWriteOnlyVidmem;    // Option: Assume all vidmem is overwritten
extern int gOptionReadOnlyVidmem;     // Option: Don't read back read-mode data
extern int gOptionMouseIRQOnVRC;      // Option: perform mouse IRQ only on retraces
extern int gOptionConstantMouseIRQs;  // Option: flood app with mouse IRQs

extern int gOptionDebugLevel;         // Option: debug/logging level

extern int gVidmemReadOnlyState;      // Flag: don't read from video memory

/*
- read only vidmem (while asked for read window, don't accept writes)
- batch mouse irqs
*/

///////////////////////////////////////////////
// Functions
///////////////////////////////////////////////

// Thread procedure
extern DWORD WINAPI threadproc(LPVOID lpParameter);

// Release VGA ports
extern void releasePorts();

// Capture VGA ports
extern void capturePorts();

// Handle int10h events
extern void handle_int10();

// Handle int33h events
extern void handle_int33();

// Trigger mouse IRQ event
extern void triggerMouseIRQ();

// Capture mouse
extern void captureMouse();

// Release mouse (and go to windowed mode if in fullscreen)
extern void releaseMouse();

// Perform mouse callback
extern void domouseevent();

// Return millisecond counter
extern int tick();

// Output formatted string into log file
extern void log(char * aFormat, ...);

// Output current tick to log file
extern void log_tick();

// Copy memory from a000 in tweak mode
extern void copyfromtweak(int plane);

// Copy memory to a000 in tweak mode
extern void copytotweak(int plane);

// Handle vesa 'info' call
extern void vesainfo();

// Handle vesa 'mode info' call
extern void vesamodeinfo();

// Handle vesa 'set mode' call
extern void vesasetmode();

// Handle vesa 'get mode' call
extern void vesagetmode();

// Handle vesa memory control (bank switching) call
extern void vesamemorycontrol();

// Handle vesa 'set scanline length' call
extern void vesasetscanlinelength();

// Handle vesa 'set display start' call
extern void vesasetdisplaystart();

// Handle vesa 'DAC control' call
extern void vesadaccontrol();

// Handle vesa 'set palette' call
extern void vesasetpalette();

// (re)initialize window 
extern void initWindow(int aFullscreen);

// Deinitialize screen memory
extern void deinitVideoMemory();

// Deinitialize framebuffer
extern void deinitFrameBuffer();

// Initialize framebuffer buffer
extern void initFrameBuffer(int aWidth, int aHeight);

// Initialize screen memory
extern void initVideoMemory(int aWidth, int aMode, int aClear);

// Set window size
extern void setWindowSize(int aWidth, int aHeight);
