/*******************************************************

   CoolReader Engine 

   lvdrawbuf.cpp:  Gray bitmap buffer class

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of
   GNU General Public License

   See LICENSE file for details

*******************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/lvdrawbuf.h"


static void ApplyAlphaRGB( lUInt32 &dst, lUInt32 src, lUInt32 alpha )
{
    if ( alpha==0 )
        dst = src;
    else if ( alpha<255 ) {
        src &= 0xFFFFFF;
        lUInt32 opaque = 256 - alpha;
        lUInt32 n1 = (((dst & 0xFF00FF) * alpha + (src & 0xFF00FF) * opaque) >> 8) & 0xFF00FF;
        lUInt32 n2 = (((dst & 0x00FF00) * alpha + (src & 0x00FF00) * opaque) >> 8) & 0x00FF00;
        dst = n1 | n2;
    }
}

static const short dither_2bpp_4x4[] = {
    5, 13,  8,  16,
    9,  1,  12,  4,
    7, 15,  6,  14,
    11, 3,  10,  2,
};

static const short dither_2bpp_8x8[] = {
0, 32, 12, 44, 2, 34, 14, 46, 
48, 16, 60, 28, 50, 18, 62, 30, 
8, 40, 4, 36, 10, 42, 6, 38, 
56, 24, 52, 20, 58, 26, 54, 22, 
3, 35, 15, 47, 1, 33, 13, 45, 
51, 19, 63, 31, 49, 17, 61, 29, 
11, 43, 7, 39, 9, 41, 5, 37, 
59, 27, 55, 23, 57, 25, 53, 21, 
};

lUInt32 Dither2BitColor( lUInt32 color, lUInt32 x, lUInt32 y )
{
    int cl = ((((color>>16) & 255) + ((color>>8) & 255) + ((color) & 255)) * (256/3)) >> 8;
    if (cl<5)
        return 0;
    else if (cl>=250)
        return 3;
    //int d = dither_2bpp_4x4[(x&3) | ( (y&3) << 2 )] - 1;
    int d = dither_2bpp_8x8[(x&7) | ( (y&7) << 3 )] - 1;

    cl = ( cl + d - 32 );
    if (cl<5)
        return 0;
    else if (cl>=250)
        return 3;
    return (cl >> 6) & 3;
}

lUInt32 Dither1BitColor( lUInt32 color, lUInt32 x, lUInt32 y )
{
    int cl = ((((color>>16) & 255) + ((color>>8) & 255) + ((color) & 255)) * (256/3)) >> 8;
    if (cl<16)
        return 0;
    else if (cl>=240)
        return 1;
    //int d = dither_2bpp_4x4[(x&3) | ( (y&3) << 2 )] - 1;
    int d = dither_2bpp_8x8[(x&7) | ( (y&7) << 3 )] - 1;

    cl = ( cl + d - 32 );
    if (cl<5)
        return 0;
    else if (cl>=250)
        return 1;
    return (cl >> 7) & 1;
}


class LVImageScaledDrawCallback : public LVImageDecoderCallback
{
private:
    LVImageSourceRef src;
    LVBaseDrawBuf * dst;
    int dst_x;
    int dst_y;
    int dst_dx;
    int dst_dy;
    int src_dx;
    int src_dy;
    int * xmap;
    int * ymap;
public:
    static int * GenMap( int src_len, int dst_len )
    {
        int  * map = new int[ dst_len ];
        for (int i=0; i<dst_len; i++)
        {
            map[ i ] = i * src_len / dst_len;
        }
        return map;
    }
    LVImageScaledDrawCallback(LVBaseDrawBuf * dstbuf, LVImageSourceRef img, int x, int y, int width, int height )
    : src(img), dst(dstbuf), dst_x(x), dst_y(y), dst_dx(width), dst_dy(height), xmap(0), ymap(0)
    {
        src_dx = img->GetWidth();
        src_dy = img->GetHeight();
        if ( src_dx != dst_dx )
            xmap = GenMap( src_dx, dst_dx );
        if ( src_dy != dst_dy )
            ymap = GenMap( src_dy, dst_dy );
    }
    virtual ~LVImageScaledDrawCallback()
    {
        if (xmap)
            delete[] xmap;
        if (ymap)
            delete[] ymap;
    }
    virtual void OnStartDecode( LVImageSource * obj )
    {
    }
    virtual bool OnLineDecoded( LVImageSource * obj, int y, lUInt32 * data )
    {
        //fprintf( stderr, "l_%d ", y );
        int yy = y;
        int yy2 = y+1;
        if ( ymap ) 
        {
            int yy0 = (y - 1) * dst_dy / src_dy;
            yy = y * dst_dy / src_dy;
            yy2 = (y+1) * dst_dy / src_dy;
            if ( yy == yy0 )
            {
                //fprintf( stderr, "skip_dup " );
                //return true; // skip duplicate drawing
            }
            if ( yy2 > dst_dy )
                yy2 = dst_dy;
        }
        lvRect clip;
        dst->GetClipRect( &clip );
        for ( ;yy<yy2; yy++ )
        {
            if ( yy+dst_y<clip.top || yy+dst_y>=clip.bottom )
                continue;
            if ( dst->GetBitsPerPixel() >= 24 )
            {
                lUInt32 * row = (lUInt32 *)dst->GetScanLine( yy + dst_y );
                row += dst_x;
                for (int x=0; x<dst_dx; x++)
                {
                    lUInt32 cl = data[xmap ? xmap[x] : x];
                    int xx = x + dst_x;
                    lUInt32 alpha = (cl >> 24)&0xFF;
                    if ( xx<clip.left || xx>=clip.right || alpha==0xFF )
                        continue;
                    if ( !alpha )
                        row[ x ] = cl;
                    else
                        ApplyAlphaRGB( row[x], cl, alpha );
                }
            }
            else if ( dst->GetBitsPerPixel() == 2 )
            {
                //fprintf( stderr, "." );
                lUInt8 * row = (lUInt8 *)dst->GetScanLine( yy+dst_y );
                //row += dst_x;
                for (int x=0; x<dst_dx; x++)
                {
                    lUInt32 cl = data[xmap ? xmap[x] : x];
                    int xx = x + dst_x;
                    lUInt32 alpha = (cl >> 24)&0xFF;
                    if ( xx<clip.left || xx>=clip.right || alpha&0x80 )
                        continue;
                    lUInt32 dcl = Dither2BitColor( cl, x, yy ) ^ 3;
                    int byteindex = (xx >> 2);
                    int bitindex = (3-(xx & 3))<<1;
                    lUInt8 mask = 0xC0 >> (6 - bitindex);
                    dcl = dcl << bitindex;
                    row[ byteindex ] = (lUInt8)((row[ byteindex ] & (~mask)) | dcl);
                }
            }
            else if ( dst->GetBitsPerPixel() == 1 )
            {
                //fprintf( stderr, "." );
                lUInt8 * row = (lUInt8 *)dst->GetScanLine( yy+dst_y );
                //row += dst_x;
                for (int x=0; x<dst_dx; x++)
                {
                    lUInt32 cl = data[xmap ? xmap[x] : x];
                    int xx = x + dst_x;
                    lUInt32 alpha = (cl >> 24)&0xFF;
                    if ( xx<clip.left || xx>=clip.right || alpha&0x80 )
                        continue;
                    lUInt32 dcl = Dither1BitColor( cl, x, yy ) ^ 1;
                    int byteindex = (xx >> 3);
                    int bitindex = ((xx & 7));
                    lUInt8 mask = 0x80 >> (bitindex);
                    dcl = dcl << (7-bitindex);
                    row[ byteindex ] = (lUInt8)((row[ byteindex ] & (~mask)) | dcl);
                }
            }
            else
            {
                return false;
            }
        }
        return true;
    }
    virtual void OnEndDecode( LVImageSource * obj, bool errors )
    {
    }
};


int  LVBaseDrawBuf::GetWidth()
{ 
    return _dx;
}

int  LVBaseDrawBuf::GetHeight()
{ 
    return _dy; 
}

int  LVGrayDrawBuf::GetBitsPerPixel()
{ 
    return _bpp;
}

void LVGrayDrawBuf::Draw( LVImageSourceRef img, int x, int y, int width, int height )
{
    //fprintf( stderr, "LVGrayDrawBuf::Draw( img(%d, %d), %d, %d, %d, %d\n", img->GetWidth(), img->GetHeight(), x, y, width, height );
    LVImageScaledDrawCallback drawcb( this, img, x, y, width, height );
    img->Decode( &drawcb );
}


/// get pixel value
lUInt32 LVGrayDrawBuf::GetPixel( int x, int y )
{
    if (x<0 || y<0 || x>=_dx || y>=_dy)
        return 0;
    lUInt8 * line = GetScanLine(y);
    if (_bpp==1)
    {
        // 1bpp
        if ( line[x>>3] & (0x80>>(x&7)) )
            return 1;
        else
            return 0;
    }
    else // 2bpp
    {
        if (x<8)
            return x/2;
        return (line[x>>2] >> (6-((x&3)<<1))) & 3;
    }
}

void LVGrayDrawBuf::Clear( lUInt32 color )
{
    if (_bpp==1)
    {
        color = (color&1) ? 0xFF : 0x00;
    }
    else
    {
        color &= 3;
        color |= (color << 2) | (color << 4) | (color << 6);
    }
    for (int i = _rowsize * _dy - 1; i>0; i--)
    {
        _data[i] = (lUInt8)color;
    }
    SetClipRect( NULL );
}

void LVGrayDrawBuf::FillRect( int x0, int y0, int x1, int y1, lUInt32 color )
{
    if (x0<_clip.left)
        x0 = _clip.left;
    if (y0<_clip.top)
        y0 = _clip.top;
    if (x1>_clip.right)
        x1 = _clip.right;
    if (y1>_clip.bottom)
        y1 = _clip.bottom;
    if (x0>=x1 || y0>=y1)
        return;
    if (_bpp==1)
    {
        color = (color&1) ? 0xFF : 0x00;
    }
    else
    {
        color &= 3;
        color |= (color << 2) | (color << 4) | (color << 6);
    }
    lUInt8 * line = GetScanLine(y0);
    for (int y=y0; y<y1; y++)
    {
        if (_bpp==1)
        {
            for (int x=x0; x<x1; x++)
            {
                lUInt8 mask = 0x80 >> (x&7);
                int index = x >> 3;
                line[index] = (lUInt8)((line[index] & ~mask) | (color & mask));
            }
        }
        else
        {
            for (int x=x0; x<x1; x++)
            {
                lUInt8 mask = 0xC0 >> ((x&3)<<1);
                int index = x >> 2;
                line[index] = (lUInt8)((line[index] & ~mask) | (color & mask));
            }
        }
        line += _rowsize;
    }
}

void LVGrayDrawBuf::Resize( int dx, int dy )
{
    _dx = dx;
    _dy = dy;
    _rowsize = (_dx * _bpp + 7) / 8;
    if ( dx && dy )
    {
        _data = (lUInt8 *) realloc(_data, _rowsize * _dy);
    }
    else if (_data) 
    {
        free(_data);
        _data = NULL;
    }
    Clear(0);
    SetClipRect( NULL );
}

LVGrayDrawBuf::LVGrayDrawBuf(int dx, int dy, int bpp)
    : LVBaseDrawBuf(), _bpp(bpp)
{
    _dx = dx;
    _dy = dy;
    _bpp = bpp;
    _rowsize = (_dx * _bpp + 7) / 8;
    if (_dx && _dy)
    {
        _data = (lUInt8 *) malloc(_rowsize * _dy);
        Clear(0);
    }
    SetClipRect( NULL );
}

LVGrayDrawBuf::~LVGrayDrawBuf()
{
    if (_data)
        free( _data );
}

void LVGrayDrawBuf::Draw( int x, int y, const lUInt8 * bitmap, int width, int height, lUInt32 * palette )
{
    //int buf_width = _dx; /* 2bpp */
    int bx = 0;
    int by = 0;
    int xx;
    int bmp_width = width;
    lUInt8 * dst;
    lUInt8 * dstline;
    const lUInt8 * src;
    int      shift, shift0;

    if (x<_clip.left)
    {
        width += x-_clip.left;
        bx -= x-_clip.left;
        x = _clip.left;
        if (width<=0)
            return;
    }
    if (y<_clip.top)
    {
        height += y-_clip.top;
        by -= y-_clip.top;
        y = _clip.top;
        if (height<=0)
            return;
    }
    if (x + width > _clip.right)
    {
        width = _clip.right - x;
    }
    if (width<=0)
        return;
    if (y + height > _clip.bottom)
    {
        height = _clip.bottom - y;
    }
    if (height<=0)
        return;

    int bytesPerRow = _rowsize;
    if ( _bpp==2 ) {
        dstline = _data + bytesPerRow*y + (x >> 2);
        shift0 = (x & 3);
    } else {
        dstline = _data + bytesPerRow*y + (x >> 3);
        shift0 = (x & 7);
    }
    dst = dstline;
    xx = width;

    bitmap += bx + by*bmp_width;
    shift = shift0;

    for (;height;height--)
    {
        src = bitmap;

        if ( _bpp==2 ) {
            for (xx = width; xx>0; --xx)
            {
                *dst |= (( (*src++) & 0xC0 ) >> ( shift << 1 ));
                /* next pixel */
                if (!(++shift & 3))
                {
                    shift = 0;
                    dst++;
                }
            }
        } else if ( _bpp==1 ) {
            for (xx = width; xx>0; --xx)
            {
                *dst |= (( (*src++) & 0x80 ) >> ( shift ));
                /* next pixel */
                if (!(++shift & 7))
                {
                    shift = 0;
                    dst++;
                }
            }
        }
        /* new dest line */
        bitmap += bmp_width;
        dstline += bytesPerRow;
        dst = dstline;
        shift = shift0;
    }
}

