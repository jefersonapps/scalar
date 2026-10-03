#include "TextRenderer.h"
#include "geometry/Geometry.h"
#include <map>
#include "math/MathRenderer.h"
#include "MathMesh.h"
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QXmlStreamReader>
namespace scalar {
namespace {
constexpr double baseScale=96./25.4;
QColor textColor(const TextObject& t){const auto c=t.style.rgba;return QColor((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255);}
QFont font(const TextObject& t,double scale){QFont f(QString::fromStdString(t.fontFamily));f.setPixelSize(std::max(1,int(std::round(t.fontSizePt*25.4/72*scale))));f.setBold(t.bold);f.setItalic(t.italic);return f;}
QImage svgBitmap(const std::string& svg,QSize size,QColor color){
    const auto mesh=svgMathMesh(svg);if(!mesh.error.isEmpty())return {};
    QXmlStreamReader xml(QString::fromStdString(svg));double width=1,height=1;
    while(!xml.atEnd()){xml.readNext();if(xml.isStartElement()){const auto box=xml.attributes().value("viewBox").toString().split(QRegularExpression("[\\s,]+"),Qt::SkipEmptyParts);if(box.size()==4){width=box[2].toDouble()/1000;height=box[3].toDouble()/1000;}break;}}
    if(width<=0||height<=0)return {};
    QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);painter.scale(size.width()/width,size.height()/height);
    QPainterPath triangles;triangles.setFillRule(Qt::WindingFill);
    for(std::size_t i=0;i+2<mesh.vertices.size();i+=3){const auto a=mesh.vertices[i],b=mesh.vertices[i+1],c=mesh.vertices[i+2];triangles.moveTo(a.x,a.y);triangles.lineTo(b.x,b.y);triangles.lineTo(c.x,c.y);triangles.closeSubpath();}
    painter.fillPath(triangles,color);return image;
}
struct Run {QString text;const MathFragment* math=nullptr;double x=0,y=0,width=0,height=0;};
struct Layout {std::vector<Run> runs;double width=0,height=0;};
Layout layout(const TextObject& t){
    const auto f=font(t,baseScale);const QFontMetricsF metrics(f);const double em=f.pixelSize(),lineHeight=metrics.height()*1.25,maxWidth=std::max(120.,80*baseScale);
    Layout out;double x=0,y=0,h=lineHeight;std::size_t start=0;
    auto newline=[&]{out.width=std::max(out.width,x);x=0;y+=h;h=lineHeight;};
    auto add=[&](QString word,const MathFragment* math){
        const double w=math?math->widthEm*em:metrics.horizontalAdvance(word),height=math?math->heightEm*em:lineHeight;
        if(math&&math->display&&x>0)newline();
        if(x>0&&x+w>maxWidth)newline();
        out.runs.push_back({word,math,x,y,w,height});x+=w;h=std::max(h,height);
        if(math&&math->display)newline();
    };
    auto plain=[&](std::string value){const auto text=QString::fromStdString(value);const auto pieces=text.split(QRegularExpression("(?<=\\s)|(?=\\n)"));
        for(const auto& word:pieces){if(word.contains('\n')){auto parts=word.split('\n');for(int i=0;i<parts.size();++i){if(!parts[i].isEmpty())add(parts[i],nullptr);if(i+1<parts.size())newline();}}else if(!word.isEmpty())add(word,nullptr);}
    };
    for(const auto& m:t.math){plain(t.source.substr(start,m.start-start));add({},&m);start=m.start+m.length;}plain(t.source.substr(start));
    out.width=std::max(1.,std::max(out.width,x));out.height=std::max(lineHeight,y+h);
    if(t.alignment){std::map<double,double> right;for(const auto& run:out.runs)right[run.y]=std::max(right[run.y],run.x+run.width);for(auto& run:out.runs)run.x+=(out.width-right[run.y])*(t.alignment==1?.5:1.);}
    return out;
}
}
bool svgRenderingAvailable(){return true;}
static QImage renderBitmap(const TextObject& t,double scale,bool includeMath){
    const auto l=layout(t);const double ratio=std::min(scale/baseScale,4096./std::max(l.width+4,l.height+4));
    QImage image(std::max(1,int(std::ceil((l.width+4)*ratio))),std::max(1,int(std::ceil((l.height+4)*ratio))),QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::TextAntialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);p.scale(ratio,ratio);p.translate(2,2);p.setFont(font(t,baseScale));p.setPen(textColor(t));
    const QFontMetricsF metrics(p.font());
    for(const auto& run:l.runs){if(run.math){if(!includeMath)continue;const auto bitmap=svgBitmap(run.math->svg,QSize(std::max(1,int(run.width*ratio)),std::max(1,int(run.height*ratio))),textColor(t));p.drawImage(QRectF(run.x,run.y,run.width,run.height),bitmap);}else p.drawText(QPointF(run.x,run.y+metrics.ascent()),run.text);}
    return image;
}
QImage textBitmap(const TextObject& t,double scale){return renderBitmap(t,scale,true);}
QSizeF textNaturalSize(const TextObject& t){const auto l=layout(t);return {(l.width+4)/baseScale,(l.height+4)/baseScale};}
TextVisual textVisual(const TextObject& t,double scale){
    TextVisual visual;visual.text=renderBitmap(t,scale,false);visual.naturalSize=textNaturalSize(t);const auto l=layout(t);const double em=font(t,baseScale).pixelSize();
    for(const auto& run:l.runs)if(run.math){auto mesh=svgMathMesh(run.math->svg);if(!mesh.error.isEmpty()){visual.error=mesh.error;return visual;}
        for(auto p:mesh.vertices)visual.math.push_back({(run.x+2+p.x*em)/baseScale,(run.y+2+p.y*em)/baseScale});
    }return visual;
}
PreparedText prepareText(TextObject t){
    auto fragments=mathFragments(t.source);
    if(!fragments.empty()&&!svgRenderingAvailable())return {t,{},"O pacote Scalar precisa de um renderizador SVG para matemática."};
    const auto error=renderMath(fragments);if(!error.isEmpty())return {t,{},error};t.math=std::move(fragments);
    const auto l=layout(t);const auto origin=t.corners.empty()?Point{20,20}:t.corners[0];
    const auto edge=t.corners.size()==4?t.corners[1]-origin:Point{1,0};const auto angle=std::atan2(edge.y,edge.x);
    const double w=(l.width+4)/baseScale,h=(l.height+4)/baseScale;
    t.corners={origin,rotatePoint(origin+Point{w,0},origin,angle),rotatePoint(origin+Point{w,h},origin,angle),rotatePoint(origin+Point{0,h},origin,angle)};
    auto visual=textVisual(t,baseScale*2);return {t,visual.text,visual.error,std::move(visual.math),visual.naturalSize};
}
}
