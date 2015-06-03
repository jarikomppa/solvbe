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
// - display thread
// - other window-related functions
//
///////////////////////////////////////////////

#include "SolVBE.h"

///////////////////////////////////////////////
// Display-update functions
///////////////////////////////////////////////

int nextpowerof2(int value)
{
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 3;
    value |= value >> 4;
    value |= value >> 5;
    value |= value >> 6;
    value |= value >> 7;
    value |= value >> 8;
    value |= value >> 9;
    value |= value >> 10;
    value |= value >> 11;
    value |= value >> 12;
    value |= value >> 13;
    value |= value >> 14;
    value |= value >> 15;
    value++;
    return value;
}

void render()
{
    if (gWindowWidth == 0 || gWindowHeight == 0)
        gWindowWidth = gWindowHeight = 128;
    float aspect = 4.0f / 3.0f;

    if (gCurrentMode != 3)
    {
        // blit to temporary buffer (pixel conversion & clip)
        BitBlt(gFrameBufferDC,0,0,gScreenWidth,gScreenHeight,gVideoMemoryDC,gDisplayXOffset,gDisplayYOffset,SRCCOPY); 
    }
    else
    {
        // Load and display SolVBE logo while in text mode.
        // (yes, it is somewhat wasteful way to do it every frame)
        HBITMAP logo = LoadBitmap((HINSTANCE)gDLLHandle,MAKEINTRESOURCE(IDB_LOGO));
        HDC lDC = CreateCompatibleDC(NULL);
        HBITMAP old = (HBITMAP)SelectObject(lDC,logo);
        BitBlt(gFrameBufferDC,0,0,128,128,lDC,0,0,SRCCOPY); 
        SelectObject(lDC,old);
        DeleteObject(logo);
        DeleteDC(lDC);
        aspect = 1;
    }

    // upload texture
    glTexImage2D(GL_TEXTURE_2D, // target
                 0, // level
                 (gOption16BTexture) ? GL_RGB5 : GL_RGB, // internalformat
                 gTextureWidth, // width
                 gTextureHeight, // height
                 0, // border
                 GL_RGBA, // format
                 GL_UNSIGNED_BYTE, // type
                 gFrameBufferPtr); // texels
    // render

    if (gLinearFiltering)
    {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
    glEnable(GL_TEXTURE_2D);

	glShadeModel(GL_SMOOTH);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0);
	glViewport(0,0,gWindowWidth,gWindowHeight);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

    glClear(GL_COLOR_BUFFER_BIT);

    float u = (float)gScreenWidth / (float)gTextureWidth;
    float v = (float)gScreenHeight / (float)gTextureHeight;
    
    float w = 1, h = 1;

    if (gLockAspect)
    {
        w = (gWindowHeight * aspect) / gWindowWidth;
        h = (gWindowWidth * (1 / aspect)) / gWindowHeight;

        if (w > h) w = 1; else h = 1;
    }

    glBegin(GL_TRIANGLE_FAN);
        glColor3f(1.0f,1.0f,1.0f); 
        glTexCoord2f(0,0); glVertex2f( -w,  h);
        glTexCoord2f(u,0); glVertex2f(  w,  h);
        glTexCoord2f(u,v); glVertex2f(  w, -h);	
        glTexCoord2f(0,v); glVertex2f( -w, -h);
    glEnd();
    
    SwapBuffers(gWindowDC);    
}

void deinitVideoMemory()
{
    SelectObject(gVideoMemoryDC, gVideoMemoryOldBM);
    DeleteDC(gVideoMemoryDC);
    DeleteObject(gVideoMemoryBM);
}

void deinitFrameBuffer()
{
    glDeleteTextures(1,&gTextureHandle);
    SelectObject(gFrameBufferDC,gFrameBufferOldBM);
    DeleteDC(gFrameBufferDC);
    DeleteObject(gFrameBufferBM);
}

void setWindowSize(int aWidth, int aHeight)
{        
    RECT r;
    r.left = 64;
    r.top = 64;
    r.bottom = aHeight+64;
    r.right = aWidth+64;
    AdjustWindowRect(&r,WS_SYSMENU|WS_CAPTION|WS_BORDER|WS_OVERLAPPED|WS_VISIBLE|WS_MINIMIZEBOX,FALSE);
    MoveWindow(gWindowHandle,r.left,r.top,r.right-r.left,r.bottom-r.top,TRUE);
}