void LVBaseDrawBuf::SetClipRect( lvRect * clipRect )
{
    if (clipRect)
    {
        _clip = *clipRect;
        if (_clip.left<0)
            _clip.left = 0;
        if (_clip.top<0)
            _clip.top = 0;
        if (_clip.right>_dx)
            _clip.right = _dx;
        if (_clip.bottom > _dy)
            _clip.bottom = _dy;
    }
    else
    {
        _clip.top = 0;
        _clip.left = 0;
        _clip.right = _dx;
        _clip.bottom = _dy;
    }
}

lUInt8 * LVGrayDrawBuf::GetScanLine( int y )
{
    return _data + _rowsize*y;
}

void LVGrayDrawBuf::Invert()
{
    int sz = _rowsize * _dy;
    for (int i=sz-1; i>=0; i--)
        _data[i] = ~_data[i];
}

void LVGrayDrawBuf::ConvertToBitmap(bool flgDither)
{
    if (_bpp==1)
        return;
    int sz = (_dx+7)/8*_dy;
    lUInt8 * bitmap = (lUInt8*) malloc( sizeof(lUInt8) * sz );
    memset( bitmap, 0, sz );
    if (flgDither)
    {
        static const lUInt8 cmap[4][4] = {
            { 0, 0, 0, 0},
            { 0, 0, 1, 0},
            { 0, 1, 0, 1},
            { 1, 1, 1, 1},
        };
        for (int y=0; y<_dy; y++)
        {
            lUInt8 * src = GetScanLine(y);
            lUInt8 * dst = bitmap + ((_dx+7)/8)*y;
            for (int x=0; x<_dx; x++) {
                int cl = (src[x>>2] >> (6-((x&3)*2)))&3;
                cl = cmap[cl][ (x&1) + ((y&1)<<1) ];
                if (cmap[cl][ (x&1) + ((y&1)<<1) ])
                    dst[x>>3] |= 0x80>>(x&7);
            }
        }
    }
    else
    {
        for (int y=0; y<_dy; y++)
        {
            lUInt8 * src = GetScanLine(y);
            lUInt8 * dst = bitmap + ((_dx+7)/8)*y;
            for (int x=0; x<_dx; x++) {
                int cl = (src[x>>2] >> (7-((x&3)*2)))&1;
                if (cl)
                    dst[x>>3] |= 0x80>>(x&7);
            }
        }
    }
    free( _data );
    _data = bitmap;
    _bpp = 1;
    _rowsize = (_dx+7)/8;
}

