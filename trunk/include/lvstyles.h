/** \file lvstyles.h

    \brief CSS Styles and node format

    (c) Vadim Lopatin, 2000-2006

    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#if !defined(__LV_STYLES_H_INCLUDED__)
#define __LV_STYLES_H_INCLUDED__

#include "cssdef.h"
#include "lvrefcache.h"
#include "lvtextfm.h"
#include "lvfntman.h"

/**
    \brief Element style record.

    Contains set of style properties.
*/
typedef struct css_style_rec_tag {
    css_display_t        display;
    css_white_space_t    white_space;
    css_text_align_t     text_align;
    css_vertical_align_t vertical_align;
    css_font_family_t    font_family;
    lString8             font_name;
    css_length_t         font_size;
    css_font_style_t     font_style;
    css_font_weight_t    font_weight;
    css_length_t         text_indent;
    css_length_t         line_height;
    css_length_t         width;
    css_length_t         height;
    css_length_t         margin[4]; ///< margin-left, -right, -top, -bottom
    css_page_break_t     page_break_before;
    css_page_break_t     page_break_after;
    css_page_break_t     page_break_inside;
    css_style_rec_tag()
    : display( css_d_inherit )
    , white_space(css_ws_inherit)
    , text_align(css_ta_inherit)
    , vertical_align(css_va_inherit)
    , font_family(css_ff_inherit)
    , font_size(css_val_inherited, 0)
    , font_style(css_fs_inherit)
    , font_weight(css_fw_inherit)
    , text_indent(css_val_inherited, 0)
    , line_height(css_val_inherited, 0)
    , width(css_val_unspecified, 0)
    , height(css_val_unspecified, 0)
    , page_break_before(css_pb_inherit)
    , page_break_after(css_pb_inherit)
    , page_break_inside(css_pb_inherit)
    {
    }
} css_style_rec_t;

/// style record reference type
typedef LVRef< css_style_rec_t > css_style_ref_t;
/// font reference type
typedef LVRef< LVFont > font_ref_t;

/// to compare two styles
bool operator == (const css_style_rec_t & r1, const css_style_rec_t & r2);

/// style hash table size
#define LV_STYLE_HASH_SIZE 0x100

/// style cache: allows to avoid duplicate style object allocation
class lvdomStyleCache : public LVRefCache< css_style_rec_t >
{
public:
    lvdomStyleCache( int size = LV_STYLE_HASH_SIZE ) : LVRefCache< css_style_rec_t >( size ) {}
};

/// element rendering methods
enum lvdom_element_render_method
{
    erm_invisible = 0, ///< invisible: don't render
    erm_block,         ///< render as block element (render as containing other elements)
    erm_final,         ///< final element: render the whole it's content as single render block
};

/// node format record
class lvdomElementFormatRec {
private:
    css_style_ref_t _style;
    font_ref_t      _font;
    int  _x;
    int  _width;
    int  _y;
    int  _height;
    lvdom_element_render_method _rendMethod;
    //LFormattedText * _formatter;
public:
    lvdomElementFormatRec()
    : _x(0), _width(0), _y(0), _height(0), _rendMethod(erm_invisible)//, _formatter(NULL)
    {
    }
    ~lvdomElementFormatRec()
    {
    }
    // Get/Set
    int getX() { return _x; }
    int getY() { return _y; }
    int getWidth() { return _width; }
    int getHeight() { return _height; }
    void setX( int x ) { _x = x; }
    void setY( int y ) { _y = y; }
    void setWidth( int w ) { _width = w; }
    void setHeight( int h ) { _height = h; }
    lvdom_element_render_method  getRendMethod() { return _rendMethod; }
    void setRendMethod( lvdom_element_render_method  method ) { _rendMethod=method; }
    css_style_ref_t getStyle() { return _style; }
    font_ref_t getFont() { return _font; }
    void setFont( font_ref_t font ) { _font = font; }
    void setStyle( css_style_ref_t & style ) { _style = style; }
};

/// calculate cache record hash
lUInt32 calcHash(css_style_rec_t & rec);
/// calculate cache record hash
inline lUInt32 calcHash(css_style_ref_t & rec) { return calcHash( *rec.get() ); }

/// splits string like "Arial", Times New Roman, Courier;  into list
// returns number of characters processed
int splitPropertyValueList( const char * fontNames, lString8Collection & list );

/// joins list into string of comma separated quoted values
lString8 joinPropertyValueList( const lString8Collection & list );


#endif // __LV_STYLES_H_INCLUDED__
