// =======================================================================
// GUIslice library
// - Calvin Hass
// - http://www.impulseadventure.com/elec/microsdl-sdl-gui.html
//
// - Version 0.8    (2016/12/24)
// =======================================================================
//
// The MIT License
//
// Copyright 2016 Calvin Hass
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// =======================================================================



// GUIslice library


#include "GUIslice.h"
#include "GUIslice_ex.h"
#include "GUIslice_drv.h"


#include <stdio.h>
#include <time.h> // for FrameRate reporting


// Version definition
#define GUISLICE_VER "0.8"



// ========================================================================

// ------------------------------------------------------------------------
// General Functions
// ------------------------------------------------------------------------

char* gslc_GetVer(gslc_tsGui* pGui)
{
  return (char*)GUISLICE_VER;
}



bool gslc_Init(gslc_tsGui* pGui,void* pvDriver,gslc_tsPage* asPage,uint8_t nMaxPage,gslc_tsFont* asFont,uint8_t nMaxFont)
{
  unsigned  nInd;
  
  // Initialize state
  pGui->nDispW          = 0;
  pGui->nDispH          = 0;
  pGui->nDispDepth      = 0;
  
  pGui->nPageMax        = nMaxPage;
  pGui->nPageCnt        = 0;
  pGui->asPage          = asPage;

  pGui->pCurPage        = NULL;
  pGui->pCurPageCollect = NULL;

  // Initialize collection of fonts with user-supplied pointer
  pGui->asFont      = asFont;
  pGui->nFontMax    = nMaxFont;
  pGui->nFontCnt    = 0;
  for (nInd=0;nInd<(pGui->nFontMax);nInd++) {
    gslc_ResetFont(&(pGui->asFont[nInd]));
  }
 
  // Initialize temporary element
  gslc_ResetElem(&(pGui->sElemTmp));


  // Last touch event
  pGui->nTouchLastX           = 0;
  pGui->nTouchLastY           = 0;
  pGui->nTouchLastPress       = 0;

  pGui->pfuncXEvent = NULL;
  
  pGui->sImgRefBkgnd = gslc_ResetImage();
    
  // Save a link to the driver
  pGui->pvDriver = pvDriver;
  
  // Default to no support for partial redraw
  // - This may be overridden by the driver-specific init
  pGui->bRedrawPartialEn = false;
 
  
  #ifdef DBG_FRAME_RATE
  pGui->nFrameRateCnt = 0;
  pGui->nFrameRateStart = time(NULL);
  #endif
  
  // Initialize the display and touch drivers
  if (!gslc_DrvInit(pGui)) { return false; }
  if (!gslc_InitTouch(pGui,GSLC_DEV_TOUCH)) { return false; }
  
  return true;
}


void gslc_Quit(gslc_tsGui* pGui)
{
  // Close all elements and fonts
  gslc_GuiDestruct(pGui);
}

// Main polling loop for GUIslice
void gslc_Update(gslc_tsGui* pGui)
{
  int16_t   nTouchX,nTouchY;
  uint16_t  nTouchPress;
  bool      bTouchEvent;
  
  // Poll for touchscreen presses  
  bTouchEvent = gslc_GetTouch(pGui,&nTouchX,&nTouchY,&nTouchPress);
  
  if (bTouchEvent) {
    // Track and handle the touch events
    // - Handle the events on the current page
    gslc_TrackTouch(pGui,pGui->pCurPage,nTouchX,nTouchY,nTouchPress);
    
    #ifdef DBG_TOUCH
    // Highlight current touch for coordinate debug
    gslc_tsRect rMark = gslc_ExpandRect((gslc_tsRect){(int16_t)nTouchX,(int16_t)nTouchY,1,1},1,1);
    gslc_DrawFrameRect(pGui,rMark,GSLC_COL_YELLOW);
    #endif    
  }
  
  // Issue a timer tick to all pages
  uint8_t nPageInd;
  gslc_tsPage* pPage = NULL;
  for (nPageInd=0;nPageInd<pGui->nPageCnt;nPageInd++) {
    pPage = &pGui->asPage[nPageInd];    
    gslc_tsEvent sEvent = gslc_EventCreate(GSLC_EVT_TICK,0,(void*)pPage,NULL);
    gslc_PageEvent(pGui,sEvent);
  }
  
  // Perform any redraw required for current page
  gslc_PageRedrawGo(pGui);
  
  // Simple "frame" rate reporting
  // - Note that the rate is based on the number of calls to gslc_Update()
  //   per second, which may or may not redraw the frame
  #ifdef DBG_FRAME_RATE
  pGui->nFrameRateCnt++;
  uint32_t  nElapsed = (time(NULL) - pGui->nFrameRateStart);
  if (nElapsed > 0) {
    debug_print("Update rate: %6u / sec\n",pGui->nFrameRateCnt);
    pGui->nFrameRateStart = time(NULL);
    pGui->nFrameRateCnt = 0;
  }
  #endif  
}

gslc_tsEvent  gslc_EventCreate(gslc_teEventType eType,uint8_t nSubType,void* pvScope,void* pvData)
{
  gslc_tsEvent    sEvent;
  sEvent.eType    = eType;
  sEvent.nSubType = nSubType;
  sEvent.pvScope  = pvScope;
  sEvent.pvData   = pvData;
  return sEvent;
}


// ------------------------------------------------------------------------
// Graphics General Functions
// ------------------------------------------------------------------------


bool gslc_IsInRect(int16_t nSelX,int16_t nSelY,gslc_tsRect rRect)
{
  if ( (nSelX >= rRect.x) && (nSelX <= rRect.x+rRect.w) && 
     (nSelY >= rRect.y) && (nSelY <= rRect.y+rRect.h) ) {
    return true;
  } else {
    return false;
  }
}

bool gslc_IsInWH(gslc_tsGui* pGui,int16_t nSelX,int16_t nSelY,uint16_t nWidth,uint16_t nHeight)
{
  if ( (nSelX >= 0) && (nSelX <= nWidth-1) && 
     (nSelY >= 0) && (nSelY <= nHeight-1) ) {
    return true;
  } else {
    return false;
  }
}

// Ensure the coordinates are increasing from nX0->nX1 and nY0->nY1
// NOTE: UNUSED
void gslc_OrderCoord(int16_t* pnX0,int16_t* pnY0,int16_t* pnX1,int16_t* pnY1)
{
  int16_t  nTmp;
  if ((*pnX1) < (*pnX0)) {
    nTmp = (*pnX0);
    (*pnX0) = (*pnX1);
    (*pnX1) = nTmp;
  }
  if ((*pnY1) < (*pnY0)) {
    nTmp = (*pnY0);
    (*pnY0) = (*pnY1);
    (*pnY1) = nTmp;
  }
}

gslc_tsImgRef gslc_ResetImage()
{
  gslc_tsImgRef sImgRef;
  sImgRef.eImgFlags = GSLC_IMGREF_NONE;
  sImgRef.pFname    = NULL;
  sImgRef.pImgBuf   = NULL;
  sImgRef.pvImgRaw  = NULL;
  return sImgRef;
}

gslc_tsImgRef gslc_GetImageFromFile(const char* pFname,gslc_teImgRefFlags eFmt)
{
  gslc_tsImgRef sImgRef;
  sImgRef.eImgFlags = GSLC_IMGREF_SRC_FILE | (GSLC_IMGREF_FMT & eFmt);
  sImgRef.pFname    = pFname;
  sImgRef.pImgBuf   = NULL;
  sImgRef.pvImgRaw  = NULL;
  return sImgRef;
}

gslc_tsImgRef gslc_GetImageFromSD(const char* pFname,gslc_teImgRefFlags eFmt)
{
  gslc_tsImgRef sImgRef;
  sImgRef.eImgFlags = GSLC_IMGREF_SRC_SD | (GSLC_IMGREF_FMT & eFmt);
  sImgRef.pFname    = pFname;
  sImgRef.pImgBuf   = NULL;
  sImgRef.pvImgRaw  = NULL;
  return sImgRef;
}

gslc_tsImgRef gslc_GetImageFromRam(unsigned char* pImgBuf,gslc_teImgRefFlags eFmt)
{
  gslc_tsImgRef sImgRef;
  sImgRef.eImgFlags = GSLC_IMGREF_SRC_RAM | (GSLC_IMGREF_FMT & eFmt);
  sImgRef.pFname    = NULL;
  sImgRef.pImgBuf   = pImgBuf;
  sImgRef.pvImgRaw  = NULL;
  return sImgRef;  
}


gslc_tsImgRef gslc_GetImageFromProg(const unsigned char* pImgBuf,gslc_teImgRefFlags eFmt)
{
  gslc_tsImgRef sImgRef;
  sImgRef.eImgFlags = GSLC_IMGREF_SRC_PROG | (GSLC_IMGREF_FMT & eFmt);
  sImgRef.pFname    = NULL;
  sImgRef.pImgBuf   = pImgBuf;
  sImgRef.pvImgRaw  = NULL;
  return sImgRef;
}


// ------------------------------------------------------------------------
// Graphics Primitive Functions
// ------------------------------------------------------------------------

void gslc_DrawSetPixel(gslc_tsGui* pGui,int16_t nX,int16_t nY,gslc_tsColor nCol)
{
   
#if (DRV_HAS_DRAW_POINT) 
  // Call optimized driver point drawing
  gslc_DrvDrawPoint(pGui,nX,nY,nCol);
#else  
  debug_print("ERROR: Mandatory DrvDrawPoint() is not defined in driver\n");
#endif
  
  gslc_PageFlipSet(pGui,true);
}