void initFrameBuffer(int aWidth, int aHeight)
{
    gScreenWidth = aWidth;
    gScreenHeight = aHeight;
    gTextureWidth = nextpowerof2(aWidth);
    gTextureHeight = nextpowerof2(aHeight);

    log_tick();
    log("Mode init: Texture size: %dx%d, screen size: %dx%d\r\n",gTextureWidth,gTextureHeight,gScreenWidth,gScreenHeight);

    glGenTextures(1,&gTextureHandle);
    glBindTexture(GL_TEXTURE_2D,gTextureHandle);

    HDC hDC;
    BITMAPINFO *bitmapinfo;
    hDC = CreateCompatibleDC(NULL);
    bitmapinfo = (BITMAPINFO *)new unsigned char[sizeof(BITMAPINFO) + 4 * sizeof(int)];
    memset(bitmapinfo, 0, sizeof(BITMAPINFO) + 4 * sizeof(int));
    bitmapinfo->bmiColors[0].rgbBlue = 0xff;
    bitmapinfo->bmiColors[1].rgbGreen = 0xff;
    bitmapinfo->bmiColors[2].rgbRed = 0xff;
    bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapinfo->bmiHeader.biWidth = gTextureWidth;
    bitmapinfo->bmiHeader.biHeight = -(signed)gTextureHeight; /* top-down */
    bitmapinfo->bmiHeader.biPlanes = 1;
    bitmapinfo->bmiHeader.biBitCount = 32;
    bitmapinfo->bmiHeader.biCompression = BI_BITFIELDS;
    bitmapinfo->bmiHeader.biSizeImage =  aWidth * aHeight * 4;
    bitmapinfo->bmiHeader.biClrUsed = 256;
    bitmapinfo->bmiHeader.biClrImportant = 256;
    gFrameBufferBM = CreateDIBSection(hDC, bitmapinfo, DIB_RGB_COLORS, (void**)&gFrameBufferPtr, 0, 0);
    gFrameBufferDC = CreateCompatibleDC(NULL);
    gFrameBufferOldBM = (HBITMAP)SelectObject(gFrameBufferDC, gFrameBufferBM);
    DeleteDC(hDC);
    delete[] bitmapinfo;
}


