#include "TextRenderer.h"
#include "geometry/Geometry.h"
#include <map>
#include <tuple>
#include "math/MathRenderer.h"
#include "MathMesh.h"
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QTextBoundaryFinder>
#include <QFontDatabase>
static void initializeTextFontResources(){Q_INIT_RESOURCE(text_fonts);}
namespace scalar {
void registerTextFonts(){
    static const bool registered=[]{
        initializeTextFontResources();
        for(const auto* name:{"Regular","Italic","Bold"})QFontDatabase::addApplicationFont(QString(":/assets/fonts/LobsterTwo-%1.otf").arg(name));
        return true;
    }();
    (void)registered;
}
QFont documentFont(const QString& family,int pixelSize,bool bold,bool italic){
    QFont result(family);
    result.setPixelSize(std::max(1,pixelSize));result.setBold(bold);result.setItalic(italic);
#ifdef Q_OS_LINUX
    // Qt 6's fontconfig fallback can dereference an invalid charset for some
    // installed fonts. Keep explicitly chosen fonts on their own glyph engine.
    result.setStyleStrategy(QFont::NoFontMerging);
#endif
    return result;
}
namespace {
constexpr double baseScale=96./25.4;
QColor textColor(const TextObject& t){const auto c=t.style.rgba;return QColor((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255);}
QFont font(const TextObject& t,double scale){return documentFont(QString::fromStdString(t.fontFamily),int(std::round(t.fontSizePt*25.4/72*scale)),t.bold,t.italic);}
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
struct Run {QString text;const MathFragment* math=nullptr;double x=0,y=0,width=0,height=0;bool bold=false,italic=false;std::uint32_t rgba=0;};
struct Layout {std::vector<Run> runs;double width=0,height=0;};
Layout layout(const TextObject& t){
    const auto f=font(t,baseScale);const QFontMetricsF metrics(f);const double em=f.pixelSize(),lineHeight=metrics.height()*1.25,maxWidth=t.boxWidthMm>0?std::max(1.,t.boxWidthMm*baseScale-4):std::max(120.,80*baseScale);
    Layout out;double x=0,y=0,h=lineHeight;std::size_t start=0;
    auto newline=[&]{out.width=std::max(out.width,x);x=0;y+=h;h=lineHeight;};
    auto add=[&](QString word,const MathFragment* math,bool bold,bool italic,std::uint32_t rgba){
        auto runFont=f;runFont.setBold(bold);runFont.setItalic(italic);const QFontMetricsF runMetrics(runFont);
        const double w=math?math->widthEm*em:runMetrics.horizontalAdvance(word),height=math?math->heightEm*em:runMetrics.height()*1.25;
        if(math&&math->display&&x>0)newline();
        if(x>0&&x+w>maxWidth)newline();
        out.runs.push_back({word,math,x,y,w,height,bold,italic,rgba});x+=w;h=std::max(h,height);
        if(math&&math->display)newline();
    };
    auto wordRun=[&](const QString& word,bool bold,bool italic,std::uint32_t rgba){
        auto runFont=f;runFont.setBold(bold);runFont.setItalic(italic);const QFontMetricsF runMetrics(runFont);
        if(t.boxWidthMm<=0||runMetrics.horizontalAdvance(word)<=maxWidth){add(word,nullptr,bold,italic,rgba);return;}
        // Wrap long words at grapheme boundaries, preserving accents and emoji.
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme,word);QString chunk;int start=0;
        for(int end=finder.toNextBoundary();end>=0;end=finder.toNextBoundary()){
            const auto grapheme=word.mid(start,end-start);start=end;
            if(!chunk.isEmpty()&&runMetrics.horizontalAdvance(chunk+grapheme)>maxWidth){add(chunk,nullptr,bold,italic,rgba);chunk.clear();}
            chunk+=grapheme;
        }
        if(!chunk.isEmpty())add(chunk,nullptr,bold,italic,rgba);
    };
    auto styleAt=[&](std::size_t position){
        const auto it=std::upper_bound(t.formats.begin(),t.formats.end(),position,[](std::size_t p,const TextFormat& f){return p<f.start;});
        if(it!=t.formats.begin()){const auto& format=*std::prev(it);if(position<format.start+format.length)return std::tuple{format.bold,format.italic,format.rgba.value_or(t.style.rgba)};}
        return std::tuple{t.bold,t.italic,t.style.rgba};
    };
    auto styledWord=[&](const QString& word,std::size_t position){
        for(qsizetype first=0;first<word.size();){auto last=first+1;const auto style=styleAt(position+first);
            while(last<word.size()&&styleAt(position+last)==style)++last;
            wordRun(word.mid(first,last-first),std::get<0>(style),std::get<1>(style),std::get<2>(style));first=last;
        }
    };
    auto plain=[&](std::string value,std::size_t byteStart){const auto text=QString::fromStdString(value);const auto pieces=text.split(QRegularExpression("(?<=\\s)|(?=\\n)"));
        std::size_t position=QString::fromStdString(t.source.substr(0,byteStart)).size();
        for(const auto& word:pieces){if(word.contains('\n')){auto parts=word.split('\n');for(int i=0;i<parts.size();++i){if(!parts[i].isEmpty())styledWord(parts[i],position);position+=parts[i].size();if(i+1<parts.size()){newline();++position;}}}else if(!word.isEmpty()){styledWord(word,position);position+=word.size();}}
    };
    for(const auto& m:t.math){plain(t.source.substr(start,m.start-start),start);const auto style=styleAt(QString::fromStdString(t.source.substr(0,m.start+(m.display?2:1))).size());add({},&m,t.bold,t.italic,std::get<2>(style));start=m.start+m.length;}plain(t.source.substr(start),start);
    out.width=std::max(1.,std::max(out.width,x));out.height=std::max(lineHeight,y+h);
    if(t.boxWidthMm>0)out.width=std::max(out.width,t.boxWidthMm*baseScale-4);
    if(t.boxHeightMm>0)out.height=std::max(out.height,t.boxHeightMm*baseScale-4);
    if(t.alignment){std::map<double,double> right;for(const auto& run:out.runs)right[run.y]=std::max(right[run.y],run.x+run.width);for(auto& run:out.runs)run.x+=(out.width-right[run.y])*(t.alignment==1?.5:1.);}
    return out;
}
}
bool svgRenderingAvailable(){return true;}
QString paintVectorText(QPainter& painter,const TextObject& t){
    const auto l=layout(t);const double em=font(t,baseScale).pixelSize();
    painter.save();painter.scale(1/baseScale,1/baseScale);painter.translate(2,2);painter.setFont(font(t,baseScale));painter.setPen(textColor(t));
    for(const auto& run:l.runs){
        const auto c=run.rgba;const QColor color((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255);painter.setPen(color);
        if(!run.math){auto runFont=painter.font();runFont.setBold(run.bold);runFont.setItalic(run.italic);painter.setFont(runFont);painter.drawText(QPointF(run.x,run.y+QFontMetricsF(runFont).ascent()),run.text);continue;}
        const auto mesh=svgMathMesh(run.math->svg);if(!mesh.error.isEmpty()){painter.restore();return mesh.error;}
        QPainterPath path;path.setFillRule(Qt::WindingFill);
        for(std::size_t i=0;i+2<mesh.vertices.size();i+=3){const auto a=mesh.vertices[i],b=mesh.vertices[i+1],c=mesh.vertices[i+2];path.moveTo(run.x+a.x*em,run.y+a.y*em);path.lineTo(run.x+b.x*em,run.y+b.y*em);path.lineTo(run.x+c.x*em,run.y+c.y*em);path.closeSubpath();}
        painter.fillPath(path,color);
    }
    painter.restore();return {};
}
static QImage renderBitmap(const TextObject& t,double scale,bool includeMath){
    const auto l=layout(t);const double ratio=std::min(scale/baseScale,4096./std::max(l.width+4,l.height+4));
    QImage image(std::max(1,int(std::ceil((l.width+4)*ratio))),std::max(1,int(std::ceil((l.height+4)*ratio))),QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::TextAntialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);p.scale(ratio,ratio);p.translate(2,2);p.setFont(font(t,baseScale));p.setPen(textColor(t));
    for(const auto& run:l.runs){const auto c=run.rgba;const QColor color((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255);p.setPen(color);if(run.math){if(!includeMath)continue;const auto bitmap=svgBitmap(run.math->svg,QSize(std::max(1,int(run.width*ratio)),std::max(1,int(run.height*ratio))),color);p.drawImage(QRectF(run.x,run.y,run.width,run.height),bitmap);}else {auto runFont=p.font();runFont.setBold(run.bold);runFont.setItalic(run.italic);p.setFont(runFont);p.drawText(QPointF(run.x,run.y+QFontMetricsF(runFont).ascent()),run.text);}}
    return image;
}
QImage textBitmap(const TextObject& t,double scale){return renderBitmap(t,scale,true);}
QSizeF textNaturalSize(const TextObject& t){const auto l=layout(t);return {(l.width+4)/baseScale,(l.height+4)/baseScale};}
TextVisual textVisual(const TextObject& t,double scale){
    TextVisual visual;visual.text=renderBitmap(t,scale,false);visual.naturalSize=textNaturalSize(t);const auto l=layout(t);const double em=font(t,baseScale).pixelSize();
    for(const auto& run:l.runs)if(run.math){auto mesh=svgMathMesh(run.math->svg);if(!mesh.error.isEmpty()){visual.error=mesh.error;return visual;}
        visual.mathColors.push_back({visual.math.size(),mesh.vertices.size(),run.rgba});
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
    auto visual=textVisual(t,baseScale*2);return {t,visual.text,visual.error,std::move(visual.math),visual.naturalSize,std::move(visual.mathColors)};
}
}
