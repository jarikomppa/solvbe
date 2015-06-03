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
// - VDD interface
// - Globals
//
///////////////////////////////////////////////

#include "SolVBE.h"



///////////////////////////////////////////////
// Globals
///////////////////////////////////////////////

                // 12345678901234567890123456789012345678901234567890123456789012345678901234567890
char gDLLInfo[] = "\tSolVBE VDD DLL Version 1.3b, September 15, 2004\n\tCopyright 2004 Jari Komppa\n";

HANDLE gDLLHandle = 0;         // DLL handle
HWND   gWindowHandle = 0;      // Display window
HWND   gConsoleWindow = 0;     // Console handle (for keyboard events)
HDC    gWindowDC = 0;          // Window device context
HGLRC  gOpenGLRC = 0;          // OpenGL rendering context
GLuint gTextureHandle = 0;     // OpenGL texture handle

HDC            gVideoMemoryDC = 0;    // Video memory buffer handle
HBITMAP        gVideoMemoryOldBM = 0; // Old bitmap / video memory
HBITMAP        gVideoMemoryBM = 0;    // Current video memory bitmap
unsigned char *gVideoMemoryPtr = 0;   // Pointer to the video memory

HDC     gFrameBufferDC = 0;    // Framebuffer buffer handle
HBITMAP gFrameBufferOldBM = 0; // Old bitmap handle / fb
HBITMAP gFrameBufferBM = 0;    // Framebuffer bitmap
int    *gFrameBufferPtr = 0;   // Pointer to the framebuffer

int gLogmode = 0;              // log mode; don't log duplicate events
int gLogmodestart = 0;         // start tick for current log mode

volatile int gThreadAlive = 1; // Flag that keeps display update thread alive
volatile int gModeChanging = 0;// Flag that keeps SolVBE from shutting down when changing modes
int gLastVRC = 0;              // Last vertical retrace tick
int gLastRefresh = 0;          // Last display refresh
volatile int gVRC = 0;         // Flag: are we inside vertical retrace?
int gStartTick = 0;            // Start tick for this SolVBE instance
CRITICAL_SECTION gThreadBusy;  // Rendering thread synchronization mutex
HANDLE gThreadHandle = 0;      // Display update thread

int gVideoBufferWidth = 320;   // Current video buffer width
int gVideoBufferPitch = 320;   // Current pitch
int gVideoBufferHeight = 200;  // Current video buffer height
int gScreenWidth = 320;  // Current screen x resolution
int gScreenHeight = 200; // Current screen y resolution
int gWindowWidth = 320;  // Current window x resolution
int gWindowHeight = 200; // Current window y resolution
int gTextureWidth = 512; // Current texture x resolution
int gTextureHeight = 256;// Current texture y resolution

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

int gBankOffset = 0;              // Current bank offset
int gDisplayXOffset = 0;          // Current display x-offset
int gDisplayYOffset = 0;          // Current display y-offset
int gSurfaceMode = 0;             // Current surface mode. Needed for setscanlinelength call.
int gLinearFiltering = 1;         // Flag: linear filtering
int gLockAspect = 1;              // Flag: aspect ratio locked
int gFullscreen = 0;              // Flag: fullscreen mode
int gPortsCaptured = 0;           // Flag: are VGA registers captured?

int gPalette[256];                // Current palette (4/8b modes)
unsigned int gPalIndex = 0;       // Current palette index (inp/outp)
unsigned int gPalWidth = 6;       // Current palette width (6 or 8 bit)
int gSeqReg04h = (2+4+8);         // Sequencer register 4h (bit 8 = chain-4)
int gSeqReg02h = (1);             // Sequencer register 2h (layer write mask)
int gCRTCReg17h = (128+64+32+8+4+2+1); // CRTC register 17h (bit 7 = byte addressing)
int gCRTCReg14h = (64 + 32);      // CRTC register 14h (bit 6 = dword addresses)
int gCRTCReg09h = (128 + 64 + 32);// CRTC register 9h (bit 8 = scan doubling)
int gGraphicsReg04h = 0;          // Graphics register 4h (read layer)

int gSeqAddress = 0;              // Active sequencer register
int gCRTCAddress = 0;             // Active CRTC address
int gGraphicsAddress = 0;         // Active graphics register

int gTweakedMode = 0;             // Flag: tweaked mode

int gCurrentMode = 3;             // Current video mode (3 = text)