// Draw an arbitrary line using Bresenham's algorithm
// - Algorithm reference: https://rosettacode.org/wiki/Bitmap/Bresenham's_line_algorithm#C
void gslc_DrawLine(gslc_tsGui* pGui,int16_t nX0,int16_t nY0,int16_t nX1,int16_t nY1,gslc_tsColor nCol)
{
  
#if (DRV_HAS_DRAW_LINE) 
  // Call optimized driver line drawing
  gslc_DrvDrawLine(pGui,nX0,nY0,nX1,nY1,nCol);
  
#else
  // Perform Bresenham's line algorithm
  int16_t nDX = abs(nX1-nX0);
  int16_t nDY = abs(nY1-nY0);

  int16_t nSX = (nX0 < nX1)? 1 : -1;
  int16_t nSY = (nY0 < nY1)? 1 : -1;
  int16_t nErr = ( (nDX>nDY)? nDX : -nDY )/2;
  int16_t nE2;
  
  // Check for degenerate cases
  // TODO: Need to test these optimizations
  bool bDone = false;
  if (nDX == 0) {
    if (nDY == 0) {
      return;
    } else if (nDY >= 0) {
      gslc_DrawLineV(pGui,nX0,nY0,nDY,nCol);
      bDone = true;
    } else {
      gslc_DrawLineV(pGui,nX1,nY1,-nDY,nCol);
      bDone = true;
    }
  } else if (nDY == 0) {
    if (nDX >= 0) {
      gslc_DrawLineH(pGui,nX0,nY0,nDX,nCol);
      bDone = true;      
    } else {
      gslc_DrawLineH(pGui,nX1,nY1,-nDX,nCol);
      bDone = true;      
    }
  }

  if (!bDone) {
    for (;;) {
      // Set the pixel
      gslc_DrvDrawPoint(pGui,nX0,nY0,nCol);

      // Calculate next coordinates
      if ( (nX0 == nX1) && (nY0 == nY1) ) break;
      nE2 = nErr;
      if (nE2 > -nDX) { nErr -= nDY; nX0 += nSX; }
      if (nE2 <  nDY) { nErr += nDX; nY0 += nSY; }
    }
  }
  gslc_PageFlipSet(pGui,true);   
#endif
  
}


void gslc_DrawLineH(gslc_tsGui* pGui,int16_t nX, int16_t nY, uint16_t nW,gslc_tsColor nCol)
{
  
  uint16_t nOffset;
  for (nOffset=0;nOffset<nW;nOffset++) {
    gslc_DrvDrawPoint(pGui,nX+nOffset,nY,nCol);    
  }  
  
  gslc_PageFlipSet(pGui,true);  
}

void gslc_DrawLineV(gslc_tsGui* pGui,int16_t nX, int16_t nY, uint16_t nH,gslc_tsColor nCol)
{
  
  uint16_t nOffset;
  for (nOffset=0;nOffset<nH;nOffset++) {
    gslc_DrvDrawPoint(pGui,nX,nY+nOffset,nCol);    
  }
  
  gslc_PageFlipSet(pGui,true);
}


void gslc_DrawFrameRect(gslc_tsGui* pGui,gslc_tsRect rRect,gslc_tsColor nCol)
{
  // Ensure dimensions are valid
  if ((rRect.w == 0) || (rRect.h == 0)) {
    return;
  }

#if (DRV_HAS_DRAW_RECT_FRAME)
  // Call optimized driver implementation
  gslc_DrvDrawFrameRect(pGui,rRect,nCol);
#else
  // Emulate rect frame with four lines
  int16_t   nX,nY;
  uint16_t  nH,nW;
  nX = rRect.x;
  nY = rRect.y;
  nW = rRect.w;
  nH = rRect.h;
  gslc_DrawLineH(pGui,nX,nY,nW-1,nCol);                 // Top
  gslc_DrawLineH(pGui,nX,(int16_t)(nY+nH-1),nW-1,nCol); // Bottom
  gslc_DrawLineV(pGui,nX,nY,nH-1,nCol);                 // Left
  gslc_DrawLineV(pGui,(int16_t)(nX+nW-1),nY,nH-1,nCol); // Right
#endif
  
  gslc_PageFlipSet(pGui,true);  
}

void gslc_DrawFillRect(gslc_tsGui* pGui,gslc_tsRect rRect,gslc_tsColor nCol)
{
  // Ensure dimensions are valid
  if ((rRect.w == 0) || (rRect.h == 0)) {
    return;
  }
  
#if (DRV_HAS_DRAW_RECT_FILL)
  // Call optimized driver implementation
  gslc_DrvDrawFillRect(pGui,rRect,nCol);
#else
  // Emulate it with individual line draws
  // TODO: This should be avoided as it will generally be very inefficient
  int nRow;
  for (nRow=0;nRow<rRect.h;nRow++) {
    gslc_DrawLineH(pGui, rRect.x, rRect.y+nRow, rRect.w, nCol);
  }

#endif
  
  gslc_PageFlipSet(pGui,true);    
}


// Expand or contract a rectangle in width and/or height (equal
// amounts on both side), based on the centerpoint of the rectangle.
gslc_tsRect gslc_ExpandRect(gslc_tsRect rRect,int16_t nExpandW,int16_t nExpandH)
{
  gslc_tsRect rNew = {0,0,0,0};

  // Detect error case of contracting region too far
  if (rRect.w + (2*nExpandW) < 0) {
    //debug_print("ERROR: ExpandRect(%d,%d) contracts too far",nExpandW,nExpandH);
    return rNew;
  }
  if (rRect.w + (2*nExpandW) < 0) {
    //debug_print("ERROR: ExpandRect(%d,%d) contracts too far",nExpandW,nExpandH);
    return rNew;
  }

  // Adjust the new width/height
  // Note that the overall width/height changes by a factor of
  // two since we are applying the adjustment on both sides (ie.
  // top/bottom or left/right) equally.
  rNew.w = rRect.w + (2*nExpandW);
  rNew.h = rRect.h + (2*nExpandH);

  // Adjust the rectangle coordinate to allow for new dimensions
  // Note that this moves the coordinate in the opposite
  // direction of the expansion/contraction.
  rNew.x = rRect.x - nExpandW;
  rNew.y = rRect.y - nExpandH;

  return rNew;
}


// Draw a circle using midpoint circle algorithm
// - Algorithm reference: https://en.wikipedia.org/wiki/Midpoint_circle_algorithm
void gslc_DrawFrameCircle(gslc_tsGui* pGui,int16_t nMidX,int16_t nMidY,
  uint16_t nRadius,gslc_tsColor nCol)
{
  
  #if (DRV_HAS_DRAW_CIRCLE_FRAME)
    // Call optimized driver implementation
    gslc_DrvDrawFrameCircle(pGui,nMidX,nMidY,nRadius,nCol);    
  #else
    // Emulate circle with point drawing
    
    int16_t nX    = nRadius;
    int16_t nY    = 0;
    int16_t nErr  = 0;

    #if (DRV_HAS_DRAW_POINTS)
      gslc_tsPt asPt[8];
      while (nX >= nY)
      {
        asPt[0] = (gslc_tsPt){nMidX + nX, nMidY + nY};
        asPt[1] = (gslc_tsPt){nMidX + nY, nMidY + nX};
        asPt[2] = (gslc_tsPt){nMidX - nY, nMidY + nX};
        asPt[3] = (gslc_tsPt){nMidX - nX, nMidY + nY};
        asPt[4] = (gslc_tsPt){nMidX - nX, nMidY - nY};
        asPt[5] = (gslc_tsPt){nMidX - nY, nMidY - nX};
        asPt[6] = (gslc_tsPt){nMidX + nY, nMidY - nX};
        asPt[7] = (gslc_tsPt){nMidX + nX, nMidY - nY};
        gslc_DrvDrawPoints(pGui,asPt,8,nCol);

        nY    += 1;
        nErr  += 1 + 2*nY;
        if (2*(nErr-nX) + 1 > 0)
        {
            nX -= 1;
            nErr += 1 - 2*nX;
        }
      } // while
    
    #elif (DRV_HAS_DRAW_POINT)
      while (nX >= nY)
      {
        gslc_DrvDrawPoint(pGui,nMidX + nX, nMidY + nY,nCol);
        gslc_DrvDrawPoint(pGui,nMidX + nY, nMidY + nX,nCol);
        gslc_DrvDrawPoint(pGui,nMidX - nY, nMidY + nX,nCol);
        gslc_DrvDrawPoint(pGui,nMidX - nX, nMidY + nY,nCol);
        gslc_DrvDrawPoint(pGui,nMidX - nX, nMidY - nY,nCol);
        gslc_DrvDrawPoint(pGui,nMidX - nY, nMidY - nX,nCol);
        gslc_DrvDrawPoint(pGui,nMidX + nY, nMidY - nX,nCol);
        gslc_DrvDrawPoint(pGui,nMidX + nX, nMidY - nY,nCol);

        nY    += 1;
        nErr  += 1 + 2*nY;
        if (2*(nErr-nX) + 1 > 0)
        {
            nX -= 1;
            nErr += 1 - 2*nX;
        }
      } // while

    #else
      // ERROR
    #endif

  #endif
  
  gslc_PageFlipSet(pGui,true);
}



// -----------------------------------------------------------------------
// Font Functions
// -----------------------------------------------------------------------

bool gslc_FontAdd(gslc_tsGui* pGui,int16_t nFontId,const char* acFontName,uint16_t nFontSz)
{
  if (pGui->nFontCnt+1 > (pGui->nFontMax)) {
    debug_print("ERROR: FontAdd(%s) added too many fonts\n","");
    return false;
  } else { 
    // Fetch a font resource from the driver
    void* pvFont = gslc_DrvFontAdd(acFontName,nFontSz);
 
    pGui->asFont[pGui->nFontCnt].pvFont = pvFont;
    pGui->asFont[pGui->nFontCnt].nId    = nFontId;
    pGui->asFont[pGui->nFontCnt].nSize  = nFontSz;
    pGui->nFontCnt++;  
    return true;
  }
}


gslc_tsFont* gslc_FontGet(gslc_tsGui* pGui,int16_t nFontId)
{
  uint8_t  nFontInd;
  for (nFontInd=0;nFontInd<pGui->nFontCnt;nFontInd++) {
    if (pGui->asFont[nFontInd].nId == nFontId) {
      return &(pGui->asFont[nFontInd]);
    }
  }
  return NULL;
}




// ------------------------------------------------------------------------
// Page Functions
// ------------------------------------------------------------------------