void initVideoMemory(int aWidth, int aMode, int aClear)
{
    int aHeight = (VIDMEM_KB * 1024);
    switch (aMode)
    {
    case MODE_4BPAL:
        aHeight /= (aWidth / 2);
        break;
    case MODE_8BPAL:
        aHeight /= aWidth;
        break;
    case MODE_15B:
    case MODE_16B:
        aHeight /= (aWidth * 2);
        break;
    case MODE_24B:
        aHeight /= (aWidth * 3);
        break;
    case MODE_32B:
        aHeight /= (aWidth * 4);
        break;
    }
    gVideoBufferWidth = aWidth;
    gVideoBufferHeight = aHeight;
    gSurfaceMode = aMode;
    HDC hDC;
    BITMAPINFO *bitmapinfo;
    hDC = CreateCompatibleDC(NULL);
    switch (aMode)
    {
    case MODE_32B:
        bitmapinfo = new BITMAPINFO;
        bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapinfo->bmiHeader.biWidth = aWidth;
        bitmapinfo->bmiHeader.biHeight = -aHeight; /* top-down */
        bitmapinfo->bmiHeader.biPlanes = 1;
        bitmapinfo->bmiHeader.biBitCount = 32;
        bitmapinfo->bmiHeader.biCompression = BI_RGB;
        bitmapinfo->bmiHeader.biSizeImage =  aWidth * aHeight * 4;
        bitmapinfo->bmiHeader.biClrUsed = 256;
        bitmapinfo->bmiHeader.biClrImportant = 256;
        gVideoMemoryBM = CreateDIBSection(hDC, bitmapinfo, DIB_RGB_COLORS, (void**)&gVideoMemoryPtr, 0, 0);
        break;
    case MODE_24B:
        bitmapinfo = new BITMAPINFO;
        bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapinfo->bmiHeader.biWidth = aWidth;
        bitmapinfo->bmiHeader.biHeight = -aHeight; /* top-down */
        bitmapinfo->bmiHeader.biPlanes = 1;
        bitmapinfo->bmiHeader.biBitCount = 24;
        bitmapinfo->bmiHeader.biCompression = BI_RGB;
        bitmapinfo->bmiHeader.biSizeImage =  aWidth * aHeight * 3;
        bitmapinfo->bmiHeader.biClrUsed = 0;
        bitmapinfo->bmiHeader.biClrImportant = 0;
        gVideoMemoryBM = CreateDIBSection(hDC, bitmapinfo, DIB_RGB_COLORS, (void**)&gVideoMemoryPtr, 0, 0);
        break;
    case MODE_16B:
        bitmapinfo = (BITMAPINFO *)new unsigned char[sizeof(BITMAPINFO) + 2 * sizeof(int)];
        memset(bitmapinfo, 0, sizeof(BITMAPINFO) + 2 * sizeof(int));
        bitmapinfo->bmiColors[0].rgbGreen = 0xf8; // set 5:6:5 mask
        bitmapinfo->bmiColors[1].rgbGreen = 0x07;
        bitmapinfo->bmiColors[1].rgbBlue  = 0xe0;
        bitmapinfo->bmiColors[2].rgbBlue  = 0x1f;
        bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapinfo->bmiHeader.biWidth = aWidth;
        bitmapinfo->bmiHeader.biHeight = -aHeight; // top-down 
        bitmapinfo->bmiHeader.biPlanes = 1;
        bitmapinfo->bmiHeader.biBitCount = 16;
        bitmapinfo->bmiHeader.biCompression = BI_BITFIELDS;
        bitmapinfo->bmiHeader.biSizeImage = aWidth * aHeight * 2;
        bitmapinfo->bmiHeader.biClrUsed = 0;
        bitmapinfo->bmiHeader.biClrImportant = 0;
        gVideoMemoryBM = CreateDIBSection(hDC, bitmapinfo, DIB_RGB_COLORS, (void**)&gVideoMemoryPtr, 0, 0);
        break;
    case MODE_15B:
        bitmapinfo = (BITMAPINFO *)new unsigned char[sizeof(BITMAPINFO) + 2 * sizeof(int)];
        memset(bitmapinfo, 0, sizeof(BITMAPINFO) + 2 * sizeof(int));
        bitmapinfo->bmiColors[0].rgbGreen = 0x7c; // set 1:5:5:5 mask
        bitmapinfo->bmiColors[1].rgbGreen = 0x03;
        bitmapinfo->bmiColors[1].rgbBlue  = 0xe0;
        bitmapinfo->bmiColors[2].rgbBlue  = 0x1f;
        bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapinfo->bmiHeader.biWidth = aWidth;
        bitmapinfo->bmiHeader.biHeight = -aHeight; // top-down 
        bitmapinfo->bmiHeader.biPlanes = 1;
        bitmapinfo->bmiHeader.biBitCount = 16;
        bitmapinfo->bmiHeader.biCompression = BI_BITFIELDS;
        bitmapinfo->bmiHeader.biSizeImage = aWidth * aHeight * 2;
        bitmapinfo->bmiHeader.biClrUsed = 0;
        bitmapinfo->bmiHeader.biClrImportant = 0;
        gVideoMemoryBM = CreateDIBSection(hDC, bitmapinfo, DIB_RGB_COLORS, (void**)&gVideoMemoryPtr, 0, 0);
        break;
    case MODE_8BPAL:
        bitmapinfo = (BITMAPINFO *)new unsigned char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
        memset(bitmapinfo, 0, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
        bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapinfo->bmiHeader.biWidth = aWidth;
        bitmapinfo->bmiHeader.biHeight = -aHeight; // top-down 
        bitmapinfo->bmiHeader.biPlanes = 1;
        bitmapinfo->bmiHeader.biBitCount = 8;
        bitmapinfo->bmiHeader.biCompression = BI_RGB;
        bitmapinfo->bmiHeader.biSizeImage = aWidth * aHeight;
        bitmapinfo->bmiHeader.biClrUsed = 0;
        bitmapinfo->bmiHeader.biClrImportant = 0;
        gVideoMemoryBM = CreateDIBSection(hDC, bitmapinfo, DIB_RGB_COLORS, (void**)&gVideoMemoryPtr, 0, 0);
        break;
    case MODE_4BPAL:
        bitmapinfo = (BITMAPINFO *)new unsigned char[sizeof(BITMAPINFOHEADER) + 16 * sizeof(RGBQUAD)];
        memset(bitmapinfo, 0, sizeof(BITMAPINFOHEADER) + 16 * sizeof(RGBQUAD));
        bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapinfo->bmiHeader.biWidth = aWidth;
        bitmapinfo->bmiHeader.biHeight = -aHeight; // top-down 
        bitmapinfo->bmiHeader.biPlanes = 1;
        bitmapinfo->bmiHeader.biBitCount = 8;
        bitmapinfo->bmiHeader.biCompression = BI_RGB;
        bitmapinfo->bmiHeader.biSizeImage = aWidth * aHeight / 2;
        bitmapinfo->bmiHeader.biClrUsed = 0;
        bitmapinfo->bmiHeader.biClrImportant = 0;
        gVideoMemoryBM = CreateDIBSection(hDC, bitmapinfo, DIB_RGB_COLORS, (void**)&gVideoMemoryPtr, 0, 0);
        break;
    }
    gVideoMemoryDC = CreateCompatibleDC(NULL);
    gVideoMemoryOldBM = (HBITMAP)SelectObject(gVideoMemoryDC, gVideoMemoryBM);
    DeleteDC(hDC);
    if (aClear)
        memset(gVideoMemoryPtr, 0, bitmapinfo->bmiHeader.biSizeImage);
    delete[] bitmapinfo;
}


LRESULT CALLBACK winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_MBUTTONUP:
        if (gMouseCaptured)
        {
            releaseMouse();
        }
        else
        {
            captureMouse();
        }
        break;
    case WM_LBUTTONDOWN:
        if (gMouseCaptured)
        {
            gMouseButtons |= 1;
            gMouseClickCount[0]++;
            gMouseClickX[0] = gMouseX;
            gMouseClickY[0] = gMouseY;
            gMouseEventNeeded |= MOUSE_LBDOWN;            
        }
        break;
    case WM_LBUTTONUP:
        if (gMouseCaptured)
        {
            gMouseButtons &= ~1;
            gMouseReleaseCount[0]++;
            gMouseReleaseX[0] = gMouseX;
            gMouseReleaseY[0] = gMouseY;
            gMouseEventNeeded |= MOUSE_LBUP;
        }
        break;
    case WM_RBUTTONDOWN:
        if (gMouseCaptured)
        {
            gMouseButtons |= 2;
            gMouseClickCount[1]++;
            gMouseClickX[1] = gMouseX;
            gMouseClickY[1] = gMouseY;
            gMouseEventNeeded |= MOUSE_RBDOWN;
        }
        break;
    case WM_RBUTTONUP:
        if (gMouseCaptured)
        {
            gMouseButtons &= ~2;
            gMouseReleaseCount[1]++;
            gMouseReleaseX[1] = gMouseX;
            gMouseReleaseY[1] = gMouseY;
            gMouseEventNeeded |= MOUSE_RBUP;
        }
        else
        {
            HMENU m = LoadMenu((HINSTANCE)gDLLHandle,MAKEINTRESOURCE(IDR_POPUP));
            HMENU p = GetSubMenu(m,0);
            if (gLockAspect)
                CheckMenuItem(p,ID_POPUP_LOCKASPECT,MF_CHECKED);
            if (gLinearFiltering)
                CheckMenuItem(p,ID_POPUP_LINEARFILTERING,MF_CHECKED);
            if (gOption16BTexture)
                CheckMenuItem(p,ID_POPUP_PERFORMANCE_16BITTEXTURE,MF_CHECKED);
            if (gOptionWriteOnlyVidmem)
                CheckMenuItem(p,ID_POPUP_PERFORMANCE_WRITEONLYVIDMEM,MF_CHECKED);
            if (gOptionReadOnlyVidmem)
                CheckMenuItem(p,ID_POPUP_PERFORMANCE_READONLYVIDMEM,MF_CHECKED);
            if (gOptionMouseIRQOnVRC)
                CheckMenuItem(p,ID_POPUP_PERFORMANCE_MOUSEIRQONVRC,MF_CHECKED);
            if (gOptionConstantMouseIRQs)
                CheckMenuItem(p,ID_POPUP_PERFORMANCE_MOUSEIRQFLOOD,MF_CHECKED);

            switch (gOptionRefreshHz)
            {
            case 100:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_100HZ,MF_CHECKED);
                break;
            case 80:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_80HZ,MF_CHECKED);
                break;
            case 60:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_60HZ,MF_CHECKED);
                break;
            case 50:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_50HZ,MF_CHECKED);
                break;
            case 40:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_40HZ,MF_CHECKED);
                break;
            case 30:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_30HZ,MF_CHECKED);
                break;
            case 25:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_25HZ,MF_CHECKED);
                break;
            case 20:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_20HZ,MF_CHECKED);
                break;
            case 15:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_15HZ,MF_CHECKED);
                break;
            case 10:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_10HZ,MF_CHECKED);
                break;
            case 5:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_5HZ,MF_CHECKED);
                break;
            case 0:
                CheckMenuItem(p, ID_POPUP_SPEED_DISPLAYUPDATE_NODELAYS,MF_CHECKED);
                break;
            }
            switch (gOptionVRCHz)
            {
            case 120:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_120HZ,MF_CHECKED);
                break;
            case 100:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_100HZ,MF_CHECKED);
                break;
            case 80:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_80HZ,MF_CHECKED);
                break;
            case 70:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_70HZ,MF_CHECKED);
                break;
            case 60:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_60HZ,MF_CHECKED);
                break;
            case 50:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_50HZ,MF_CHECKED);
                break;
            case 40:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_40HZ,MF_CHECKED);
                break;
            case 30:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_30HZ,MF_CHECKED);
                break;
            case 25:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_25HZ,MF_CHECKED);
                break;
            case 20:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_20HZ,MF_CHECKED);
                break;
            case 15:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_15HZ,MF_CHECKED);
                break;
            case 10:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_10HZ,MF_CHECKED);
                break;
            case 5:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_5HZ,MF_CHECKED);
                break;
            case 0:
                CheckMenuItem(p, ID_POPUP_SPEED_VERTICALRETRACE_FLIPFLOP,MF_CHECKED);
                break;
            }
            switch (gOptionVRCSleep)
            {
            case -1:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_NOSLEEP,MF_CHECKED);
                break;
            case 0:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP0,MF_CHECKED);
                break;
            case 1:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP1,MF_CHECKED);
                break;
            case 2:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP2,MF_CHECKED);
                break;
            case 3:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP3,MF_CHECKED);
                break;
            case 4:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP4,MF_CHECKED);
                break;
            case 5:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP5,MF_CHECKED);
                break;
            case 6:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP6,MF_CHECKED);
                break;
            case 7:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP7,MF_CHECKED);
                break;
            case 8:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP8,MF_CHECKED);
                break;
            case 9:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP9,MF_CHECKED);
                break;
            case 10:
                CheckMenuItem(p, ID_POPUP_SPEED_VRCTIMING_SLEEP10,MF_CHECKED);
                break;
            }
            
            POINT point;
            GetCursorPos(&point); // screen coords
            TrackPopupMenu(p,
                TPM_VCENTERALIGN|TPM_CENTERALIGN,
                point.x,
                point.y,
                0,
                gWindowHandle,
                0);
            DestroyMenu(m);
        }
        break;
    case WM_MOUSEMOVE:
        if (gMouseCaptured)
        {
            // Step 1: figure out how much mouse has moved
            RECT rect;            
            POINT point;
            GetCursorPos(&point); // screen coords
            int mousex = point.x;
            int mousey = point.y;
            GetWindowRect(hWnd,&rect);
            int centerx = (rect.left+rect.right) / 2;
            int centery = (rect.top+rect.bottom) / 2;
            
            int mousexdelta = mousex - centerx;
            int mouseydelta = mousey - centery;

            gMouseX += mousexdelta * 8 / gMouseXSpeed;
            gMouseY += mouseydelta * 8 / gMouseYSpeed;
            gMickeyX += mousexdelta * 8;
            gMickeyY += mousexdelta * 8;
            if (gMouseX < gMouseMinX) gMouseX = gMouseMinX;
            if (gMouseX > gMouseMaxX) gMouseX = gMouseMaxX;
            if (gMouseY < gMouseMinY) gMouseY = gMouseMinY;
            if (gMouseY > gMouseMaxY) gMouseY = gMouseMaxY;
/*
            gMouseX += mousexdelta;
            gMouseY += mouseydelta;
            gMickeyX += mousexdelta * 8;
            gMickeyY += mousexdelta * 8;
            if (gMouseX < gMouseMinX) gMouseX = gMouseMinX;
            if (gMouseX > gMouseMaxX) gMouseX = gMouseMaxX;
            if (gMouseY < gMouseMinY) gMouseY = gMouseMinY;
            if (gMouseY > gMouseMaxY) gMouseY = gMouseMaxY;
*/

            if (mousex != centerx ||
                mousey != centery)            
            {
                // Step 2: force mouse back under the window..
                SetCursorPos(centerx, centery);
            }
            gMouseEventNeeded |= MOUSE_MOVE;
        }
        break;
    case WM_DESTROY:        
        if (!gModeChanging)
            VDDTerminateVDM(); // take VDM down if window closes (and we're not the ones doing it)
        break;        
    case WM_KEYDOWN:
    case WM_KEYUP:    
        if (gConsoleWindow)
        {
            SendMessage(gConsoleWindow,uMsg,wParam,lParam);
        }
        break;
	case WM_SIZE:								// Resize The OpenGL Window
        gWindowWidth = LOWORD(lParam);
        gWindowHeight = HIWORD(lParam);
		break;
    case WM_ACTIVATE:
        if (wParam == WA_INACTIVE)            
        {
            // If we lose focus, remove mouse capture
            // and go back to windowed mode if we were
            // in fullscreen mode.
            if (gMouseCaptured)
            {
                releaseMouse();
            }
        }
    case WM_COMMAND:
        if (HIWORD(wParam) == 0)
        {
            int w,h;
            switch(LOWORD(wParam))
            {
            case ID_POPUP_RESOLUTION_1X:
            case ID_POPUP_RESOLUTION_2X:
            case ID_POPUP_RESOLUTION_3X:
            case ID_POPUP_RESOLUTION_4X:
                if (gLockAspect)
                {
                    w = gScreenHeight * 4 / 3;
                    h = gScreenWidth * 3 / 4;
                    if (w > gScreenWidth)
                        h = gScreenHeight;
                    else
                        w = gScreenWidth;
                }
                else
                {
                    w = gScreenWidth;
                    h = gScreenHeight;
                }
            }
            // menu command
            switch(LOWORD(wParam))
            {
            case ID_POPUP_LINEARFILTERING:
                gLinearFiltering = 1 - gLinearFiltering;
                break;
            case ID_POPUP_LOCKASPECT:
                gLockAspect = 1 - gLockAspect;
                break;
            case ID_POPUP_RESOLUTION_640X480:
                setWindowSize(640, 480);
                break;
            case ID_POPUP_RESOLUTION_1X:
                setWindowSize(w,h);
                break;
            case ID_POPUP_RESOLUTION_2X:
                setWindowSize(w * 2, h * 2);
                break;
            case ID_POPUP_RESOLUTION_3X:
                setWindowSize(w * 3, h * 3);
                break;
            case ID_POPUP_RESOLUTION_4X:
                setWindowSize(w * 4, h * 4);
                break;
            case ID_POPUP_FULLSCREEN:
                initWindow(1); // go fullscreen
                break;
            case ID_POPUP_WEB_HTTPIKIFISOL:
                ShellExecute(NULL, "open", "http://iki.fi/sol", NULL, NULL, SW_SHOW);
                break;
            case ID_POPUP_WEB_HTTPVOGONSZETAFLEETCOM:
                ShellExecute(NULL, "open", "http://vogons.zetafleet.com", NULL, NULL, SW_SHOW);
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_100HZ:
                gOptionRefreshHz = 100;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_80HZ:
                gOptionRefreshHz = 80;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_60HZ:
                gOptionRefreshHz = 60;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_50HZ:
                gOptionRefreshHz = 50;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_40HZ:
                gOptionRefreshHz = 40;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_30HZ:
                gOptionRefreshHz = 30;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_25HZ:
                gOptionRefreshHz = 25;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_20HZ:
                gOptionRefreshHz = 20;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_15HZ:
                gOptionRefreshHz = 15;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_10HZ:
                gOptionRefreshHz = 10;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_5HZ:
                gOptionRefreshHz = 5;
                break;
            case ID_POPUP_SPEED_DISPLAYUPDATE_NODELAYS:
                gOptionRefreshHz = 0;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_FLIPFLOP:
                gOptionVRCHz = 0;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_120HZ:
                gOptionVRCHz = 120;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_100HZ:
                gOptionVRCHz = 100;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_80HZ:
                gOptionVRCHz = 80;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_70HZ:
                gOptionVRCHz = 70;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_60HZ:
                gOptionVRCHz = 60;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_50HZ:
                gOptionVRCHz = 50;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_40HZ:
                gOptionVRCHz = 40;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_30HZ:
                gOptionVRCHz = 30;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_25HZ:
                gOptionVRCHz = 25;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_20HZ:
                gOptionVRCHz = 20;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_15HZ:
                gOptionVRCHz = 15;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_10HZ:
                gOptionVRCHz = 10;
                break;
            case ID_POPUP_SPEED_VERTICALRETRACE_5HZ:
                gOptionVRCHz = 5;
                break;
            case ID_POPUP_SPEED_VRCTIMING_NOSLEEP:
                gOptionVRCSleep = -1;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP0:
                gOptionVRCSleep = 0;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP1:
                gOptionVRCSleep = 1;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP2:
                gOptionVRCSleep = 2;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP3:
                gOptionVRCSleep = 3;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP4:
                gOptionVRCSleep = 4;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP5:
                gOptionVRCSleep = 5;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP6:
                gOptionVRCSleep = 6;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP7:
                gOptionVRCSleep = 7;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP8:
                gOptionVRCSleep = 8;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP9:
                gOptionVRCSleep = 9;
                break;
            case ID_POPUP_SPEED_VRCTIMING_SLEEP10:
                gOptionVRCSleep = 10;
                break;
            case ID_POPUP_PERFORMANCE_16BITTEXTURE:
                gOption16BTexture = !gOption16BTexture;
                initWindow(0); // reinit window
                break;
            case ID_POPUP_PERFORMANCE_WRITEONLYVIDMEM:
                gOptionWriteOnlyVidmem = !gOptionWriteOnlyVidmem;
                break;
            case ID_POPUP_PERFORMANCE_READONLYVIDMEM:
                gOptionReadOnlyVidmem = !gOptionReadOnlyVidmem;
                break;
            case ID_POPUP_PERFORMANCE_MOUSEIRQONVRC:
                gOptionMouseIRQOnVRC = !gOptionMouseIRQOnVRC;
                break;
            case ID_POPUP_TERMINATEVDM:
                if (MessageBox(gWindowHandle,"Are you sure you want to terminate?","SolVBE Confirmation",MB_YESNO | MB_ICONQUESTION) == IDYES)
                    VDDTerminateVDM();
                break;
            case ID_POPUP_PERFORMANCE_MOUSEIRQFLOOD:
                gOptionConstantMouseIRQs = !gOptionConstantMouseIRQs;
                break;
            }
        }
        break;
    default:
        return DefWindowProc (hWnd, uMsg, wParam, lParam);
        break;
    }
    return 0;
}