int gMouseEventNeeded = 0;        // Bit flags on what kinds of things have happened since last event.
int gMouseCaptured = 0;           // Flag: is the mouse captured?
int gMouseButtons = 0;            // Currently depressed mouse buttons
int gMouseX = 0;                  // Current mouse x coordinate
int gMouseY = 0;                  // Current mouse y coordinate
int gMouseMinX = 0;               // Mouse min. x coordinate
int gMouseMaxX = 320;             // Mouse max. x coordinate
int gMouseMinY = 0;               // Mouse min. y coordinate
int gMouseMaxY = 200;             // Mouse max. y coordinate
int gMouseXSpeed = 8;             // Mouse x speed (mickeys per pixel; lower = faster)
int gMouseYSpeed = 8;             // Mouse y speed (mickeys per pixel; lower = faster)
int gMouseEventMask = 0;          // Mouse event call mask

int gMouseClickCount[2] = {0, 0}; // Current mouse click count
int gMouseClickX[2] = {0, 0};     // X Coordinate of last mouse click
int gMouseClickY[2] = {0, 0};     // Y Coordinate of last mouse click
int gMouseReleaseCount[2] = {0, 0};  // Current mouse release count
int gMouseReleaseX[2] = {0, 0};   // X Coordinate of last mouse release
int gMouseReleaseY[2] = {0, 0};   // Y Coordinate of last mouse release
int gMickeyX = 0;                 // Current X mickey count
int gMickeyY = 0;                 // Current Y mickey count

unsigned short *gRegs = NULL;     // Array of registers. (valid while processing interrupts)
int gRMDataSeg = 0;               // Real mode data segment
int gRMInfoOfs = 0;               // Real mode info offset (in data seg)
int gRMModeOfs = 0;               // Real mode mode list offset (in data seg)
int gRMCodeSeg = 0;               // Real mode code segment
int gRMBankSwitchOfs = 0;         // Real mode bank switch function offset (in code seg)
int gRMMouseTrapOfs = 0;          // Real mode far call calling function offset (in code seg)
unsigned char *gRMA000Ptr = NULL; // Pointer to the real-mode A000 segment

__int64 gTickDivisor = 1;   // PerformanceCounter divisor for milliseconds
__int64 gTickStart = 0;     // PerformanceCounter start value
int gUseHighPerformanceTimer = 0;

int gOptionMouseIRQCall = 1;      // Option: perform mouse IRQ calls
int gOptionVRCFlipFlop = 0;       // Option: VRC is a simple flip-flop
int gOptionHighPerfTimerUse = 1;  // Option: Use high-performance timer
int gOptionSimpleVGARegUse = 0;   // Option: Use simple VGA register set
int gOptionMouseMode = 1;         // Option: Current mouse mode

int gOptionRefreshHz = 60;     // Option: Screen refresh frequency
int gOptionVRCHz = 60;         // Option: Vertical retrace frequency
int gOptionVRCSleep = 0;       // Option: Vertical retrace sleep
int gOption16BTexture = 0;     // Option: Try to use 16-bit textures
int gOptionWriteOnlyVidmem = 0; // Option: Assume all vidmem is overwritten
int gOptionReadOnlyVidmem = 0; // Option: Don't read back read-mode data
int gOptionMouseIRQOnVRC = 0;  // Option: perform mouse IRQ only on retraces
int gOptionConstantMouseIRQs = 0; // Option: flood app with mouse IRQs

int gOptionDebugLevel = DEBUG_LOG;  // Option: debug/logging level

int gVidmemReadOnlyState = 0;  // Flag: don't read from video memory


///////////////////////////////////////////////
// Time
///////////////////////////////////////////////

int tick()
{
    if (gUseHighPerformanceTimer && gOptionHighPerfTimerUse)
    {
        __int64 v;
        QueryPerformanceCounter((LARGE_INTEGER *)&v);
        v -= gTickStart;
        v /= gTickDivisor;
        return (int)v;
    }
    return GetTickCount();  
}

///////////////////////////////////////////////
// Debug functions
///////////////////////////////////////////////

void log(char * aFormat, ...)
{
	if (gOptionDebugLevel < DEBUG_LOG) return;
    va_list marker;
    va_start(marker, aFormat);
    FILE * f = fopen("c:\\SolVBE.log","ab+");
    vfprintf(f, aFormat, marker);
    fclose(f);
}

void log_tick()
{
    int currenttick = tick();
    if (gLogmode != LOG_NORMAL)
    {
        gLogmode = LOG_NORMAL;
        log(" (%dms)\r\n", currenttick - gLogmodestart);
    }
    log("%8dms:",currenttick - gStartTick);
}


