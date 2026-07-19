static long GetMaxScrollPixels(void)
{
    LongRect viewRect;
    long viewHeight, totalHeight, maxScroll;
    
    if (!gActiveTE) return 0;
    
    WEGetViewRect(&viewRect, gActiveTE);
    viewHeight = viewRect.bottom - viewRect.top;
    totalHeight = WEGetHeight(0, WEGetLineCount(gActiveTE), gActiveTE);
    
    maxScroll = totalHeight - viewHeight;
    if (maxScroll < 0) maxScroll = 0;
    return maxScroll;
}