// Common event handler
bool gslc_PageEvent(void* pvGui,gslc_tsEvent sEvent)
{
  if (pvGui == NULL) {
    debug_print("ERROR: PageEvent(%s) called with NULL ptr\n","");
    return false;
  }
  //gslc_tsGui*       pGui        = (gslc_tsGui*)(pvGui);
  //void*             pvData      = sEvent.pvData;
  gslc_tsPage*        pPage       = (gslc_tsPage*)(sEvent.pvScope);
  gslc_tsCollect*     pCollect    = NULL;
    
  // Handle any page-level events first
  // ...
  
  // A Page only contains one Element Collection, so propagate
  
  // Handle the event types
  switch(sEvent.eType) {
    case GSLC_EVT_DRAW:
    case GSLC_EVT_TICK:    
    case GSLC_EVT_TOUCH:      
      pCollect  = &pPage->sCollect;
      // Update scope reference & propagate
      sEvent.pvScope = (void*)(pCollect);
      gslc_CollectEvent(pvGui,sEvent);
      break;

    default:
      break;
  } // sEvent.eType
  
  return true;
}

void gslc_PageAdd(gslc_tsGui* pGui,int16_t nPageId,gslc_tsElem* psElem,uint16_t nMaxElem)
{
  gslc_tsPage*  pPage = &pGui->asPage[pGui->nPageCnt];

  // TODO: Create proper PageReset()
  pPage->bPageNeedRedraw  = true;
  pPage->bPageNeedFlip    = false;
  pPage->pfuncXEvent      = NULL;
  
  // Initialize pPage->sCollect
  gslc_CollectReset(&pPage->sCollect,psElem,nMaxElem);
  
  // Assign the requested Page ID
  pPage->nPageId = nPageId;
  
  // Increment the page count
  pGui->nPageCnt++;
  
  // Default the page pointer to the first page we create
  if (gslc_GetPageCur(pGui) == GSLC_PAGE_NONE) {
    gslc_SetPageCur(pGui,nPageId);
  }
  
  // Force the page to redraw
  gslc_PageRedrawSet(pGui,true);

}

int gslc_GetPageCur(gslc_tsGui* pGui)
{
  if (pGui->pCurPage == NULL) {
    return GSLC_PAGE_NONE;
  }
  return pGui->pCurPage->nPageId;
}


void gslc_SetPageCur(gslc_tsGui* pGui,int16_t nPageId)
{
  int16_t nPageSaved = GSLC_PAGE_NONE;
  if (pGui->pCurPage != NULL) {
    nPageSaved = pGui->pCurPage->nPageId;
  }
  
  // Find the page
  gslc_tsPage* pPage = gslc_PageFindById(pGui,nPageId);
  if (pPage == NULL) {
    debug_print("ERROR: SetPageCur() can't find page (ID=%d)\n",nPageId);
    exit(1);
  }
  
  // Save a reference to the selected page
  pGui->pCurPage = pPage;
  
  // Save a reference to the selected page's element collection
  pGui->pCurPageCollect = &pPage->sCollect;
  
  // A change of page should always force a future redraw
  if (nPageSaved != nPageId) {
    gslc_PageRedrawSet(pGui,true);
  }
}


// Adjust the flag that indicates whether the entire page
// requires a redraw.
void gslc_PageRedrawSet(gslc_tsGui* pGui,bool bRedraw)
{
  pGui->pCurPage->bPageNeedRedraw = bRedraw;
}

bool gslc_PageRedrawGet(gslc_tsGui* pGui)
{
  return pGui->pCurPage->bPageNeedRedraw;
}

// Check the redraw flag on all elements on the current page and update
// the redraw status if additional redraws are required (or the
// entire page should be marked as requiring redraw).
// - The typical case for this being required is when an element
//   requires redraw but it is marked as being transparent. Therefore,
//   the lower level elements should be redrawn.
// - For now, just mark the entire page as requiring redraw.
// TODO: Determine which elements underneath should be redrawn based
//       on the region exposed by the transparent element.
void gslc_PageRedrawCalc(gslc_tsGui* pGui)
{
  int               nInd;
  gslc_tsElem*  pElem = NULL;
  for (nInd=GSLC_IND_FIRST;nInd<pGui->pCurPageCollect->nElemCnt;nInd++) {
    pElem = &(pGui->pCurPageCollect->asElem[nInd]);
    if (pElem->bNeedRedraw) {
      
      // Determine if entire page requires redraw
      bool  bRedrawFullPage = false;
      
      // If partial redraw is supported, then we
      // look out for transparent elements which may
      // still warrant full page redraw.
      if (pGui->bRedrawPartialEn) {
        // Is the element transparent?
        if (!pElem->bFillEn) {
          bRedrawFullPage = true;
        }
      } else {
        bRedrawFullPage = true;
      }
      
      if (bRedrawFullPage) {
        // Mark the entire page as requiring redraw
        gslc_PageRedrawSet(pGui,true);
        // No need to check any more elements 
        break;
      }
      
    }
  }
}

// Redraw the active page
// - If the page has been marked as needing redraw, then all
//   elements are rendered
// - If the page has not been marked as needing redraw then only
//   the elements that have been marked as needing redraw
//   are rendered.
void gslc_PageRedrawGo(gslc_tsGui* pGui)
{ 
  // Update any page redraw status that may be required
  // - Note that this routine handles cases where an element
  //   marked as requiring update is semi-transparent which can
  //   cause other elements to be redrawn as well.
  gslc_PageRedrawCalc(pGui);

  // Determine final state of full-page redraw
  bool  bPageRedraw = gslc_PageRedrawGet(pGui);

  // If a full page redraw is required, then start by
  // redrawing the background.
  // NOTE:
  // - It would be cleaner if we could treat the background
  //   layer like any other element (and hence check for its
  //   need-redraw status).
  // - For now, assume background doesn't need update except
  //   if the entire page is to be redrawn
  //   TODO: Fix this assumption (either add specific flag
  //         for bBkgndNeedRedraw or make the background just
  //         another element).
  if (bPageRedraw) {
    gslc_DrvDrawBkgnd(pGui);
    gslc_PageFlipSet(pGui,true);
  }
    
  // Draw other elements (as needed, unless forced page redraw)
  uint32_t nSubType = (bPageRedraw)?GSLC_EVTSUB_DRAW_FORCE:GSLC_EVTSUB_DRAW_NEEDED;
  void* pvData = (void*)(pGui->pCurPage);
  gslc_tsEvent  sEvent = gslc_EventCreate(GSLC_EVT_DRAW,nSubType,pvData,NULL);
  gslc_PageEvent(pGui,sEvent);
  
 
  // Clear the page redraw flag
  gslc_PageRedrawSet(pGui,false);
  
  // Page flip the entire screen
  // - TODO: We could also call Update instead of Flip as that would
  //         limit the region to refresh.
  gslc_PageFlipGo(pGui);
  
}


void gslc_PageFlipSet(gslc_tsGui* pGui,bool bNeeded)
{
  pGui->pCurPage->bPageNeedFlip = bNeeded;

  // To assist in debug of drawing primitives, support immediate
  // rendering of the current display. Note that this only works
  // for display drivers that don't invalidate the double-buffer (eg. SDL1.2)
#ifdef DBG_DRAW_IMM
  gslc_DrvPageFlipNow(pGui);
#endif
}

bool gslc_PageFlipGet(gslc_tsGui* pGui)
{
  return pGui->pCurPage->bPageNeedFlip;
}

void gslc_PageFlipGo(gslc_tsGui* pGui)
{
  if (pGui->pCurPage->bPageNeedFlip) {
    gslc_DrvPageFlipNow(pGui);
    
    // Indicate that page flip is no longer required
    gslc_PageFlipSet(pGui,false);
  }
}


gslc_tsPage* gslc_PageFindById(gslc_tsGui* pGui,int16_t nPageId)
{
  int8_t nInd;
  
  // Loop through list of pages
  // Return pointer to page
  gslc_tsPage*  pFoundPage = NULL;
  for (nInd=0;nInd<pGui->nPageMax;nInd++) {
    if (pGui->asPage[nInd].nPageId == nPageId) {
      pFoundPage = &pGui->asPage[nInd];
      break;
    }
  }
  
  // Error handling: if not found, make this a fatal error
  // as it shows a serious config error and continued operation
  // is not viable.
  if (pFoundPage == NULL) {
    debug_print("ERROR: PageGet() could not find page (ID=%d)",nPageId);
    exit(1);
  }
  
  return pFoundPage;
}

gslc_tsElem* gslc_PageFindElemById(gslc_tsGui* pGui,int16_t nPageId,int16_t nElemId)
{
  gslc_tsPage*  pPage = NULL;
  gslc_tsElem*  pElem = NULL;

  // Get the page
  pPage = gslc_PageFindById(pGui,nPageId);
  if (pPage == NULL) {
    debug_print("ERROR: PageFindElemById() can't find page (ID=%d)\n",nPageId);
    exit(1);
  }
  // Find the element in the page's element collection
  pElem = gslc_CollectFindElemById(&pPage->sCollect,nElemId);
  return pElem;
}


void gslc_PageSetEventFunc(gslc_tsPage* pPage,GSLC_CB_EVENT funcCb)
{
  if ((pPage == NULL) || (funcCb == NULL)) {
    debug_print("ERROR: PageSetEventFunc(%s) called with NULL ptr\n","");
    return;
  }    
  pPage->pfuncXEvent       = funcCb;
}

// ------------------------------------------------------------------------
// Element General Functions
// ------------------------------------------------------------------------


int gslc_ElemGetId(gslc_tsElem* pElem)
{
  return pElem->nId;
}


// ------------------------------------------------------------------------
// Element Creation Functions
// ------------------------------------------------------------------------