///////////////////////////////////////////////
// Finding the console
///////////////////////////////////////////////

void findConsoleWindow()
{
    char oldtitle[200];
    GetConsoleTitle(oldtitle,199);
    srand(GetTickCount());
    char title[81];
    int i;
    for (i = 0; i < 80; i++)    
        title[i] = (rand()%27)+'A';
    title[80] = 0;
    memcpy(title,"(SolVBE Console Detect) ", 24);                  
    SetConsoleTitle(title);
    Sleep(100); // make sure the title has changed
    gConsoleWindow = FindWindow(NULL,title);
    SetConsoleTitle(oldtitle);
    if (gConsoleWindow == NULL)
    {
        if (MessageBox(NULL,"Could not find the console window.\n"
                        "\n"
                        "Keyboard will not work.\n\n"
                        "Are you sure you want to continue?",
                        "SolVBE Error",MB_YESNO|MB_ICONWARNING) == IDNO)
        {
            VDDTerminateVDM();
        }
    }
}

///////////////////////////////////////////////
// Commandline
///////////////////////////////////////////////

void parseCmd(int aSegment, int aOffset)
{
    char *s = (char *)VdmMapFlat(aSegment,aOffset,VDM_V86);
    
    if (s[0] != 0 && s[1] != 0)
    {
        switch (tolower(s[1]))
        {
        case 'm':
            switch (tolower(s[2]))
            {
            case '0':
                gOptionMouseIRQCall = 0;
                gOptionMouseMode = 0;
                gOptionConstantMouseIRQs = 0;
                break;
            case '1':
                gOptionMouseIRQCall = 1;
                gOptionMouseMode = 1;
                gOptionMouseIRQOnVRC = 0;
                gOptionConstantMouseIRQs = 0;
                break;
            case '2':
                gOptionMouseIRQCall = 1;
                gOptionMouseMode = 2;
                gOptionMouseIRQOnVRC = 0;
                gOptionConstantMouseIRQs = 0;
                break;
            case '3':
                gOptionMouseIRQCall = 1;
                gOptionMouseMode = 1;
                gOptionMouseIRQOnVRC = 1;
                gOptionConstantMouseIRQs = 0;
                break;
            case '4':
                gOptionMouseIRQCall = 1;
                gOptionMouseMode = 2;
                gOptionMouseIRQOnVRC = 0;
                gOptionConstantMouseIRQs = 0;
                break;
            case '5':
                gOptionMouseIRQCall = 1;
                gOptionMouseMode = 1;
                gOptionMouseIRQOnVRC = 1;
                gOptionConstantMouseIRQs = 1;
                break;
            case '6':
                gOptionMouseIRQCall = 1;
                gOptionMouseMode = 2;
                gOptionMouseIRQOnVRC = 0;
                gOptionConstantMouseIRQs = 1;
                break;
            case '8':
                gOptionMouseIRQCall = 0;
                gOptionMouseMode = 3;
                gOptionMouseIRQOnVRC = 0;
                break;
            default:
                MessageBox(NULL,s,"SolVBE - Unknown option",MB_OK|MB_ICONERROR);
            }
            break;
        case 'p':
            switch (tolower(s[2]))
            {
            case '0':
                gOptionSimpleVGARegUse = 1;
                break;
            case '1':
                gOptionSimpleVGARegUse = 0;
                break;
            default:
                MessageBox(NULL,s,"SolVBE - Unknown option",MB_OK|MB_ICONERROR);
            }
            break;
        case 't':
            switch (tolower(s[2]))
            {
            case '0':
                gOptionHighPerfTimerUse = 0;
                gStartTick = tick();
                break;
            case '1':
                gOptionHighPerfTimerUse = 1;
                break;
            default:
                MessageBox(NULL,s,"SolVBE - Unknown option",MB_OK|MB_ICONERROR);
            }
            break;
        case 'f':
            switch (tolower(s[2]))
            {
            case '0':
                gLinearFiltering = 0;
                break;
            case '1':
                gLinearFiltering = 1;
                break;
            default:
                MessageBox(NULL,s,"SolVBE - Unknown option",MB_OK|MB_ICONERROR);
            }
            break;
        case 'a':
            switch (tolower(s[2]))
            {
            case '0':
                gLockAspect = 0;
                break;
            case '1':
                gLockAspect = 1;
                break;
            default:
                MessageBox(NULL,s,"SolVBE - Unknown option",MB_OK|MB_ICONERROR);
            }
            break;
        case 'x':
            switch (tolower(s[2]))
            {
            case '0':
                gOption16BTexture = 0;
                break;
            case '1':
                gOption16BTexture = 1;
                break;
            }
            break;
        case 'w':
            switch (tolower(s[2]))
            {
            case '0':
                gOptionWriteOnlyVidmem = 0;
                break;
            case '1':
                gOptionWriteOnlyVidmem = 1;
                break;
            }
            break;
        case 'r':
            switch (tolower(s[2]))
            {
            case '0':
                gOptionReadOnlyVidmem = 0;
                break;
            case '1':
                gOptionReadOnlyVidmem = 1;
                break;
            }
            break;
        case 'd':
            switch (tolower(s[2]))
            {
            case '0':
                gOptionRefreshHz = 0;
                break;
            case '1':
                gOptionRefreshHz = 5;
                break;
            case '2':
                gOptionRefreshHz = 10;
                break;
            case '3':
                gOptionRefreshHz = 15;
                break;
            case '4':
                gOptionRefreshHz = 20;
                break;
            case '5':
                gOptionRefreshHz = 25;
                break;
            case '6':
                gOptionRefreshHz = 30;
                break;
            case '7':
                gOptionRefreshHz = 40;
                break;
            case '8':
                gOptionRefreshHz = 50;
                break;
            case '9':
                gOptionRefreshHz = 60;
                break;
            case 'a':
                gOptionRefreshHz = 80;
                break;
            case 'b':
                gOptionRefreshHz = 100;
                break;
            }
            break;
        case 'v':
            switch (tolower(s[2]))
            {
            case '0':
                gOptionVRCHz = 0;
                break;
            case '1':
                gOptionVRCHz = 5;
                break;
            case '2':
                gOptionVRCHz = 10;
                break;
            case '3':
                gOptionVRCHz = 15;
                break;
            case '4':
                gOptionVRCHz = 20;
                break;
            case '5':
                gOptionVRCHz = 25;
                break;
            case '6':
                gOptionVRCHz = 30;
                break;
            case '7':
                gOptionVRCHz = 40;
                break;
            case '8':
                gOptionVRCHz = 50;
                break;
            case '9':
                gOptionVRCHz = 60;
                break;
            case 'a':
                gOptionVRCHz = 70;
                break;
            case 'b':
                gOptionVRCHz = 80;
                break;
            case 'c':
                gOptionVRCHz = 100;
                break;
            case 'd':
                gOptionVRCHz = 120;
                break;
            }
            break;
        case 's':
            switch (tolower(s[2]))
            {
            case '0':
                gOptionVRCSleep = -1;
                break;
            case '1':
                gOptionVRCSleep = 0;
                break;
            case '2':
                gOptionVRCSleep = 1;
                break;
            case '3':
                gOptionVRCSleep = 2;
                break;
            case '4':
                gOptionVRCSleep = 3;
                break;
            case '5':
                gOptionVRCSleep = 4;
                break;
            case '6':
                gOptionVRCSleep = 5;
                break;
            case '7':
                gOptionVRCSleep = 6;
                break;
            case '8':
                gOptionVRCSleep = 7;
                break;
            case '9':
                gOptionVRCSleep = 8;
                break;
            case 'a':
                gOptionVRCSleep = 9;
                break;
            case 'b':
                gOptionVRCSleep = 10;
                break;
            }
			break;
        case 'l':
            switch (tolower(s[2]))
            {
            case '0':
                gOptionDebugLevel = 0;
                break;
            case '1':
                gOptionDebugLevel = 1;
                break;
            case '2':
                gOptionDebugLevel = 2;
                break;
			}
			break;
        default:
            MessageBox(NULL,s,"SolVBE - Unknown option",MB_OK|MB_ICONERROR);
        }
    }
    
    setAX(gOptionMouseMode);
    
    VdmUnmapFlat(aSegment,aOffset,s,VDM_V86);
}