//=======================================================
// 32-bit RGB buffer
//=======================================================

/// invert image
void  LVColorDrawBuf::Invert()
{
}

/// get buffer bits per pixel
int  LVColorDrawBuf::GetBitsPerPixel()
{
    return 32;
}

void LVColorDrawBuf::Draw( LVImageSourceRef img, int x, int y, int width, int height )
{
    //fprintf( stderr, "LVColorDrawBuf::Draw( img(%d, %d), %d, %d, %d, %d\n", img->GetWidth(), img->GetHeight(), x, y, width, height );
    LVImageScaledDrawCallback drawcb( this, img, x, y, width, height );
    img->Decode( &drawcb );
}

/// fills buffer with specified color
void LVColorDrawBuf::Clear( lUInt32 color )
{
    for (int y=0; y<_dy; y++)
    {
        lUInt32 * line = (lUInt32 *)GetScanLine(y);
        for (int x=0; x<_dx; x++)
        {
            line[x] = color;
        }
    }
}

/// get pixel value
lUInt32 LVColorDrawBuf::GetPixel( int x, int y )
{
    if (!_data || y<0 || x<0 || y>=_dy || x>=_dx)
        return 0;
    return ((lUInt32*)GetScanLine(y))[x];
}

/// fills rectangle with specified color
void LVColorDrawBuf::FillRect( int x0, int y0, int x1, int y1, lUInt32 color )
{
    if (x0<_clip.left)
        x0 = _clip.left;
    if (y0<_clip.top)
        y0 = _clip.top;
    if (x1>_clip.right)
        x1 = _clip.right;
    if (y1>_clip.bottom)
        y1 = _clip.bottom;
    if (x0>=x1 || y0>=y1)
        return;
    for (int y=y0; y<y1; y++)
    {
        lUInt32 * line = (lUInt32 *)GetScanLine(y);
        for (int x=x0; x<x1; x++)
        {
            line[x] = color;
        }
    }
}