gslc_tsElem* gslc_ElemCreateTxt(gslc_tsGui* pGui,int16_t nElemId,int16_t nPage,gslc_tsRect rElem,
  char* pStrBuf,uint8_t nStrBufMax,int16_t nFontId)
{
  gslc_tsElem   sElem;
  gslc_tsElem*  pElem = NULL;
  sElem = gslc_ElemCreate(pGui,nElemId,nPage,GSLC_TYPE_TXT,rElem,pStrBuf,nStrBufMax,nFontId);
  sElem.colElemFill       = GSLC_COL_BLACK;
  sElem.colElemFillGlow   = GSLC_COL_BLACK;
  sElem.colElemFrame      = GSLC_COL_BLACK;
  sElem.colElemFrameGlow  = GSLC_COL_BLACK;
  sElem.colElemText       = GSLC_COL_YELLOW;
  sElem.colElemTextGlow   = GSLC_COL_YELLOW;
  sElem.bFillEn           = true;
  sElem.eTxtAlign         = GSLC_ALIGN_MID_LEFT;
  if (nPage != GSLC_PAGE_NONE) {
    pElem = gslc_ElemAdd(pGui,nPage,&sElem);
    return pElem;
  } else {
    // Save as temporary element
    pGui->sElemTmp = sElem;
    return &(pGui->sElemTmp);      
  }
}

gslc_tsElem* gslc_ElemCreateBtnTxt(gslc_tsGui* pGui,int16_t nElemId,int16_t nPage,
  gslc_tsRect rElem,char* pStrBuf,uint8_t nStrBufMax,int16_t nFontId,GSLC_CB_TOUCH cbTouch)
{
  gslc_tsElem   sElem;
  gslc_tsElem*  pElem = NULL;
  
  // Ensure the Font has been defined
  if (gslc_FontGet(pGui,nFontId) == NULL) {
    debug_print("ERROR: ElemCreateBtnTxt(ID=%d): Font(ID=%d) not loaded\n",nElemId,nFontId);
    return NULL;
  }

  sElem = gslc_ElemCreate(pGui,nElemId,nPage,GSLC_TYPE_BTN,rElem,pStrBuf,nStrBufMax,nFontId);
  sElem.colElemFill       = GSLC_COL_BLUE_DK4;
  sElem.colElemFillGlow   = GSLC_COL_BLUE_DK1;
  sElem.colElemFrame      = GSLC_COL_BLUE_DK2;
  sElem.colElemFrameGlow  = GSLC_COL_BLUE_DK2;
  sElem.colElemText       = GSLC_COL_WHITE;
  sElem.colElemTextGlow   = GSLC_COL_WHITE;
  sElem.bFrameEn          = true;
  sElem.bFillEn           = true;
  sElem.bClickEn          = true;
  sElem.bGlowEn           = true;
  sElem.pfuncXTouch       = cbTouch;
  if (nPage != GSLC_PAGE_NONE) {
    pElem = gslc_ElemAdd(pGui,nPage,&sElem);
    return pElem;
  } else {
    // Save as temporary element
    pGui->sElemTmp = sElem;
    return &(pGui->sElemTmp);       
  }
}


gslc_tsElem* gslc_ElemCreateBtnImg(gslc_tsGui* pGui,int16_t nElemId,int16_t nPage,
  gslc_tsRect rElem,gslc_tsImgRef sImgRef,gslc_tsImgRef sImgRefSel,GSLC_CB_TOUCH cbTouch)
{
  gslc_tsElem   sElem;
  gslc_tsElem*  pElem = NULL;
  sElem = gslc_ElemCreate(pGui,nElemId,nPage,GSLC_TYPE_BTN,rElem,NULL,0,GSLC_FONT_NONE);
  sElem.colElemFill       = GSLC_COL_BLACK;
  sElem.colElemFillGlow   = GSLC_COL_BLACK;
  sElem.colElemFrame      = GSLC_COL_BLUE_DK2;
  sElem.colElemFrameGlow  = GSLC_COL_BLUE_DK2;
  sElem.bFrameEn          = false;
  sElem.bFillEn           = false;
  sElem.bClickEn          = true;
  sElem.bGlowEn           = true;  
  sElem.pfuncXTouch       = cbTouch;  
  gslc_ElemSetImage(pGui,&sElem,sImgRef,sImgRefSel);
  if (nPage != GSLC_PAGE_NONE) {
    pElem = gslc_ElemAdd(pGui,nPage,&sElem);
    return pElem;
  } else {
    // Save as temporary element
    pGui->sElemTmp = sElem;
    return &(pGui->sElemTmp);     
  }
}


gslc_tsElem* gslc_ElemCreateBox(gslc_tsGui* pGui,int16_t nElemId,int16_t nPage,gslc_tsRect rElem)
{
  gslc_tsElem   sElem;
  gslc_tsElem*  pElem = NULL;
  sElem = gslc_ElemCreate(pGui,nElemId,nPage,GSLC_TYPE_BOX,rElem,NULL,0,GSLC_FONT_NONE);
  sElem.colElemFill       = GSLC_COL_BLACK;
  sElem.colElemFillGlow   = GSLC_COL_BLACK;
  sElem.colElemFrame      = GSLC_COL_GRAY;
  sElem.colElemFrameGlow  = GSLC_COL_GRAY;
  sElem.bFillEn           = true;
  sElem.bFrameEn          = true;
  if (nPage != GSLC_PAGE_NONE) {
    pElem = gslc_ElemAdd(pGui,nPage,&sElem);  
    return pElem;
  } else {
    // Save as temporary element
    pGui->sElemTmp = sElem;
    return &(pGui->sElemTmp);     
  }
}


gslc_tsElem* gslc_ElemCreateImg(gslc_tsGui* pGui,int16_t nElemId,int16_t nPage,
  gslc_tsRect rElem,gslc_tsImgRef sImgRef)
{
  gslc_tsElem   sElem;
  gslc_tsElem*  pElem = NULL;
  sElem = gslc_ElemCreate(pGui,nElemId,nPage,GSLC_TYPE_BOX,rElem,NULL,0,GSLC_FONT_NONE);
  sElem.bFrameEn        = false;
  sElem.bFillEn         = false;
  sElem.bClickEn        = false;
  gslc_ElemSetImage(pGui,&sElem,sImgRef,sImgRef);
  if (nPage != GSLC_PAGE_NONE) {
    pElem = gslc_ElemAdd(pGui,nPage,&sElem);
    return pElem;
  } else {
    // Save as temporary element
    pGui->sElemTmp = sElem;
    return &(pGui->sElemTmp);     
  }
}


// ------------------------------------------------------------------------
// Element Event Handlers
// ------------------------------------------------------------------------

// Common event handler
bool gslc_ElemEvent(void* pvGui,gslc_tsEvent sEvent)
{
  if (pvGui == NULL) {
    debug_print("ERROR: ElemEvent(%s) called with NULL ptr\n","");
    return false;
  }
  gslc_tsGui*         pGui          = (gslc_tsGui*)(pvGui);
  void*               pvData        = sEvent.pvData;
  void*               pvScope       = sEvent.pvScope;  
  gslc_tsElem*        pElem         = NULL;
  gslc_tsElem*        pElemTracked  = NULL;
  gslc_tsEventTouch*  pTouchRec     = NULL;
  int                 nRelX,nRelY;
  gslc_teTouch        eTouch;
  GSLC_CB_TOUCH       pfuncXTouch   = NULL;
  
  switch(sEvent.eType) {
    case GSLC_EVT_DRAW:
      // Fetch the parameters      
      pElem = (gslc_tsElem*)(pvScope);
      if ((sEvent.nSubType == GSLC_EVTSUB_DRAW_FORCE) || (pElem->bNeedRedraw)) {
        // Call the function that invokes the callback
        return gslc_ElemDrawByRef(pGui,pElem);
      } else {
        return true;
      }
      break;
      
    case GSLC_EVT_TOUCH:
      // Fetch the parameters
      pTouchRec = (gslc_tsEventTouch*)(pvData);
      pElemTracked = (gslc_tsElem*)(pvScope);
      nRelX = pTouchRec->nX - pElemTracked->rElem.x;
      nRelY = pTouchRec->nY - pElemTracked->rElem.y;
      eTouch = pTouchRec->eTouch;
      pfuncXTouch = pElemTracked->pfuncXTouch;
      
      // Invoke the callback function
      if (pfuncXTouch != NULL) {
        // Pass in the relative position from corner of element region
        (*pfuncXTouch)(pvGui,(void*)(pElemTracked),eTouch,nRelX,nRelY);
      }
   
      break;
      
    case GSLC_EVT_TICK:
      // Fetch the parameters
      pElem = (gslc_tsElem*)(pvScope);
      
      // Invoke the callback function      
      if (pElem->pfuncXTick != NULL) {
        // TODO: Confirm that tick functions want pvScope
        (*pElem->pfuncXTick)(pvGui,(void*)(pElem));
        return true;
      }
      break;
      
    default:
      break;
  }
  return true;
}

// ------------------------------------------------------------------------
// Element Drawing Functions
// ------------------------------------------------------------------------


// Draw an element to the active display
// - Element is referenced by page ID and element ID
// - This routine is typically called by user code for custom
//   drawing callbacks
void gslc_ElemDraw(gslc_tsGui* pGui,int16_t nPageId,int16_t nElemId)
{
  gslc_tsElem* pElem = gslc_PageFindElemById(pGui,nPageId,nElemId);
  gslc_tsEvent sEvent = gslc_EventCreate(GSLC_EVT_DRAW,GSLC_EVTSUB_DRAW_FORCE,(void*)pElem,NULL);
  gslc_ElemEvent(pGui,sEvent);
}