///////////////////////////////////////////////
// Main functions
///////////////////////////////////////////////


/*
*
* VDDInitialize
*
* Arguments:
*     DllHandle - Identifies this VDD
*     Reason - Attach or Detach
*     Context - Not Used
*
* Return Value:
*     TRUE: SUCCESS
*     FALSE: FAILURE
*
*/

BOOL DllMain(IN PVOID aDllHandle,
             IN ULONG aReason,
             IN PCONTEXT aContext OPTIONAL)
{    
    int i;
    switch (aReason) 
    {       
    case DLL_PROCESS_ATTACH:
        {
            if (QueryPerformanceFrequency((LARGE_INTEGER *)&gTickDivisor))
            {
                gTickDivisor /= 1000;
                QueryPerformanceCounter((LARGE_INTEGER *)&gTickStart);                
                gUseHighPerformanceTimer = 1;
            }


            gStartTick = tick();
            struct tm *newtime;
            long ltime;
            time( &ltime );

            // Obtain coordinated universal time: 
            newtime = gmtime( &ltime );
            
            // wipe log
			if (gOptionDebugLevel >= DEBUG_LOG)
			{
				FILE * f = fopen("c:\\SolVBE.log","w");
				fclose(f);
			}
            log_tick();
            log("==================================================\r\n");
            log_tick();
            log("SolVBE initialized at %s\r\n", asctime( newtime ));
        }            

        gDLLHandle = aDllHandle;
        gRMA000Ptr = (unsigned char *)VdmMapFlat(0xA000,0,VDM_V86);        
        if (gRMA000Ptr == NULL)
            MessageBox(NULL,"Unable to map a000 segment","SolVBE",MB_OK|MB_ICONERROR);
        
        InitializeCriticalSection(&gThreadBusy);

        gThreadHandle = CreateThread(
            NULL,
            0,
            threadproc,
            NULL,
            0,
            NULL);
        

        for (i = 0; i < 256; i++)
            gPalette[i] = i * 0xf47b4b35;
        break;
        
    case DLL_PROCESS_DETACH:
        gThreadAlive = 0;
        Sleep(500);
        DeleteCriticalSection(&gThreadBusy);
        DestroyWindow(gWindowHandle);
        VdmUnmapFlat(0xA000,0,gRMA000Ptr,VDM_V86);

        releasePorts();
        {
            struct tm *newtime;
            long ltime;
            time( &ltime );

            // Obtain coordinated universal time:
            newtime = gmtime( &ltime );
            log_tick();
            log("SolVBE deinitialized at %s\r\n", asctime( newtime ));
            log_tick();
            log("--------------------------------------------------\r\n");
        }
        
        break;
        
    default:
        break;
    }
    
    return TRUE;
}


