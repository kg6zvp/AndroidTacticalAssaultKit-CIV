#include "feature/Style.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "feature/DrawingTool.h"
#include "math/Utils.h"
#include "util/Memory.h"

using namespace atakmap;

using namespace TAK::Engine;
using namespace TAK::Engine::Util;

namespace
{

    class OGR_Color
    {
    public:
        OGR_Color (unsigned int color) :
            color (color)
        { }
    private:
        template <class _CharT, class _Traits>
        friend
        std::basic_ostream<_CharT, _Traits>& operator<< (std::basic_ostream<_CharT, _Traits>& strm, const OGR_Color& ogr)
        {
            std::ios::fmtflags oldFlags (strm.flags ());
            std::streamsize oldWidth (strm.width ());
            char oldFill (strm.fill ('0'));
            char fmt[10];
#ifdef MSVC
            _snprintf(fmt, 10, "#%06X%02X", (ogr.color & 0xFFFFFF), (ogr.color >> 24 & 0xFF));
#else
            snprintf(fmt, 10, "#%06X%02X", (ogr.color & 0xFFFFFF), (ogr.color >> 24 & 0xFF));
#endif
            strm << fmt;

            // Return stream with restored settings.
            strm.flags (oldFlags);
            return strm << std::setw (oldWidth) << std::setfill (oldFill);
        }

        unsigned int color;
    };

    template <typename T>
    struct VecDeleter
    {
        VecDeleter (const std::vector<T*>& vector) :
            vector (vector)
        { }

        static void deleteElement (T* elem)
        { delete elem; }

        void operator() () const
        {
            std::for_each (vector.begin (), vector.end (),
                       std::ptr_fun (deleteElement));
        }

        const std::vector<T*>& vector;
    };
}

namespace
{

    bool isNULL (const void* ptr)
    { return !ptr; }

    inline
    float pixelsToPoints (float pixels)
    {
        // This value of PPI is copied from convertUnits in DrawingTool.cpp.
        // Both functions should get the value from elsewhere.
        const double PPI (240);             // Need to get this from somewhere!!!

        return pixels * 72 / PPI;           // 72 points per inch.
    }
}