/// sets new size
void LVColorDrawBuf::Resize( int dx, int dy )
{
    // delete old bitmap
    if ( _dx>0 && _dy>0 && _data )
    {
#if !defined(__SYMBIAN32__) && defined(_WIN32)
        if (_drawbmp)
            DeleteObject( _drawbmp );
        if (_drawdc)
            DeleteObject( _drawdc );
        _drawbmp = NULL;
        _drawdc = NULL;
#elif __SYMBIAN32__
        if (_drawbmp)
            //CleanupStack::PopAndDestroy( _drawbmp );
            delete _drawbmp;
        if (_drawdc)
            //CleanupStack::PopAndDestroy( _drawdc );
            delete _drawdc;
        if (bitmapDevice)
            delete(bitmapDevice);
    	   
        _drawbmp = NULL;
        _drawdc = NULL;	
        bitmapDevice = NULL;	    
#else
        free(_data);
#endif
        _data = NULL;
        _rowsize = 0;
        _dx = 0;
        _dy = 0;
    }
    
    if (dx>0 && dy>0) 
    {
        // create new bitmap
        _dx = dx;
        _dy = dy;
        _rowsize = dx*4;
#if !defined(__SYMBIAN32__) && defined(_WIN32)
        BITMAPINFO bmi;
        memset( &bmi, 0, sizeof(bmi) );
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = _dx;
        bmi.bmiHeader.biHeight = _dy;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage = 0;
        bmi.bmiHeader.biXPelsPerMeter = 1024;
        bmi.bmiHeader.biYPelsPerMeter = 1024;
        bmi.bmiHeader.biClrUsed = 0;
        bmi.bmiHeader.biClrImportant = 0;
    
        _drawbmp = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, (void**)(&_data), NULL, 0 );
        _drawdc = CreateCompatibleDC(NULL);
        SelectObject(_drawdc, _drawbmp);
#elif __SYMBIAN32__
        _drawbmp = new (ELeave) CFbsBitmap();
        //User::LeaveIfError(
        _drawbmp->Create(TSize(_dx, _dy), EColor16MU);//EColor64K);
        //CleanupStack::PushL(_drawbmp);
			
        // create an off-screen device and context
        bitmapDevice = CFbsBitmapDevice::NewL(_drawbmp);
        //CleanupStack::PushL(bitmapDevice);
        //User::LeaveIfError(
        bitmapDevice->CreateContext(_drawdc);
        //CleanupStack::PushL(_drawdc);
	
        _drawbmp->LockHeap();
        _data = (unsigned char *)_drawbmp->DataAddress();
        _drawbmp->UnlockHeap();
#else
        _data = (lUInt8 *)malloc( sizeof(lUInt32) * _dx * _dy);
#endif
        memset( _data, 0, _rowsize * _dy );
    }
    SetClipRect( NULL );
}