/*
*
* VDDInit
*
*/
extern "C" VOID SolVBEInit( VOID )
{
    
    // Called from dos side. Since we do all initialization
    // in dllmain, we can just set carry clear here to signal "ok".
    
    setCF( 0 );
        
    return;
}





extern "C" VOID SolVBECall( VOID )
{
    if (getBX() == 0x0074)
    {
        // mouse irq event
        domouseevent(); 
        return;
    }
    if (getBX() == 0x0100)
    {
        // configure call 1
        gRMDataSeg = getDS();
        gRMInfoOfs = getCX();
        gRMModeOfs = getDX();
        char *infoptr = (char *)VdmMapFlat(gRMDataSeg, gRMInfoOfs, VDM_V86);
        strcpy(infoptr, gDLLInfo);
        VdmUnmapFlat(gRMDataSeg, gRMInfoOfs, infoptr, VDM_V86);
        return;
    }
    if (getBX() == 0x0200)
    {
        // configure call 2
        findConsoleWindow(); // find the console window handle
        gRMCodeSeg = getCS();
        gRMBankSwitchOfs = getCX();        
        gRMMouseTrapOfs = getDX();
        return;
    }
    if (getBX() == 0x0300)
    {
        // Configure call 3: commandline parameters
        parseCmd(getCX(), getDX());
        return;
    }
    if (getBX() == 0x0033)
    {
        // Mouse interrupt
        gRegs = (unsigned short *)VdmMapFlat(getSS(),getBP(),VDM_V86);
        
        handle_int33();       

        VdmUnmapFlat(getSS(),getBP(),gRegs,VDM_V86);
        return;
    }
    if (getBX() == 0x0010)
    {
        // Video interrupt
        gRegs = (unsigned short *)VdmMapFlat(getSS(),getBP(),VDM_V86);
        
        handle_int10();        

        VdmUnmapFlat(getSS(),getBP(),gRegs,VDM_V86);
        return;
    }    
    char temp[1024];
    
	sprintf(temp,"Unknown call: BX=%04x\n(version mismatch?)",getBX());
	
	MessageBox(NULL,temp,"SolVBE Unknown Call",MB_OK|MB_ICONERROR);

    VDDTerminateVDM();
}