namespace atakmap {
    namespace feature {

Style::Style(const StyleClass styleClass_) NOTHROWS :
    styleClass(styleClass_)
{}
Style::~Style () throw ()
  { }
StyleClass Style::getClass() const NOTHROWS
{
    return styleClass;
}
void Style::destructStyle(const Style *style)
{
    delete style;
}

Style* Style::parseStyle (const char* styleOGR)
{
    if (!styleOGR) {
        throw std::invalid_argument
                  ("atakmap::feature::Style::parseStyle: "
                   "Received NULL OGR style string");
    }

    const char* styleEnd (styleOGR+ std::strlen (styleOGR));
    std::vector<std::unique_ptr<Style>> styles;

    while (styleOGR!= styleEnd) {
        std::auto_ptr<DrawingTool> tool (DrawingTool::parseOGR_Tool (styleOGR, styleOGR));
        Brush* tmpBrush = (tool.get() && tool->getType() == DrawingTool::BRUSH) ? static_cast<Brush *>(tool.get()) : NULL;
        Label* tmpLabel = (tool.get() && tool->getType() == DrawingTool::LABEL) ? static_cast<Label *>(tool.get()) : NULL;
        Pen* tmpPen = (tool.get() && tool->getType() == DrawingTool::PEN) ? static_cast<Pen *>(tool.get()) : NULL;
        Symbol* tmpSymbol = (tool.get() && tool->getType() == DrawingTool::SYMBOL) ? static_cast<Symbol *>(tool.get()) : NULL;

        if (tmpBrush) {
            styles.emplace_back (std::unique_ptr<Style>(new BasicFillStyle (tmpBrush->foreColor)));
        } else if (tmpPen) {
            styles.emplace_back (std::unique_ptr<Style>(new BasicStrokeStyle (tmpPen->color,
                                                    tmpPen->width)));
        } else if (tmpSymbol) {
            float scaling (tmpSymbol->scaling);
            float size (tmpSymbol->size);
            IconPointStyle::HorizontalAlignment hAlign
                (tmpSymbol->dx == 0.0
                 ? IconPointStyle::H_CENTER
                 : tmpSymbol->dx < 0.0
                   ? IconPointStyle::LEFT
                   : IconPointStyle::RIGHT);
            IconPointStyle::VerticalAlignment vAlign
                (tmpSymbol->dy == 0.0
                 ? IconPointStyle::V_CENTER
                 : tmpSymbol->dy < 0.0
                   ? IconPointStyle::ABOVE
                   : IconPointStyle::BELOW);

            if(!tmpSymbol->names.get()) {
                styles.emplace_back(std::unique_ptr<Style>(new BasicPointStyle(tmpSymbol->color, tmpSymbol->size)));
            } else {
                styles.emplace_back(std::unique_ptr<Style>(
                    scaling
                      ? new IconPointStyle(tmpSymbol->color,
                      tmpSymbol->names,
                      scaling,
                      hAlign,
                      vAlign,
                      -tmpSymbol->angle)
                      : new IconPointStyle(tmpSymbol->color,
                      tmpSymbol->names,
                      size, size,
                      hAlign,
                      vAlign,
                      -tmpSymbol->angle)));
            }
        } else if (tmpLabel) {
            // Label represents font size in pixels.  Convert to points.
            float fontSize (pixelsToPoints (tmpLabel->fontSize));
            LabelPointStyle::HorizontalAlignment hAlign (LabelPointStyle::H_CENTER);

            switch (tmpLabel->position)
            {
            case Label::BASELINE_LEFT:
            case Label::CENTER_LEFT:
            case Label::TOP_LEFT:
            case Label::BOTTOM_LEFT:
                hAlign = LabelPointStyle::RIGHT;
                break;
            case Label::BASELINE_RIGHT:
            case Label::CENTER_RIGHT:
            case Label::TOP_RIGHT:
            case Label::BOTTOM_RIGHT:
                hAlign = LabelPointStyle::LEFT;
                break;
            default:
                break;
            }

            LabelPointStyle::VerticalAlignment vAlign (LabelPointStyle::V_CENTER);

            switch (tmpLabel->position)
            {
            case Label::BASELINE_LEFT:
            case Label::BASELINE_CENTER:
            case Label::BASELINE_RIGHT:
            case Label::BOTTOM_LEFT:
            case Label::BOTTOM_CENTER:
            case Label::BOTTOM_RIGHT:
                vAlign = LabelPointStyle::ABOVE;
                break;
            case Label::TOP_LEFT:
            case Label::TOP_CENTER:
            case Label::TOP_RIGHT:
                vAlign = LabelPointStyle::BELOW;
                break;
            default:
                break;
            }

            // Determine ScrollMode from OGR Label placement mode.  See
            // LabelPointStyle::toOGR below.
            LabelPointStyle::ScrollMode scrollMode (LabelPointStyle::DEFAULT);

            switch (tmpLabel->placement)
              {
              case Label::STRETCHED:

                scrollMode = LabelPointStyle::ON;
                break;

              case Label::MIDDLE:

                scrollMode = LabelPointStyle::OFF;
                break;

              default:
                break;
            }

            if(!tmpLabel->text)
                tmpLabel->text = "";

            styles.emplace_back (std::unique_ptr<Style>(new LabelPointStyle (tmpLabel->text,
                                                   tmpLabel->foreColor,
                                                   tmpLabel->backColor,
                                                   scrollMode,
                                                   fontSize,
                                                   hAlign,
                                                   vAlign,
                                                   -tmpLabel->angle)));
        }
    }

    Style* result (NULL);

    if (styles.size () == 1) {
        result = styles[0].release();
    } else if (styles.size () > 1) {
        std::vector<Style *> compositeStyles;
        compositeStyles.reserve(styles.size());
        for (std::size_t i = 0u; i < styles.size(); i++)
            compositeStyles.push_back(styles[i].release());
        result = new CompositeStyle (compositeStyles);
    }

    return result;
}

BasicFillStyle::BasicFillStyle (unsigned int color) throw () :
    Style(TESC_BasicFillStyle),
    color (color)
{ }
TAKErr BasicFillStyle::toOGR(Port::String &value) const NOTHROWS
{
    std::ostringstream strm;
    strm << "BRUSH(fc:" << OGR_Color(color) << ")";
    value = strm.str().c_str();
    return TE_Ok;
}
Style *BasicFillStyle::clone() const
{
    return new BasicFillStyle(*this);
}

BasicPointStyle::BasicPointStyle (unsigned int color, float size) :
    Style(TESC_BasicPointStyle),
    color (color),
    size (size)
{
    if (size < 0) {
        throw std::invalid_argument
                  ("atakmap::feature::BasicPointStyle::BasicPoinStyle: "
                   "Received negative size");
    }
}
TAKErr BasicPointStyle::toOGR(Port::String &value) const NOTHROWS
{
    std::ostringstream strm;
    strm << "SYMBOL(c:" << OGR_Color(color) << ",s:" << size << "px)";
    value = strm.str().c_str();
    return TE_Ok;
}
Style * BasicPointStyle::clone() const
{
  return new BasicPointStyle(*this);
}

BasicStrokeStyle::BasicStrokeStyle (unsigned int color, float width) :
    Style(TESC_BasicStrokeStyle),
    color (color),
    width (width)
{
    if (width < 0) {
        throw std::invalid_argument
                  ("atakmap::feature::BasicStrokeStyle::BasicStrokeStyle: "
                   "Received negative width");
    }
}
TAKErr BasicStrokeStyle::toOGR(Port::String &value) const NOTHROWS
{
    std::ostringstream strm;
	strm << "PEN(c:" << OGR_Color(color) << ",w:" << width << "px)";
    value = strm.str().c_str();
    return TE_Ok;
}
Style *BasicStrokeStyle::clone() const
{
    return new BasicStrokeStyle(*this);
}


CompositeStyle::CompositeStyle (const std::vector<Style*>& styles) :
    Style(TESC_CompositeStyle),
    styles (styles.begin (), styles.end ())
{
    if (styles.empty ()) {
        throw std::invalid_argument
                  ("atakmap::feature::CompositeStyle::CompositeStyle: "
                   "Received empty Style vector");
    }

    if (std::find_if (styles.begin (), styles.end (), std::ptr_fun (isNULL))
        != styles.end ()) {
        throw std::invalid_argument
                  ("atakmap::feature::CompositeStyle::CompositeStyle: "
                   "Received NULL Style");
    }
}
const Style& CompositeStyle::getStyle (std::size_t index) const throw (std::range_error)
{
    if (index >= styles.size ()) {
        throw std::range_error
                  ("atakmap::feature::CompositeStyle::getStyle: "
                   "Received out-of-range index");
    }
    return *styles[index];
}
TAKErr CompositeStyle::toOGR(Port::String &value) const NOTHROWS
{
    TAKErr code(TE_Ok);
    std::ostringstream strm;
    StyleVector::const_iterator iter (styles.begin ());
    StyleVector::const_iterator end (styles.end ());

    TAK::Engine::Port::String substyle;

    code = (*iter)->toOGR(substyle);
    TE_CHECKRETURN_CODE(code);
    strm << substyle;
    while (++iter != end) {
        substyle = nullptr;
        code = (*iter)->toOGR(substyle);
        TE_CHECKRETURN_CODE(code);
        strm << ";" << substyle;
    }

    value = strm.str().c_str();
    return TE_Ok;
}
Style *CompositeStyle::clone() const
{
    std::vector<Style *> styles;
    styles.reserve(this->getStyleCount());
    for (size_t i = 0; i < this->getStyleCount(); ++i) {
        styles.push_back(this->getStyle(i).clone());
    }
    return new CompositeStyle(styles);
}

IconPointStyle::IconPointStyle (unsigned int color,
                                const char* iconURI,
                                float scaling,
                                HorizontalAlignment hAlign,
                                VerticalAlignment vAlign,
                                float rotation,
                                bool absoluteRotation) :
    Style(TESC_IconPointStyle),
    color (color),
    iconURI (iconURI),
    hAlign (hAlign),
    vAlign (vAlign),
    scaling (scaling == 0 ? 1.0 : scaling),
    width (0),
    height (0),
    rotation (rotation),
    absoluteRotation (absoluteRotation)
{

    if (!iconURI) {
        throw std::invalid_argument
                  ("atakmap::feature::IconPointStyle::IconPointStyle: "
                   "Received NULL icon URI");
    }
    if (scaling < 0) {
        throw std::invalid_argument
                  ("atakmap::feature::IconPointStyle::IconPointStyle: "
                   "Received negative scaling");
    }
}
IconPointStyle::IconPointStyle (unsigned int color,
                                const char* iconURI,
                                float width,
                                float height,
                                HorizontalAlignment hAlign,
                                VerticalAlignment vAlign,
                                float rotation,
                                bool absoluteRotation) :
    Style(TESC_IconPointStyle),
    color (color),
    iconURI (iconURI),
    hAlign (hAlign),
    vAlign (vAlign),
    scaling (!width && !height ? 1.0 : 0.0),
    width (width),
    height (height),
    rotation (rotation),
    absoluteRotation (absoluteRotation)
{
    if (!iconURI) {
        throw std::invalid_argument
                  ("atakmap::feature::IconPointStyle::IconPointStyle: "
                   "Received NULL icon URI");
    }
    if (width < 0) {
        throw std::invalid_argument
                  ("atakmap::feature::IconPointStyle::IconPointStyle: "
                   "Received negative width");
    }
    if (height < 0) {
        throw std::invalid_argument
                  ("atakmap::feature::IconPointStyle::IconPointStyle: "
                   "Received negative height");
    }
}
TAKErr IconPointStyle::toOGR(Port::String &value) const NOTHROWS
{
    std::ostringstream strm;
    float size (std::max (width, height));

    strm << "SYMBOL(id:\"" << iconURI << "\"";
    if (rotation) {
        strm << ",a:" << -rotation;     // OGR uses CCW degrees.
    }
	strm << ",c:" << OGR_Color(color);
    if (!scaling) {
        strm << ",s:" << size << "px";  // Units means size, not scaling.
    } else {
        strm << ",s:" << scaling;       // No units means scaling, not size.

        // Since we have no explicit size and may need to provide X and Y
        // offsets below, set size to a scaled (arbitrarily selected) 32-pixel
        // symbol size.
        size = 32 * scaling;
      }

    switch (hAlign)
    {
    case LEFT:  strm << ",dx:" << -size << "px";    break;
    case RIGHT: strm << ",dx:" << size << "px";     break;
    default:                                        break;
    }
    switch (vAlign)
    {
    case ABOVE: strm << ",dy:" << -size << "px";    break;
    case BELOW: strm << ",dy:" << size << "px";     break;
    default:                                        break;
    }
    strm << ")";

    value = strm.str().c_str();
    return TE_Ok;
}
Style *IconPointStyle::clone() const
{
  return new IconPointStyle(*this);
}

LabelPointStyle::LabelPointStyle (const char* text,
                                  unsigned int textColor,
                                  unsigned int backColor,
                                  ScrollMode mode,
                                  float textSize,
                                  HorizontalAlignment hAlign,
                                  VerticalAlignment vAlign,
                                  float rotation,
                                  bool absoluteRotation,
                                  float paddingX,
                                  float paddingY) :
    Style(TESC_LabelPointStyle),
    text (text),
    foreColor (textColor),
    backColor (backColor),
    scrollMode (mode),
    hAlign (hAlign),
    vAlign (vAlign),
    textSize (textSize ? textSize : 16),        // ATAK hack
    rotation (rotation),
    paddingX(paddingX),
    paddingY(paddingY),
    absoluteRotation (absoluteRotation)
  {
    if (!text) {
        throw std::invalid_argument
                  ("atakmap::feature::LabelPointStyle::LabelPointStyle: "
                   "Received NULL label text");
    }
    if (textSize < 0) {
        throw std::invalid_argument
                  ("atakmap::feature::LabelPointStyle::LabelPointStyle: "
                   "Received negative textSize");
    }
}
TAKErr LabelPointStyle::toOGR(Port::String &value) const NOTHROWS
{
    std::ostringstream strm;

    strm << "LABEL(t:\"";
    if (text) {
        for (int i = 0u; i < strlen(text); i++) {
            const char c = text[i];
            if (c == '"')
                strm << '\\';
            strm << c;
        }
    }
    strm << "\""
         << ",s:" << textSize << "pt";
    if (rotation) {
        strm << ",a:" << -rotation;     // OGR uses CCW degrees.
    }
    strm << ",c:" << OGR_Color (foreColor)
         << ",b:" << OGR_Color (backColor);

    // Co-opt the OGR Label placement mode to specify the scroll mode.  The
    // co-opted placements normally apply to polylines.
    //
    //  ScrollMode::DEFAULT     ==>     Label::DEFAULT
    //  ScrollMode::ON          ==>     Label::STRETCHED
    //  ScrollMode::OFF         ==>     Label::MIDDLE
    Label::Placement placement (Label::DEFAULT);

    switch (scrollMode)
    {
    case ON:  placement = Label::STRETCHED; break;
    case OFF: placement = Label::MIDDLE;    break;
    default:                                break;
    }
    (strm << ",m:").put (placement);

    // Translate the horizontal and vertical alignments into an OGR Label anchor
    // position number.  We use baseline rather than bottom vertical alignments
    // for labels that are to be above the point.
    unsigned short position (0);

    switch (hAlign)
    {
    case LEFT:  position = 6;   break;
    case RIGHT: position = 4;   break;
    default:    position = 5;   break;
    }
    switch (vAlign)
    {
    case ABOVE: position += 6;  break;  // Adjust to baseline values.
    case BELOW: position += 3;  break;  // Adjust to top values.
    default:                    break;
    }
    strm << ",p:" << position << ")";

    value = strm.str().c_str();
    return TE_Ok;
}
Style *LabelPointStyle::clone() const
{
    return new LabelPointStyle(*this);
}

PatternStrokeStyle::PatternStrokeStyle (const uint64_t pattern_,
                    const std::size_t patternLen_,
                    const unsigned int color_,
                    const float strokeWidth_) :
    Style(TESC_PatternStrokeStyle),
    pattern(pattern_),
    patternLen(patternLen_),
    color(color_),
    width(strokeWidth_)
{
    if(patternLen < 2u || patternLen > 64u || !atakmap::math::isPowerOf2(patternLen))
        throw std::invalid_argument("bad pattern length");
    if(width < 0.0)
        throw std::invalid_argument("invalid stroke width");
}
uint64_t PatternStrokeStyle::getPattern() const throw ()
{
    return pattern;
}
std::size_t PatternStrokeStyle::getPatternLength() const throw ()
{
    return patternLen;
}
unsigned int PatternStrokeStyle::getColor () const throw ()
{
    return color;
}
float PatternStrokeStyle::getStrokeWidth() const throw ()
{
    return width;
}
TAKErr PatternStrokeStyle::toOGR(TAK::Engine::Port::String &value) const NOTHROWS
{
    std::ostringstream ogr;
    //PEN(c:#FF0000,w:2px,p:”4px 5px”)
    ogr << "PEN(c:#" << OGR_Color (color) << ",w" << width << "px,p:\"";
    uint8_t state = 0x1;
    std::size_t stateCount = 0;
    uint64_t pat = pattern;
    std::size_t emit = 0;
    for(std::size_t i = 0u; i < patternLen; i++) {
        const uint8_t px = pat&0x1u;
        pat >>= 0x1u;
        if(px != state) {
            // the state has flipped, emit the pixel count
            if(!emit)
                ogr << ' ';
            ogr << stateCount << "px";
            emit++;
            state = px;
            stateCount = 0u;
        }

        // update pixel count for current state
        stateCount++;
    }
    if(stateCount) {
        if(!emit)
            ogr << ' ';
        ogr << stateCount << "px";
    }
    ogr << "\")";
    value = ogr.str().c_str();
    return TE_Ok;
}
Style *PatternStrokeStyle::clone() const
{
    return new PatternStrokeStyle(*this);
}

    }
}