/// draws bitmap (1 byte per pixel) using specified palette
void LVColorDrawBuf::Draw( int x, int y, const lUInt8 * bitmap, int width, int height, lUInt32 * palette )
{
    //int buf_width = _dx; /* 2bpp */
    int bx = 0;
    int by = 0;
    int xx;
    int bmp_width = width;
    lUInt32 * dst;
    lUInt32 * dstline;
    const lUInt8 * src;
    lUInt32 bmpcl = palette?palette[0]:0x000000;

    if (x<_clip.left)
    {
        width += x-_clip.left;
        bx -= x-_clip.left;
        x = _clip.left;
        if (width<=0)
            return;
    }
    if (y<_clip.top)
    {
        height += y-_clip.top;
        by -= y-_clip.top;
        y = _clip.top;
        if (height<=0)
            return;
    }
    if (x + width > _clip.right)
    {
        width = _clip.right - x;
    }
    if (width<=0)
        return;
    if (y + height > _clip.bottom)
    {
        height = _clip.bottom - y;
    }
    if (height<=0)
        return;

    xx = width;

    bitmap += bx + by*bmp_width;

    for (;height;height--)
    {
        src = bitmap;
        dstline = ((lUInt32*)GetScanLine(y++)) + x;
        dst = dstline;

        for (xx = width; xx>0; --xx)
        {
            lUInt32 opaque = ((*(src++))>>4)&15;
            if ( opaque>=12 )
                *dst = bmpcl;
            else if ( opaque>3 ) {
                lUInt32 alpha = 15-opaque;
                lUInt32 cl1 = ((alpha*((*dst)&0xFF00FF) + opaque*(bmpcl&0xFF00FF))>>4) & 0xFF00FF;
                lUInt32 cl2 = ((alpha*((*dst)&0x00FF00) + opaque*(bmpcl&0x00FF00))>>4) & 0x00FF00;
                *dst = cl1 | cl2;
            }
            /* next pixel */
            dst++;
        }
        /* new dest line */
        bitmap += bmp_width;
    }
}