void deinitWindow()
{
    wglMakeCurrent(NULL,NULL);
    wglDeleteContext(gOpenGLRC);
    ReleaseDC(gWindowHandle, gWindowDC);
    DestroyWindow(gWindowHandle);
    UnregisterClass("SolVBE", (HINSTANCE)gDLLHandle);
}

void initWindow(int aFullscreen)
{
    if (gWindowHandle)
    {
        gModeChanging = 1;
        if (gFullscreen)
        {
            ChangeDisplaySettings(NULL, 0);
        }
        deinitWindow();
    }

    WNDCLASSEX winclass;

    winclass.cbSize=sizeof(WNDCLASSEX);
    winclass.style=CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    winclass.lpfnWndProc=&winproc;
    winclass.cbClsExtra=0;
    winclass.cbWndExtra=0;
    winclass.hInstance=(HINSTANCE)gDLLHandle;
    winclass.hIcon=LoadIcon((HINSTANCE)gDLLHandle,MAKEINTRESOURCE(IDI_ICON1));
    winclass.hCursor=LoadCursor((HINSTANCE)gDLLHandle, MAKEINTRESOURCE(IDC_CURSOR1));
    winclass.hbrBackground=NULL;
    winclass.lpszMenuName=NULL;
    winclass.lpszClassName="SolVBE";
    winclass.hIconSm=LoadIcon((HINSTANCE)gDLLHandle,MAKEINTRESOURCE(IDI_ICON1));

    if (!RegisterClassEx(&winclass))
        return;

    gFullscreen = 0;

    if (aFullscreen)
    {
		DEVMODE ss;
		memset(&ss, 0, sizeof(ss));
		ss.dmSize = sizeof(ss);
		ss.dmPelsWidth = 640;
		ss.dmPelsHeight = 480;
		ss.dmBitsPerPel = 32;
		ss.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		if (ChangeDisplaySettings(&ss, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
            MessageBox(NULL,"Unable to change to full screen.","SolVBE Error",MB_OK|MB_ICONERROR);
		}
        else
        {
            gFullscreen = 1;
            gWindowWidth = 640;
            gWindowHeight = 480;
        }
    }

    if (gFullscreen)
    {
        gWindowHandle=CreateWindowEx(WS_EX_TOPMOST,
                          "SolVBE",
                          "SolVBE",
                          WS_POPUP|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,
                          0,
                          0,
                          640,
                          480,
                          NULL,
                          NULL,
                          (HINSTANCE)gDLLHandle,
                          NULL);
    }
    else
    {
        RECT r;
        r.left = 64;
        r.top = 64;
        if (gCurrentMode == 3)
        {
            r.bottom = 128 + 64;
            r.right = 128 + 64;
        }
        else
        {
            r.bottom = 480 + 64;
            r.right = 640 + 64;
        }
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS|WS_CLIPCHILDREN, FALSE);

        gWindowHandle=CreateWindow("SolVBE",
                          "SolVBE",
                          WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,
                          r.left,
                          r.top,
                          r.right-r.left,
                          r.bottom-r.top,
                          NULL,
                          NULL,
                          (HINSTANCE)gDLLHandle,
                          NULL);
    }

    PIXELFORMATDESCRIPTOR pfd;
    pfd.nSize=sizeof(PIXELFORMATDESCRIPTOR);                             // Size 
    pfd.nVersion=1;                                                      // Version
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;  // Selected flags
    pfd.iPixelType=PFD_TYPE_RGBA;                                        // Pixelformat
    pfd.cColorBits=16;                                                   // Pixel depth
    pfd.cDepthBits=16;                                                   // Zbuffer depth
    pfd.iLayerType=PFD_MAIN_PLANE;                                       // Place the pixelformat on the main plane


    gWindowDC=GetDC(gWindowHandle);
    int pf=ChoosePixelFormat(gWindowDC, &pfd);
	SetPixelFormat(gWindowDC, pf, &pfd);
	gOpenGLRC = wglCreateContext(gWindowDC);
	wglMakeCurrent(gWindowDC, gOpenGLRC);

    ShowWindow(gWindowHandle,SW_SHOW);

    gModeChanging = 0;

    if (gFullscreen)
    {
        captureMouse();
    }
    else
    {
        // start with the console window focused..
        if (gConsoleWindow && gCurrentMode == 3)
        {
            SetForegroundWindow(gConsoleWindow);
            SetFocus(gConsoleWindow);
        }
    }
}