// Draw an element to the active display
// - Element is referenced by an element pointer
// - TODO: Handle GSLC_TYPE_BKGND
bool gslc_ElemDrawByRef(gslc_tsGui* pGui,gslc_tsElem* pElem)
{
  if ((pGui == NULL) || (pElem == NULL)) {
    debug_print("ERROR: ElemDrawByRef(%s) called with NULL ptr\n","");
    return false;
  }    
  
  // --------------------------------------------------------------------------
  // Custom drawing
  // --------------------------------------------------------------------------
  
  // Handle any extended element types
  // - If the pfuncXDraw callback is defined, then let the callback
  //   function supersede all default handling here
  // - Note that the end of the callback function is expected
  //   to clear the redraw flag
  if (pElem->pfuncXDraw != NULL) {
    (*pElem->pfuncXDraw)((void*)(pGui),(void*)(pElem));
    return true;
  }  
  
  // --------------------------------------------------------------------------
  // Init for default drawing
  // --------------------------------------------------------------------------
  
  bool      bGlowEn,bGlowing,bGlowNow;
  int16_t   nElemX,nElemY;
  uint16_t  nElemW,nElemH;
  
  nElemX    = pElem->rElem.x;
  nElemY    = pElem->rElem.y;
  nElemW    = pElem->rElem.w;
  nElemH    = pElem->rElem.h;
  bGlowEn   = pElem->bGlowEn;     // Does the element support glow state?
  bGlowing  = pElem->bGlowing;    // Element should be glowing (if enabled)
  bGlowNow  = bGlowEn & bGlowing; // Element is currently glowing
  
  // --------------------------------------------------------------------------
  // Background
  // --------------------------------------------------------------------------
  
  // Fill in the background
  // - This also changes the fill color if selected and glow state is enabled
  if (pElem->bFillEn) {
    if (bGlowEn && bGlowing) {
      gslc_DrawFillRect(pGui,pElem->rElem,pElem->colElemFillGlow);
    } else {
      gslc_DrawFillRect(pGui,pElem->rElem,pElem->colElemFill);
    }
  } else {
    // TODO: If unfilled, then we might need
    // to redraw the background layer(s)
  }

  // --------------------------------------------------------------------------
  // Frame
  // --------------------------------------------------------------------------

  // Frame the region
  #ifdef DBG_FRAME
  // For debug purposes, draw a frame around every element
  gslc_DrawFrameRect(pGui,sElem.rElem,GSLC_COL_GRAY_DK);
  #else
  if (pElem->bFrameEn) {
    gslc_DrawFrameRect(pGui,pElem->rElem,pElem->colElemFrame);
  }
  #endif

  
  // --------------------------------------------------------------------------
  // Image overlays
  // --------------------------------------------------------------------------
  
  // Draw any images associated with element
  if (pElem->sImgRefNorm.eImgFlags != GSLC_IMGREF_NONE) {
    if ((bGlowEn && bGlowing) && (pElem->sImgRefGlow.eImgFlags != GSLC_IMGREF_NONE)) { 
      gslc_DrvDrawImage(pGui,nElemX,nElemY,pElem->sImgRefGlow);
    } else {
      gslc_DrvDrawImage(pGui,nElemX,nElemY,pElem->sImgRefNorm);
    }
  }

  // --------------------------------------------------------------------------
  // Text overlays
  // --------------------------------------------------------------------------
 
  // Overlay the text
  bool    bRenderTxt = true;
  if ((bRenderTxt) && (pElem->pStrBuf == NULL))     { bRenderTxt = false; }
  if ((bRenderTxt) && (pElem->pStrBuf[0] == '\0'))  { bRenderTxt = false; }  
  if (bRenderTxt) {
#if (DRV_HAS_DRAW_TEXT)    
    int16_t       nMargin   = pElem->nTxtMargin;

    // Determine the text color
    gslc_tsColor  colTxt    = (bGlowNow)? pElem->colElemTextGlow : pElem->colElemText;
  
    // Fetch the size of the text to allow for justification
    uint16_t      nTxtSzW,nTxtSzH;
    gslc_DrvGetTxtSize(pGui,pElem->pTxtFont,pElem->pStrBuf,&nTxtSzW,&nTxtSzH);
    
    // Calculate the text alignment
    int16_t       nTxtX,nTxtY;
    
    // Check for ALIGNH_LEFT & ALIGNH_RIGHT. Default to ALIGNH_MID
    if      (pElem->eTxtAlign & GSLC_ALIGNH_LEFT)     { nTxtX = nElemX+nMargin; }
    else if (pElem->eTxtAlign & GSLC_ALIGNH_RIGHT)    { nTxtX = nElemX+nElemW-nMargin-nTxtSzW; }
    else                                              { nTxtX = nElemX+(nElemW/2)-(nTxtSzW/2); }

    // Check for ALIGNV_TOP & ALIGNV_BOT. Default to ALIGNV_MID
    if      (pElem->eTxtAlign & GSLC_ALIGNV_TOP)      { nTxtY = nElemY+nMargin; }
    else if (pElem->eTxtAlign & GSLC_ALIGNV_BOT)      { nTxtY = nElemY+nElemH-nMargin-nTxtSzH; }
    else                                              { nTxtY = nElemY+(nElemH/2)-(nTxtSzH/2); }    

    // Call the driver text rendering routine
    gslc_DrvDrawTxt(pGui,nTxtX,nTxtY,pElem->pTxtFont,pElem->pStrBuf,colTxt);
    
#else
    // No text support in driver, so skip
#endif
  }

  // Mark the element as no longer requiring redraw
  gslc_ElemSetRedraw(pElem,false);
  
  return true;
}


// ------------------------------------------------------------------------
// Element Update Functions
// ------------------------------------------------------------------------

void gslc_ElemSetFillEn(gslc_tsElem* pElem,bool bFillEn)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetFillEn(%s) called with NULL ptr\n","");
    return;
  }
  pElem->bFillEn          = bFillEn;  
  gslc_ElemSetRedraw(pElem,true); 
}


void gslc_ElemSetFrameEn(gslc_tsElem* pElem,bool bFrameEn)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetFrameEn(%s) called with NULL ptr\n","");
    return;
  }
  pElem->bFrameEn         = bFrameEn;  
  gslc_ElemSetRedraw(pElem,true); 
}


void gslc_ElemSetCol(gslc_tsElem* pElem,gslc_tsColor colFrame,gslc_tsColor colFill,gslc_tsColor colFillGlow)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetCol(%s) called with NULL ptr\n","");
    return;
  }  
  pElem->colElemFrame     = colFrame;
  pElem->colElemFill      = colFill;
  pElem->colElemFillGlow      = colFillGlow;
  gslc_ElemSetRedraw(pElem,true); 
}

void gslc_ElemSetGlowCol(gslc_tsElem* pElem,gslc_tsColor colFrameGlow,gslc_tsColor colFillGlow,gslc_tsColor colTxtGlow)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetColGlow(%s) called with NULL ptr\n","");
    return;
  }  
  pElem->colElemFrameGlow   = colFrameGlow;
  pElem->colElemFillGlow    = colFillGlow;
  pElem->colElemTextGlow    = colTxtGlow;
  gslc_ElemSetRedraw(pElem,true); 
}

void gslc_ElemSetGroup(gslc_tsElem* pElem,int nGroupId)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetGroup(%s) called with NULL ptr\n","");
    return;
  }    
  pElem->nGroup           = nGroupId;
}

int gslc_ElemGetGroup(gslc_tsElem* pElem)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemGetGroup(%s) called with NULL ptr\n","");
    return GSLC_GROUP_ID_NONE;
  }    
  return pElem->nGroup;  
}


void gslc_ElemSetTxtAlign(gslc_tsElem* pElem,unsigned nAlign)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetTxtAlign(%s) called with NULL ptr\n","");
    return;
  }    
  pElem->eTxtAlign        = nAlign;
  gslc_ElemSetRedraw(pElem,true);  
}

void gslc_ElemSetTxtMargin(gslc_tsElem* pElem,unsigned nMargin)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetTxtMargin(%s) called with NULL ptr\n","");
    return;
  }    
  pElem->nTxtMargin        = nMargin;
  gslc_ElemSetRedraw(pElem,true);  
}

void gslc_ElemSetTxtStr(gslc_tsElem* pElem,const char* pStr)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetTxtStr(%s) called with NULL ptr\n","");
    return;
  }    
  // To avoid unnecessary redraw / flicker, only a change in
  // the text content will drive a redraw
  if (strncmp(pElem->pStrBuf,pStr,pElem->nStrBufMax-1)) {
    strncpy(pElem->pStrBuf,pStr,pElem->nStrBufMax-1);
    pElem->pStrBuf[pElem->nStrBufMax-1] = '\0';  // Force termination
    gslc_ElemSetRedraw(pElem,true);
  }
}

void gslc_ElemSetTxtCol(gslc_tsElem* pElem,gslc_tsColor colVal)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetTxtCol(%s) called with NULL ptr\n","");
    return;
  }    
  pElem->colElemText      = colVal;
  pElem->colElemTextGlow  = colVal; // Default to same color for glowing state
  gslc_ElemSetRedraw(pElem,true); 
}

void gslc_ElemUpdateFont(gslc_tsGui* pGui,gslc_tsElem* pElem,int nFontId)
{
  if ((pGui == NULL) || (pElem == NULL)) {
    debug_print("ERROR: ElemUpdateFont(%s) called with NULL ptr\n","");
    return;
  }
  pElem->pTxtFont = gslc_FontGet(pGui,nFontId);
  gslc_ElemSetRedraw(pElem,true);
}

void gslc_ElemSetRedraw(gslc_tsElem* pElem,bool bRedraw)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetRedraw(%s) called with NULL ptr\n","");
    return;
  }    
  pElem->bNeedRedraw      = bRedraw;
  
  // Now propagate up the element hierarchy
  // (eg. in case of compound elements)
  if (pElem->pElemParent != NULL) {
    gslc_ElemSetRedraw(pElem->pElemParent,bRedraw);
  }
}

void gslc_ElemSetGlow(gslc_tsElem* pElem,bool bGlowing)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetGlow(%s) called with NULL ptr\n","");
    return;
  }    
  pElem->bGlowing         = bGlowing;
  gslc_ElemSetRedraw(pElem,true);
}

bool gslc_ElemGetGlow(gslc_tsElem* pElem)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemGetGlow(%s) called with NULL ptr\n","");
    return false;
  }    
  return pElem->bGlowing;
}

void gslc_ElemSetGlowEn(gslc_tsElem* pElem,bool bGlowEn)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemSetGlowEn(%s) called with NULL ptr\n","");
    return;
  }    
  pElem->bGlowEn         = bGlowEn;
  gslc_ElemSetRedraw(pElem,true);
}

bool gslc_ElemGetGlowEn(gslc_tsElem* pElem)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemGetGlowEn(%s) called with NULL ptr\n","");
    return false;
  }    
  return pElem->bGlowEn;
}