#if !defined(__SYMBIAN32__) && defined(_WIN32)
/// draws buffer content to DC doing color conversion if necessary
void LVGrayDrawBuf::DrawTo( HDC dc, int x, int y, int options, lUInt32 * palette )
{
    if (!dc || !_data)
        return;
    LVColorDrawBuf buf( _dx, 1 );
    lUInt32 * dst = (lUInt32 *)buf.GetScanLine(0);
    static lUInt32 def_pal_1bpp[2] = {0xFFFFFF, 0x000000};
    static lUInt32 def_pal_2bpp[4] = {0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000};
    if (!palette)
        palette = (_bpp==1) ? def_pal_1bpp : def_pal_2bpp;
    for (int yy=0; yy<_dy; yy++)
    {
        lUInt8 * src = GetScanLine(yy);
        for (int xx=0; xx<_dx; xx++)
        {
            //
            if (_bpp==1)
            {
                int shift = 7-(xx&7);
                int x0 = xx >> 3;
                dst[xx] = palette[ (src[x0]>>shift) & 1];
            }
            else if (_bpp==2)
            {
                int shift = 6-((xx&3)<<1);
                int x0 = xx >> 2;
                dst[xx] = palette[ (src[x0]>>shift) & 3];
            }
        }
        BitBlt( dc, x, y+yy, _dx, 1, buf.GetDC(), 0, 0, SRCCOPY );
    }
}


/// draws buffer content to DC doing color conversion if necessary
void LVColorDrawBuf::DrawTo( HDC dc, int x, int y, int options, lUInt32 * palette )
{
    if (dc!=NULL && _drawdc!=NULL)
        BitBlt( dc, x, y, _dx, _dy, _drawdc, 0, 0, SRCCOPY );
}

#elif __SYMBIAN32__

void LVGrayDrawBuf::DrawTo(CWindowGc &dc, int x, int y, int options, lUInt32 * palette )
{
    //if (!dc || !_data)
    //    return;
    LVColorDrawBuf buf( _dx, 1 );
    lUInt32 * dst = (lUInt32 *)buf.GetScanLine(0);
    static lUInt32 def_pal_1bpp[2] = {0xFFFFFF, 0x000000};
    static lUInt32 def_pal_2bpp[4] = {0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000};
    if (!palette)
        palette = (_bpp==1) ? def_pal_1bpp : def_pal_2bpp;
    for (int yy=0; yy<_dy; yy++)
    {
        lUInt8 * src = GetScanLine(yy);
        for (int xx=0; xx<_dx; xx++)
        {
            //
            if (_bpp==1)
            {
                int shift = 7-(xx&7);
                int x0 = xx >> 3;
                dst[xx] = palette[ (src[x0]>>shift) & 1];
            }
            else if (_bpp==2)
            {
                int shift = 6-((xx&3)<<1);
                int x0 = xx >> 2;
                dst[xx] = palette[ (src[x0]>>shift) & 3];
            }
        }
       	dc.BitBlt(TPoint(x, y+yy), buf.GetBitmap());
    }
}