DWORD WINAPI threadproc(LPVOID lpParameter)
{
    EnterCriticalSection(&gThreadBusy);
    initWindow(0);
    initVideoMemory(320, MODE_8BPAL, 1);
    initFrameBuffer(128,128);
    setWindowSize(128,128);
    LeaveCriticalSection(&gThreadBusy);
    gThreadAlive = 1;
    while (gThreadAlive)
    {
        // update window about 60 times per second
        int currenttick = tick();
        if (gOptionRefreshHz == 0 || ((currenttick - gLastRefresh) >= (1000 / gOptionRefreshHz)))
        {
            EnterCriticalSection(&gThreadBusy);
            if (gCurrentMode != 3)
            {
                if (gTweakedMode)
                {
                    if (!(gOptionReadOnlyVidmem && gVidmemReadOnlyState))
                    {
                        int i;
                        for (i = 0; i < 4; i++)
                            if (gSeqReg02h & (1 << i))
                                copyfromtweak(i);
                    }
                }
                else
                {
                    if (!(gOptionReadOnlyVidmem && gVidmemReadOnlyState))
                    {
                        if (gRMA000Ptr != NULL) 
                            if ((gBankOffset + 0x20000) < (VIDMEM_KB * 1024))
                                memcpy(gVideoMemoryPtr + gBankOffset, gRMA000Ptr, 0x10000);
                    }
                }
            }
            render();
            LeaveCriticalSection(&gThreadBusy);
            if (gOptionRefreshHz == 0)
            {
                gLastRefresh = currenttick;
            }
            else
                gLastRefresh += (1000 / gOptionRefreshHz);
        }        

        if (gOptionVRCHz)
        {
             if ((currenttick - gLastVRC) >= (1000 / gOptionVRCHz))
             {
                 if (gCurrentMode != 3)
                 {
                     gVRC = 1;
                     EnterCriticalSection(&gThreadBusy);
                     // set current palette
                     SetDIBColorTable(gVideoMemoryDC, 0, 256, (CONST RGBQUAD *)gPalette);
                     LeaveCriticalSection(&gThreadBusy);
                     if (gOptionVRCSleep != -1)
                         Sleep(gOptionVRCSleep);
                     gVRC = 0;
                 }
                 gLastVRC += (1000 / gOptionVRCHz);
                 if (gMouseEventNeeded && gOptionMouseIRQOnVRC)
                     triggerMouseIRQ();
             }
        }
        else
        {
             gVRC = !gVRC;
             gLastVRC = currenttick;
             // set current palette
             EnterCriticalSection(&gThreadBusy);
             SetDIBColorTable(gVideoMemoryDC, 0, 256, (CONST RGBQUAD *)gPalette);
             LeaveCriticalSection(&gThreadBusy);
        }



        MSG msg;
        while (PeekMessage(&msg,gWindowHandle,0,0,PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (gMouseEventNeeded && !(gOptionMouseIRQOnVRC && gOptionVRCHz != 0))
            triggerMouseIRQ();

        Sleep(1);
        
    }
    EnterCriticalSection(&gThreadBusy);
    deinitVideoMemory();
    deinitFrameBuffer();
    LeaveCriticalSection(&gThreadBusy);
    return 0;
}