void gslc_ElemSetStyleFrom(gslc_tsElem* pElemSrc,gslc_tsElem* pElemDest)
{
  if ((pElemSrc == NULL) || (pElemDest == NULL)) {
    debug_print("ERROR: ElemSetStyleFrom(%s) called with NULL ptr\n","");
    return;
  }

  // nId
  // nType
  // rElem
  pElemDest->nGroup           = pElemSrc->nGroup;
  // bValid
  pElemDest->bGlowEn          = pElemSrc->bGlowEn;
  pElemDest->bGlowing         = pElemSrc->bGlowing;
  pElemDest->sImgRefNorm      = pElemSrc->sImgRefNorm;
  pElemDest->sImgRefGlow      = pElemSrc->sImgRefGlow;
  
  pElemDest->bClickEn         = pElemSrc->bClickEn;
  pElemDest->bFrameEn         = pElemSrc->bFrameEn;
  pElemDest->bFillEn          = pElemSrc->bFillEn;
  
  pElemDest->colElemFill      = pElemSrc->colElemFill;
  pElemDest->colElemFillGlow  = pElemSrc->colElemFillGlow;
  pElemDest->colElemFrame     = pElemSrc->colElemFrame;
  pElemDest->colElemFrameGlow = pElemSrc->colElemFrameGlow;

  // bNeedRedraw

  pElemDest->pElemParent      = pElemSrc->pElemParent;

  // Don't copy over the text strings
  //  pStrBuf[GSLC_LOCAL_STR_LEN]
  //  pStr
  //  nStrMax
  
  pElemDest->colElemText      = pElemSrc->colElemText;
  pElemDest->colElemTextGlow  = pElemSrc->colElemTextGlow; 
  pElemDest->eTxtAlign        = pElemSrc->eTxtAlign;
  pElemDest->nTxtMargin       = pElemSrc->nTxtMargin;
  pElemDest->pTxtFont         = pElemSrc->pTxtFont;

  // pXData
  
  pElemDest->pfuncXEvent      = pElemSrc->pfuncXEvent;
  pElemDest->pfuncXDraw       = pElemSrc->pfuncXDraw;
  pElemDest->pfuncXTouch      = pElemSrc->pfuncXTouch;
  pElemDest->pfuncXTick       = pElemSrc->pfuncXTick;
   
  gslc_ElemSetRedraw(pElemDest,true); 
}

void gslc_ElemSetEventFunc(gslc_tsElem* pElem,GSLC_CB_EVENT funcCb)
{
  if ((pElem == NULL) || (funcCb == NULL)) {
    debug_print("ERROR: ElemSetEventFunc(%s) called with NULL ptr\n","");
    return;
  }    
  pElem->pfuncXEvent       = funcCb;
}


void gslc_ElemSetDrawFunc(gslc_tsElem* pElem,GSLC_CB_DRAW funcCb)
{
  if ((pElem == NULL) || (funcCb == NULL)) {
    debug_print("ERROR: ElemSetDrawFunc(%s) called with NULL ptr\n","");
    return;
  }    
  pElem->pfuncXDraw       = funcCb;
  gslc_ElemSetRedraw(pElem,true);   
}

void gslc_ElemSetTickFunc(gslc_tsElem* pElem,GSLC_CB_TICK funcCb)
{
  if ((pElem == NULL) || (funcCb == NULL)) {
    debug_print("ERROR: ElemSetTickFunc(%s) called with NULL ptr\n","");
    return;
  }    
  pElem->pfuncXTick       = funcCb; 
}

bool gslc_ElemOwnsCoord(gslc_tsElem* pElem,int16_t nX,int16_t nY,bool bOnlyClickEn)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemOwnsCoord(%s) called with NULL ptr\n","");
    return false;
  }
  if (bOnlyClickEn && !pElem->bClickEn) {
    return false;
  }
  return gslc_IsInRect(nX,nY,pElem->rElem);
}


// ------------------------------------------------------------------------
// Tracking Functions
// ------------------------------------------------------------------------


void gslc_CollectTouch(gslc_tsGui* pGui,gslc_tsCollect* pCollect,gslc_tsEventTouch* pEventTouch)
{
  // Fetch the data members of the touch event
  int16_t       nX      = pEventTouch->nX;
  int16_t       nY      = pEventTouch->nY;
  gslc_teTouch  eTouch  = pEventTouch->eTouch;
  
  gslc_tsElem*  pTrackedOld = NULL;
  gslc_tsElem*  pTrackedNew = NULL;
  
  // Fetch the item currently being tracked (if any)
  pTrackedOld = gslc_CollectGetElemTracked(pCollect);
  
  // Reset the in-tracked flag
  bool  bInTracked = false;
  
  if (eTouch == GSLC_TOUCH_DOWN) {
    // ---------------------------------
    // Touch Down Event
    // ---------------------------------
    
    // End glow on previously tracked element (if any)
    // - We shouldn't really enter a "Touch Down" event
    //   with an element still marked as being tracked
    if (pTrackedOld != NULL) {
      gslc_ElemSetGlow(pTrackedOld,false);
    }
    
    // Determine the new element to start tracking
    pTrackedNew = gslc_CollectFindElemFromCoord(pCollect,nX,nY);
    
    if (pTrackedNew == NULL) {
      // Didn't find an element, so clear the tracking reference   
      gslc_CollectSetElemTracked(pCollect,NULL);
    } else {
      // Found an element, so mark it as being the tracked element

      // Set the new tracked element reference
      gslc_CollectSetElemTracked(pCollect,pTrackedNew);

      // Start glow on new element
      gslc_ElemSetGlow(pTrackedNew,true);
      
      // Notify element for optional custom handling
      // - We do this after we have determined which element should
      //   receive the touch tracking
      eTouch = GSLC_TOUCH_DOWN_IN;
      gslc_ElemSendEventTouch(pGui,pTrackedNew,eTouch,nX,nY);
 
    }
   
  } else if (eTouch == GSLC_TOUCH_UP) {
    // ---------------------------------
    // Touch Up Event
    // ---------------------------------

    if (pTrackedOld != NULL) {
      // Are we still over tracked element?
      bInTracked = gslc_ElemOwnsCoord(pTrackedOld,nX,nY,true);
  
      if (!bInTracked) {
        // Released not over tracked element
        eTouch = GSLC_TOUCH_UP_OUT;
        gslc_ElemSendEventTouch(pGui,pTrackedOld,eTouch,nX,nY);
      } else {
        // Notify original tracked element for optional custom handling
        eTouch = GSLC_TOUCH_UP_IN;        
        gslc_ElemSendEventTouch(pGui,pTrackedOld,eTouch,nX,nY);
      }

      // Clear glow state
      gslc_ElemSetGlow(pTrackedOld,false);      

    }

    // Clear the element tracking state
    gslc_CollectSetElemTracked(pCollect,NULL);
    
  } else if (eTouch == GSLC_TOUCH_MOVE) {
    // ---------------------------------
    // Touch Move Event
    // ---------------------------------
    
    // Determine if we are still over tracked element
    if (pTrackedOld != NULL) {
      bInTracked = gslc_ElemOwnsCoord(pTrackedOld,nX,nY,true);

      if (!bInTracked) {

        // Not currently over tracked element
        // - Notify tracked element that we moved out of it
        eTouch = GSLC_TOUCH_MOVE_OUT;
        gslc_ElemSendEventTouch(pGui,pTrackedOld,eTouch,nX,nY);

        // Ensure the tracked element is no longer glowing
        if (pTrackedOld) {
          gslc_ElemSetGlow(pTrackedOld,false);
        }
      } else {
        // We are still over tracked element
        // - Notify tracked element
        eTouch = GSLC_TOUCH_MOVE_IN;
        gslc_ElemSendEventTouch(pGui,pTrackedOld,eTouch,nX,nY);

        // Ensure it is glowing
        gslc_ElemSetGlow(pTrackedOld,true);
      }      
      
    }   

  }  
}


// This routine is responsible for the GUI-level touch event state machine
// and dispatching to the touch event handler for the page
void gslc_TrackTouch(gslc_tsGui* pGui,gslc_tsPage* pPage,int16_t nX,int16_t nY,uint16_t nPress)
{
  if ((pGui == NULL) || (pPage == NULL)) {
    debug_print("ERROR: TrackTouch(%s) called with NULL ptr\n","");
    return;
  }    

  // Determine the transitions in the touch events based
  // on the previous touch pressure state
  // TODO: Provide hysteresis thresholds for the touch
  //       pressure levels in case a display outputs a
  //       variable range (eg. 15..200) that doesn't
  //       go to zero when touch is removed.
  gslc_teTouch  eTouch = GSLC_TOUCH_NONE;
  if ((pGui->nTouchLastPress == 0) && (nPress > 0)) {
    eTouch = GSLC_TOUCH_DOWN;
    #ifdef DBG_TOUCH    
    debug_print(" TS : (%3d,%3d) Pressure=%3u : TouchDown\n",nX,nY,nPress);
    #endif
  } else if ((pGui->nTouchLastPress > 0) && (nPress == 0)) {
    eTouch = GSLC_TOUCH_UP;
    #ifdef DBG_TOUCH    
    debug_print(" TS : (%3d,%3d) Pressure=%3u : TouchUp\n",nX,nY,nPress);
    #endif
    
  } else if ((pGui->nTouchLastX != nX) || (pGui->nTouchLastY != nY)) {
    // We only track movement if touch is "down"
    if (nPress > 0) {
      eTouch = GSLC_TOUCH_MOVE;
      #ifdef DBG_TOUCH    
      debug_print(" TS : (%3d,%3d) Pressure=%3u : TouchMove\n",nX,nY,nPress);
      #endif
    }
  }
  
  gslc_tsEventTouch sEventTouch;
  sEventTouch.eTouch        = eTouch;
  sEventTouch.nX            = nX;
  sEventTouch.nY            = nY;
  void* pvData = (void*)(&sEventTouch);
  gslc_tsEvent sEvent = gslc_EventCreate(GSLC_EVT_TOUCH,0,(void*)pPage,pvData);
  gslc_PageEvent(pGui,sEvent);
  

  // Save raw touch status so that we can detect transitions
  pGui->nTouchLastX      = nX;
  pGui->nTouchLastY      = nY;
  pGui->nTouchLastPress  = nPress;
}