/// draws buffer content to DC doing color conversion if necessary
void LVColorDrawBuf::DrawTo(CWindowGc &dc, int x, int y, int options, lUInt32 * palette )
{
	if (_drawbmp)
		dc.BitBlt(TPoint(0, 0), _drawbmp);
}

#endif

/// draws buffer content to another buffer doing color conversion if necessary
void LVColorDrawBuf::DrawTo( LVDrawBuf * buf, int x, int y, int options, lUInt32 * palette )
{
    //
    lvRect clip;
    buf->GetClipRect(&clip);
    int bpp = buf->GetBitsPerPixel();
    for (int yy=0; yy<_dy; yy++)
    {
        if (y+yy >= clip.top && y+yy < clip.bottom)
        {
            lUInt32 * src = (lUInt32 *)GetScanLine(yy);
            if (bpp==1)
            {
                int shift = x & 7;
                lUInt8 * dst = buf->GetScanLine(y+yy) + (x>>3);
                for (int xx=0; xx<_dx; xx++)
                {
                    if ( x+xx >= clip.left && x+xx < clip.right )
                    {
                        //lUInt8 mask = ~((lUInt8)0xC0>>shift);
                        lUInt8 cl = (((lUInt8)(*src)&0x80)^0x80) >> (shift);
                        *dst |= cl;
                    }    
                    if ( !(shift = (shift+1)&7) )
                        dst++;
                    src++;
                }
            }
            else if (bpp==2)
            {
                int shift = x & 3;
                lUInt8 * dst = buf->GetScanLine(y+yy) + (x>>2);
                for (int xx=0; xx<_dx; xx++)
                {
                    if ( x+xx >= clip.left && x+xx < clip.right )
                    {
                        //lUInt8 mask = ~((lUInt8)0xC0>>shift);
                        lUInt8 cl = (((lUInt8)(*src)&0xC0)^0xC0) >> (shift<<1);
                        *dst |= cl;
                    }    
                    if ( !(shift = (shift+1)&3) )
                        dst++;
                    src++;
                }
            }
            else if (bpp==32)
            {
                lUInt32 * dst = ((lUInt32 *)buf->GetScanLine(y+yy)) + x;
                for (int xx=0; xx<_dx; xx++)
                {
                    if ( x+xx >= clip.left && x+xx < clip.right )
                    {
                        *dst = *src;
                    }    
                    dst++;
                    src++;
                }
            }
        }
    }
}

/// returns scanline pointer
lUInt8 * LVColorDrawBuf::GetScanLine( int y )
{
    if (!_data || y<0 || y>=_dy)
        return NULL;
#if !defined(__SYMBIAN32__) && defined(_WIN32)
    return _data + _rowsize * (_dy-1-y);
#elif __SYMBIAN32__
    return _data + _rowsize * y;
#else
    return _data + _rowsize * y;
#endif
}

/// constructor
LVColorDrawBuf::LVColorDrawBuf(int dx, int dy)
:     LVBaseDrawBuf()
#if !defined(__SYMBIAN32__) && defined(_WIN32)
    ,_drawdc(NULL)
    ,_drawbmp(NULL)
#elif defined(__SYMBIAN32__)
    ,_drawdc(NULL)
    ,_drawbmp(NULL)
    ,bitmapDevice(NULL)
#endif
{
    Resize( dx, dy );
}

/// destructor
LVColorDrawBuf::~LVColorDrawBuf()
{
#if !defined(__SYMBIAN32__) && defined(_WIN32)
    if (_drawdc)
        DeleteDC(_drawdc);
    if (_drawbmp)
        DeleteObject(_drawbmp);
#elif defined(__SYMBIAN32__)
    if (_drawdc)
        //CleanupStack::PopAndDestroy(_drawdc);
        delete(_drawdc);
    if (_drawbmp)
       //CleanupStack::PopAndDestroy(_drawbmp);
       delete(_drawbmp);
    if (bitmapDevice)
    	delete(bitmapDevice);   
#else
    if (_data)
        free( _data );
#endif
}

/// convert to 1-bit bitmap
void LVColorDrawBuf::ConvertToBitmap(bool flgDither)
{
}

