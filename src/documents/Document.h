#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <optional>

namespace scalar {
struct Point {
    double x = 0, y = 0;
    Point operator+(Point b) const { return {x+b.x,y+b.y}; }
    Point operator-(Point b) const { return {x-b.x,y-b.y}; }
    Point operator*(double k) const { return {x*k,y*k}; }
    bool operator==(const Point&) const = default;
};
inline double length(Point p) { return std::hypot(p.x,p.y); }
struct PageSize {
    double widthMm = 210, heightMm = 297;
    bool valid() const { return std::isfinite(widthMm) && std::isfinite(heightMm) && widthMm >= 10 && heightMm >= 10 && widthMm <= 5000 && heightMm <= 5000; }
    static PageSize a4(bool landscape = false) { return landscape ? PageSize{297,210} : PageSize{210,297}; }
    static PageSize letter(bool landscape = false) { return landscape ? PageSize{279.4,215.9} : PageSize{215.9,279.4}; }
};
enum class DeviceType { Mouse, Stylus, Touch };
struct PointerSample {
    Point position; // world millimetres
    double pressure = 1, tiltX = 0, tiltY = 0, rotation = 0;
    std::uint64_t timestamp = 0;
    std::uint32_t buttons = 0;
    DeviceType device = DeviceType::Mouse;
};
enum class LinePattern { Solid, Dashed, Dotted };
struct PenStyle {
    std::uint32_t rgba = 0x263345ff;
    double minWidthMm = 0.15, maxWidthMm = 0.85, gamma = 1.2, sensitivity = 1;
    LinePattern pattern = LinePattern::Solid;
    double dashLengthMm = 3, gapLengthMm = 2, dotSpacingMm = 2.5;
    double width(double pressure) const {
        return minWidthMm + (maxWidthMm-minWidthMm)*std::pow(std::clamp(pressure*sensitivity,0.0,1.0),gamma);
    }
};
struct ObjectProperties {
    bool locked=false,visible=true;
    std::int64_t zIndex=0;
    std::uint64_t revision=0; // render cache generation, never serialized
};
struct StrokeObject {
    std::string id;
    PenStyle style;
    std::vector<PointerSample> samples;
    ObjectProperties properties{};
};
enum class ShapeKind { Line, Circle, Ellipse, Triangle, Rectangle, Square, Polygon };
struct ShapeObject {
    std::string id;
    ShapeKind kind=ShapeKind::Line;
    PenStyle style;
    std::vector<Point> vertices; // line endpoints or polygon vertices, in mm
    Point center;
    double radiusX=1,radiusY=1,rotation=0; // rotation in radians
    double fillOpacity=0.10;
    std::optional<std::uint32_t> fillRgba; // absent in older projects: follows the border
    std::uint32_t fillColor() const { return fillRgba.value_or(style.rgba); }
    ObjectProperties properties{};
};
struct ImageObject {
    std::string id;
    std::vector<Point> corners;
    std::shared_ptr<const std::vector<std::uint8_t>> png = std::make_shared<const std::vector<std::uint8_t>>();
    int pixelWidth=0,pixelHeight=0;
    ObjectProperties properties{};
};
struct MathFragment {
    std::string latex,svg;
    bool display=false;
    std::size_t start=0,length=0; // UTF-8 byte offsets including delimiters
    double widthEm=0,heightEm=0;
};
struct TextObject {
    std::string id,source,fontFamily;
    PenStyle style; // shared RGBA color, retained across text and math edits
    double fontSizePt=18;
    bool bold=false,italic=false;
    int alignment=0; // left, center, right
    std::vector<Point> corners;
    std::vector<MathFragment> math;
    ObjectProperties properties{};
};
using CanvasObject=std::variant<StrokeObject,ShapeObject,ImageObject,TextObject>;
enum class GridType { None, Ruled, Square, Dots, Millimetric, Isometric };
struct BackgroundStyle {
    GridType gridType=GridType::None;
    std::uint32_t gridColor=0x64748bff;
    double opacity=0.25, thicknessMm=0.15, spacingX=5, spacingY=5;
    bool valid() const {
        return int(gridType)>=0&&int(gridType)<=5&&std::isfinite(opacity)&&opacity>=0&&opacity<=1
            &&std::isfinite(thicknessMm)&&thicknessMm>=0.02&&thicknessMm<=2
            &&std::isfinite(spacingX)&&spacingX>=1&&spacingX<=100
            &&std::isfinite(spacingY)&&spacingY>=1&&spacingY<=100;
    }
    bool operator==(const BackgroundStyle&) const=default;
};
struct Page {
    std::string id;
    PageSize size;
    std::uint32_t background = 0xffffffff;
    std::vector<StrokeObject> strokes;
    std::vector<ShapeObject> shapes{};
    std::vector<ImageObject> images{};
    BackgroundStyle backgroundStyle{};
    std::vector<TextObject> texts{};
};
struct Project {
    std::string id, name, createdAt, updatedAt;
    std::vector<Page> pages;
};
struct ViewTransform {
    double pixelsPerMm = 96.0/25.4, zoom = 1;
    Point pan {40,40}; // logical screen pixels
    Point worldToScreen(Point p) const { return p*(pixelsPerMm*zoom)+pan; }
    Point screenToWorld(Point p) const { return (p-pan)*(1.0/(pixelsPerMm*zoom)); }
    void zoomAt(Point screen, double factor) {
        const auto world = screenToWorld(screen);
        zoom = std::clamp(zoom*factor,0.05,8.0);
        pan = screen-world*(pixelsPerMm*zoom);
    }
};
std::string newId();
}