// ------------------------------------------------------------------------
// Touchscreen Functions
// ------------------------------------------------------------------------

bool gslc_InitTouch(gslc_tsGui* pGui,const char* acDev)
{
  if (pGui == NULL) {
    debug_print("ERROR: InitTouch(%s) called with NULL ptr\n","");
    return false;
  }
  
  // Call driver-specific touchscreen init
  //
  // Determine if touch events are provided by the display driver
  // or an external touch driver.
#if defined(DRV_TOUCH_NONE)
  // Touch handling disabled
  return true;
#elif defined(DRV_TOUCH_IN_DISP)  
  // Touch handling by display driver
  return gslc_DrvInitTouch(pGui,acDev);
#else
  // Touch handling by external touch driver
  return gslc_TDrvInitTouch(pGui,acDev);
#endif
  
}


bool gslc_GetTouch(gslc_tsGui* pGui,int16_t* pnX,int16_t* pnY,uint16_t* pnPress)
{
  if (pGui == NULL) {
    debug_print("ERROR: GetTouch(%s) called with NULL ptr\n","");
    return false;
  }    
  
#if defined(DRV_TOUCH_NONE)
  // Touch handling disabled
  return false;
#elif defined(DRV_TOUCH_IN_DISP)
  // Use display driver for touch events
  return gslc_DrvGetTouch(pGui,pnX,pnY,pnPress);  
#else
  // Use external touch driver for touch events
  return gslc_TDrvGetTouch(pGui,pnX,pnY,pnPress);
#endif
    
  return false;
}


// ------------------------------------------------------------------------
// Private Functions
// ------------------------------------------------------------------------


// NOTE: nId is a positive ID specified by the user or
//       GSLC_ID_AUTO if the user wants an auto-generated ID
//       (which will be assigned in Element nId)
// NOTE: When we are creating sub-elements within a compound element,
//       we usually pass nPageId=GSLC_PAGE_NONE. In this mode we
//       won't add the element to any page, but just create the
//       element struct. However, in this mode we can't support
//       auto-generated IDs since we don't know which IDs will
//       be taken when we finally create the compound element.
gslc_tsElem gslc_ElemCreate(gslc_tsGui* pGui,int16_t nElemId,int16_t nPageId,
  int16_t nType,gslc_tsRect rElem,char* pStrBuf,uint8_t nStrBufMax,int16_t nFontId)
{
  gslc_tsElem sElem;
  // Assign defaults to the element record
  gslc_ResetElem(&sElem);
  
  if (pGui == NULL) {
    debug_print("ERROR: ElemCreate(%s) called with NULL ptr\n","");
    return sElem;
  }  

  gslc_tsPage*    pPage = NULL;
  gslc_tsCollect* pCollect = NULL;
  
  // If we are going to be adding the element to a page then we
  // perform some additional checks
  if (nPageId == GSLC_PAGE_NONE) {
    // This is a temporary element, so we skip the ID collision checks.
    // In this mode we don't support auto ID assignment
    if (nElemId == GSLC_ID_AUTO) {
      debug_print("ERROR: ElemCreate(%s) doesn't support temp elements with auto ID\n","");
      return sElem;
    }
  } else {
    // Look up the targeted page to ensure that we check its
    // collection for collision with other IDs (or assign the
    // next available if auto-incremented)
    pPage = gslc_PageFindById(pGui,nPageId);
    if (pPage == NULL) {
      debug_print("ERROR: ElemCreate() can't find page (ID=%d)\n",nPageId);
      return sElem;
    }     
    pCollect  = &pPage->sCollect;
  
    // Validate the user-supplied ID
    if (nElemId == GSLC_ID_AUTO) {
      // Get next auto-generated ID
      nElemId = gslc_CollectGetNextId(pCollect);
    } else {
      // Ensure the ID is positive
      if (nElemId < 0) {
        debug_print("ERROR: ElemCreate() called with negative ID (%d)\n",nElemId);
        return sElem;
      }
      // Ensure the ID isn't already taken
      if (gslc_CollectFindElemById(pCollect,nElemId) != NULL) {
        debug_print("ERROR: ElemCreate() called with existing ID (%d)\n",nElemId);
        return sElem;
      }
    }
  }


  // Override defaults with parameterization
  sElem.nId             = nElemId;
  sElem.rElem           = rElem;
  sElem.nType           = nType;
  gslc_ElemUpdateFont(pGui,&sElem,nFontId);
 
  if (pStrBuf != NULL) {
    #if (GSLC_LOCAL_STR)
      strncpy(sElem.pStrBuf,pStrBuf,GSLC_LOCAL_STR_LEN-1);
      sElem.pStrBuf[GSLC_LOCAL_STR_LEN-1] = '\0';  // Force termination    
      sElem.nStrBufMax = GSLC_LOCAL_STR_LEN;
    #else
      // No need to copy locally; instead, we are going to retain
      // the external string pointer (must be static)
      sElem.pStrBuf         = pStrBuf;
      sElem.nStrBufMax      = nStrBufMax;
    #endif
  }  
  
  // TODO:
  // - Save pCollect in element?
  //   - This would facilitate any group operations (eg. checkbox)
  //   - Alternately, include pGui in parameters to gslc_ElemXCheckboxToggleState().
  
  // If the element creation was successful, then set the valid flag
  sElem.bValid          = true;

  return sElem;
}

// ------------------------------------------------------------------------
// Collect Event Handlers
// ------------------------------------------------------------------------


// Common event handler
bool gslc_CollectEvent(void* pvGui,gslc_tsEvent sEvent)
{
  if (pvGui == NULL) {
    debug_print("ERROR: CollectEvent(%s) called with NULL ptr\n","");
    return false;
  }
  gslc_tsGui*     pGui      = (gslc_tsGui*)(pvGui);
  void*           pvScope   = sEvent.pvScope;
  void*           pvData    = sEvent.pvData;
  gslc_tsCollect* pCollect  = (gslc_tsCollect*)(pvScope);

  unsigned        nInd;
  gslc_tsElem*    pElem = NULL;
  
  // Handle any collection-based events first
  // ...
  if (sEvent.eType == GSLC_EVT_TOUCH) {
    // TOUCH is passed to CollectTouch which determines the element
    // in the collection that should receive the event
    gslc_tsEventTouch* pEventTouch = (gslc_tsEventTouch*)(pvData);
    gslc_CollectTouch(pGui,pCollect,pEventTouch);    
    return true;
  
  } else if ( (sEvent.eType == GSLC_EVT_DRAW) || (sEvent.eType == GSLC_EVT_TICK) ) {
    // DRAW and TICK are propagated down to all elements in collection
    for (nInd=GSLC_IND_FIRST;nInd<pCollect->nElemCnt;nInd++) {
      pElem = &(pCollect->asElem[nInd]); 
      // Copy event so we can modify it in the loop
      gslc_tsEvent sEventNew = sEvent;
      // Update event data reference & propagate
      sEventNew.pvScope = (void*)(pElem);
      gslc_ElemEvent(pvGui,sEventNew);      
    } // nInd

  } // eType

  return true;
}



// Helper function for gslc_ElemAdd()
// - Note that this copies the content of pElem to collection element array
//   so the pointer can be released after the call
gslc_tsElem* gslc_CollectElemAdd(gslc_tsCollect* pCollect,gslc_tsElem* pElem)
{
  if ((pCollect == NULL) || (pElem == NULL)) {
    debug_print("ERROR: CollectElemAdd(%s) called with NULL ptr\n","");
    return NULL;
  }    
  
  if (pCollect->nElemCnt+1 >= (pCollect->nElemMax)) {
    debug_print("ERROR: CollectElemAdd(%s) too many elements\n","");
    return NULL;
  }
  // In case the element creation failed, trap that here
  if (!pElem->bValid) {
    debug_print("ERROR: CollectElemAdd(%s) skipping add of invalid element\n","");
    return NULL;
  }
  // Add the element to the internal array
  // - This performs a copy so that we can discard the element
  //   pointer after the call is complete
  uint16_t nElemInd = pCollect->nElemCnt;  
  pCollect->asElem[nElemInd] = *pElem;
  pCollect->nElemCnt++;

  return &(pCollect->asElem[nElemInd]);    
}


// Add an element to the collection associated with the page
//
// NOTE: The content (not address) of pElem is copied so the pointer
//       can be released after the call.
gslc_tsElem* gslc_ElemAdd(gslc_tsGui* pGui,int16_t nPageId,gslc_tsElem* pElem)
{
  if ((pGui == NULL) || (pElem == NULL)) {
    debug_print("ERROR: ElemAdd(%s) called with NULL ptr\n","");
    return NULL;
  }    

  // Fetch the page containing the item
  gslc_tsPage* pPage = gslc_PageFindById(pGui,nPageId);
  if (pPage == NULL) {
    debug_print("ERROR: ElemAdd() page (ID=%d) was not found\n",nPageId);
    return NULL;
  }   
  
  gslc_tsCollect* pCollect = &pPage->sCollect;
  
  return gslc_CollectElemAdd(pCollect,pElem);
}

bool gslc_SetClipRect(gslc_tsGui* pGui,gslc_tsRect* pRect)
{
  // Update the drawing clip rectangle
  if (pRect == NULL) {
    // Set to full size of screen
    return gslc_DrvSetClipRect(pGui,NULL);
  } else {
    // Set to user-specified region    
    return gslc_DrvSetClipRect(pGui,pRect);
  }
}


void gslc_ElemSetImage(gslc_tsGui* pGui,gslc_tsElem* pElem,gslc_tsImgRef sImgRef,
  gslc_tsImgRef sImgRefSel)
{
  if ((pGui == NULL) || (pElem == NULL)) {
    debug_print("ERROR: ElemSetImage(%s) called with NULL ptr\n","");
    return;
  }    

  // Update the normal and glowing images
  gslc_DrvSetElemImageNorm(pGui,pElem,sImgRef);
  gslc_DrvSetElemImageGlow(pGui,pElem,sImgRefSel);    
}

