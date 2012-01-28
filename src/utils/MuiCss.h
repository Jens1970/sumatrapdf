/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MuiCss_h
#define MuiCss_h

#include "Vec.h"
#include "HtmlPullParser.h"

namespace mui {
namespace css {

using namespace Gdiplus;

enum PropType {
    PropFontName,           // font-family
    PropFontSize,           // font-size
    PropFontWeight,         // font-weight
    PropPadding,            // padding
    PropColor,              // color
    PropBgColor,            // background-color

    PropBorderTopWidth,     // border-top-width
    PropBorderRightWidth,   // border-right-width
    PropBorderBottomWidth,  // border-bottom-width
    PropBorderLeftWidth,    // border-left-width

    PropBorderTopColor,     // border-top-color
    PropBorderRightColor,   // border-right-color
    PropBorderBottomColor,  // border-bottom-color
    PropBorderLeftColor,    // border-left-color

    PropTextAlign,          // text-align
};

bool IsWidthProp(PropType type);
bool IsColorProp(PropType type);

struct FontNameData {
    const TCHAR * name;
};

struct FontSizeData {
    float   size;
};

struct FontWeightData {
    FontStyle style;
};

struct WidthData {
    float width;
};

struct TextAlignData {
    AlignAttr   align;
};

enum ColorType {
    ColorSolid,
    ColorGradientLinear,
    // TODO: other gradient types?
};

struct ColorDataSolid {
    ARGB color;
};

struct ColorDataGradientLinear {
    LinearGradientMode  mode;
    ARGB                startColor;
    ARGB                endColor;
};

struct ColorData {
    ColorType   type;
    union {
        ColorDataSolid          solid;
        ColorDataGradientLinear gradientLinear;
    };

    bool operator==(const ColorData& other) const;
};

struct PaddingData {
    int top, right, bottom, left;
    bool operator ==(const PaddingData& other) const {
        return (top == other.top) &&
               (right == other.right) &&
               (bottom == other.bottom) &&
               (left == other.left);
    }
};

class Prop {
private:
    // can only allocate via Alloc*() functions so that we can
    // optimize allocations
    Prop(PropType type) : type(type) {}

public:
    PropType    type;

    union {
        FontNameData    fontName;
        FontSizeData    fontSize;
        FontWeightData  fontWeight;
        PaddingData     padding;
        ColorData       color;
        WidthData       width;
        TextAlignData   align;
    };

    ~Prop();

    bool operator==(const Prop& other) const;

    float GetWidth() const {
        return width.width;
    }

    static Prop *AllocFontName(const TCHAR *name);
    static Prop *AllocFontSize(float size);
    static Prop *AllocFontWeight(FontStyle style);
    // TODO: add AllocTextAlign(const char *s);
    static Prop *AllocTextAlign(AlignAttr align);
    static Prop *AllocPadding(int top, int right, int bottom, int left);
    static Prop *AllocColorSolid(PropType type, ARGB color);
    static Prop *AllocColorSolid(PropType type, int a, int r, int g, int b);
    static Prop *AllocColorSolid(PropType type, int r, int g, int b);
    static Prop *AllocColorSolid(PropType type, const char *color);
    static Prop *AllocColorLinearGradient(PropType type, LinearGradientMode mode, ARGB startColor, ARGB endColor);
    static Prop *AllocColorLinearGradient(PropType type, LinearGradientMode mode, const char *startColor, const char *endColor);
    static Prop *AllocWidth(PropType type, float width);
};

struct Style {

    Style(Style *inheritsFrom=NULL) : inheritsFrom(inheritsFrom) {
    }

    Vec<Prop*>  props;

    // if property is not found here, we'll search the
    // inheritance chain
    Style *     inheritsFrom;

    void Set(Prop *prop);

    // shortcuts for setting multiple properties at a time
    void SetBorderWidth(float width);
    void SetBorderColor(ARGB color);
};

void Initialize();
void Destroy();

Font *CachedFontFromProps(Style *first, Style *second);

struct PropToGet {
    // provided by the caller
    PropType    type;
    // filled-out by GetProps(). Must be set to NULL by
    // caller to enable being called twice with different
    // Style objects
    Prop *      prop;
};

void GetProps(Style *style, PropToGet *props, size_t propsCount);
void GetProps(Style *first, Style *second, PropToGet *props, size_t propsCount);
Prop *GetProp(Style *first, Style *second, PropType type);

// globally known properties for elements we know about
// we fill them with default values and they can be
// modified by an app for global visual makeover
extern Style *gStyleDefault;
extern Style *gStyleButtonDefault;
extern Style *gStyleButtonMouseOver;

} // namespace css
} // namespace mui

#endif