bool gslc_SetBkgndImage(gslc_tsGui* pGui,gslc_tsImgRef sImgRef)
{
  if (!gslc_DrvSetBkgndImage(pGui,sImgRef)) {
    return false;
  }
  gslc_PageFlipSet(pGui,true);
  return true;
}

bool gslc_SetBkgndColor(gslc_tsGui* pGui,gslc_tsColor nCol)
{
  if (!gslc_DrvSetBkgndColor(pGui,nCol)) {
    return false;
  }
  gslc_PageFlipSet(pGui,true);
  return true;  
}


// Trigger a touch event on an element
bool gslc_ElemSendEventTouch(gslc_tsGui* pGui,gslc_tsElem* pElemTracked,
        gslc_teTouch eTouch,int16_t nX,int16_t nY)
{
  gslc_tsEventTouch sEventTouch;
  sEventTouch.eTouch        = eTouch;
  sEventTouch.nX            = nX;
  sEventTouch.nY            = nY;
  gslc_tsEvent sEvent = gslc_EventCreate(GSLC_EVT_TOUCH,0,(void*)pElemTracked,&sEventTouch);
  gslc_ElemEvent((void*)pGui,sEvent);  
  return true;
}


// Initialize the element struct to all zeros
void gslc_ResetElem(gslc_tsElem* pElem)
{
  if (pElem == NULL) {
    debug_print("ERROR: ResetElem(%s) called with NULL ptr\n","");
    return;
  }  
  pElem->bValid           = false;
  pElem->nId              = GSLC_ID_NONE;
  pElem->nType            = GSLC_TYPE_BOX;
  pElem->nGroup           = GSLC_GROUP_ID_NONE;
  pElem->rElem            = (gslc_tsRect){0,0,0,0};
  pElem->bGlowEn          = false;
  pElem->bGlowing         = false;
  pElem->sImgRefNorm      = gslc_ResetImage();
  pElem->sImgRefGlow      = gslc_ResetImage();
  pElem->bClickEn         = false;
  pElem->bFrameEn         = false;
  pElem->bFillEn          = false;
  pElem->bNeedRedraw      = true;
  pElem->colElemFrame     = GSLC_COL_WHITE;
  pElem->colElemFill      = GSLC_COL_WHITE;
  pElem->colElemFrameGlow = GSLC_COL_WHITE;  
  pElem->colElemFillGlow  = GSLC_COL_WHITE;
  #if (GSLC_LOCAL_STR)
    pElem->pStrBuf[0]       = '\0';
    pElem->nStrBufMax       = 0;    
  #else
    pElem->pStrBuf          = NULL;
    pElem->nStrBufMax       = 0;
  #endif
  pElem->colElemText      = GSLC_COL_WHITE;
  pElem->colElemTextGlow  = GSLC_COL_WHITE;  
  pElem->eTxtAlign        = GSLC_ALIGN_MID_MID;
  pElem->nTxtMargin       = 0;
  pElem->pTxtFont         = NULL;
  
  pElem->pXData           = NULL;
  pElem->pfuncXEvent      = NULL;
  pElem->pfuncXDraw       = NULL;
  pElem->pfuncXTouch      = NULL;
  pElem->pfuncXTick       = NULL;
  
  pElem->pElemParent      = NULL;

}

// Initialize the font struct to all zeros
void gslc_ResetFont(gslc_tsFont* pFont)
{
  if (pFont == NULL) {
    debug_print("ERROR: ResetFont(%s) called with NULL ptr\n","");
    return;
  }    
  pFont->nId    = GSLC_FONT_NONE;
  pFont->pvFont = NULL;
  pFont->nSize  = 0;
}


// Close down an element
void gslc_ElemDestruct(gslc_tsElem* pElem)
{
  if (pElem == NULL) {
    debug_print("ERROR: ElemDestruct(%s) called with NULL ptr\n","");
    return;
  }    
  if (pElem->sImgRefNorm.pvImgRaw != NULL) {
    gslc_DrvImageDestruct(pElem->sImgRefNorm.pvImgRaw);
    pElem->sImgRefNorm = gslc_ResetImage();
  }
  if (pElem->sImgRefGlow.pvImgRaw != NULL) {
    gslc_DrvImageDestruct(pElem->sImgRefGlow.pvImgRaw);
    pElem->sImgRefGlow = gslc_ResetImage();  
  }
  
  // TODO: Add callback function so that
  // we can support additional closure actions
  // (eg. closing sub-elements of compound element).
  
}


// Close down a collection
void gslc_CollectDestruct(gslc_tsCollect* pCollect)
{
  if (pCollect == NULL) {
    debug_print("ERROR: CollectDestruct(%s) called with NULL ptr\n","");
    return;
  }
  uint16_t      nElemInd;
  gslc_tsElem*  pElem = NULL;

  for (nElemInd=0;nElemInd<pCollect->nElemCnt;nElemInd++) {
    pElem = &pCollect->asElem[nElemInd];
    gslc_ElemDestruct(pElem);
  }
    
}

// Close down all in page
void gslc_PageDestruct(gslc_tsPage* pPage)
{
  if (pPage == NULL) {
    debug_print("ERROR: PageDestruct(%s) called with NULL ptr\n","");
    return;
  }      
  gslc_tsCollect* pCollect = &pPage->sCollect;
  gslc_CollectDestruct(pCollect);
}

// Close down all GUI members, including pages and fonts
void gslc_GuiDestruct(gslc_tsGui* pGui)
{
  if (pGui == NULL) {
    debug_print("ERROR: GuiDestruct(%s) called with NULL ptr\n","");
    return;
  }    
  // Loop through all pages in GUI
  uint8_t nPageInd;
  gslc_tsPage*  pPage = NULL;
  for (nPageInd=0;nPageInd<pGui->nPageCnt;nPageInd++) {
    pPage = &pGui->asPage[nPageInd];
    gslc_PageDestruct(pPage);
  }
  
  // TODO: Consider moving into main element array
  if (pGui->sImgRefBkgnd.eImgFlags != GSLC_IMGREF_NONE) {
    gslc_DrvImageDestruct(pGui->sImgRefBkgnd.pvImgRaw);
    pGui->sImgRefBkgnd = gslc_ResetImage();
  }
  
  // Close all fonts
  gslc_DrvFontsDestruct(pGui);
  
  // Close any driver-specific data
  gslc_DrvDestruct(pGui);  

}


// ================================
// Private: Element Collection

void gslc_CollectReset(gslc_tsCollect* pCollect,gslc_tsElem* asElem,uint16_t nElemMax)
{
  if (pCollect == NULL) {
    debug_print("ERROR: CollectReset(%s) called with NULL ptr\n","");
    return;
  }  
  
  pCollect->nElemMax          = nElemMax;
  pCollect->nElemCnt          = GSLC_IND_FIRST;
  
  pCollect->nElemAutoIdNext   = GSLC_ID_AUTO_BASE;
  
  pCollect->pElemTracked      = NULL;
  pCollect->pfuncXEvent       = NULL;
  
  // Save the pointer to the element array
  pCollect->asElem = asElem;
  
  uint16_t nInd;
  for (nInd=0;nInd<nElemMax;nInd++) {
    gslc_ResetElem(&(pCollect->asElem[nInd]));
  }
}


gslc_tsElem* gslc_CollectFindElemById(gslc_tsCollect* pCollect,int16_t nElemId)
{
  if (pCollect == NULL) {
    debug_print("ERROR: CollectFindElemById(%s) called with NULL ptr\n","");
    return NULL;
  }  
  gslc_tsElem*  pFoundElem = NULL;
  uint16_t      nInd;
  if (nElemId == GSLC_ID_TEMP) {
    // ERROR: Don't expect to do this
    debug_print("ERROR: CollectFindElemById(%s) searching for temp ID\n","");    
    return NULL;
  }
  for (nInd=GSLC_IND_FIRST;nInd<pCollect->nElemCnt;nInd++) {
    if (pCollect->asElem[nInd].nId == nElemId) {
      pFoundElem = &(pCollect->asElem[nInd]);
      break;
    }
  }
  return pFoundElem;
}

int gslc_CollectGetNextId(gslc_tsCollect* pCollect)
{
  int16_t nElemId = pCollect->nElemAutoIdNext;
  pCollect->nElemAutoIdNext++;
  return nElemId;
}

gslc_tsElem* gslc_CollectGetElemTracked(gslc_tsCollect* pCollect)
{
  return pCollect->pElemTracked;
}

void gslc_CollectSetElemTracked(gslc_tsCollect* pCollect,gslc_tsElem* pElem)
{
  pCollect->pElemTracked = pElem;
}

// Find an element index in a collection from a coordinate
gslc_tsElem* gslc_CollectFindElemFromCoord(gslc_tsCollect* pCollect,int16_t nX, int16_t nY)
{
  uint16_t      nInd;
  bool          bFound = false;
  gslc_tsElem*  pElem = NULL;
  gslc_tsElem*  pFoundElem = NULL;


  for (nInd=GSLC_IND_FIRST;nInd<pCollect->nElemCnt;nInd++) {
    pElem = &pCollect->asElem[nInd];
    bFound = gslc_ElemOwnsCoord(pElem,nX,nY,true);
    if (bFound) {
      pFoundElem = pElem;
      // Stop searching
      break;
    }
  }
  
  // Return pointer or NULL if none found
  return pFoundElem;
}

// Go through all elements in a collection and set the parent
// element pointer.
void gslc_CollectSetParent(gslc_tsCollect* pCollect,gslc_tsElem* pElemParent)
{
  gslc_tsElem*  pElem = NULL;
  uint16_t      nInd;
  for (nInd=GSLC_IND_FIRST;nInd<pCollect->nElemCnt;nInd++) {
    pElem = &pCollect->asElem[nInd];
    pElem->pElemParent = pElemParent;
  }
}

void gslc_CollectSetEventFunc(gslc_tsCollect* pCollect,GSLC_CB_EVENT funcCb)
{
  if ((pCollect == NULL) || (funcCb == NULL)) {
    debug_print("ERROR: CollectSetEventFunc(%s) called with NULL ptr\n","");
    return;
  }    
  pCollect->pfuncXEvent       = funcCb;
}