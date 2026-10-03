#include "MathMesh.h"
#include "math/SvgValidation.h"
#include <QPainterPath>
#include <QFont>
#include <QPainterPathStroker>
#include <QTransform>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <cstdlib>
namespace scalar {
namespace {
class PathReader {
    const char* at_;
    bool valid_=true;
    void space(){while(*at_&&(std::isspace(static_cast<unsigned char>(*at_))||*at_==','))++at_;}
    double number(){space();char* end=nullptr;const double n=std::strtod(at_,&end);if(end==at_||!std::isfinite(n)){valid_=false;return 0;}at_=end;return n;}
public:
    explicit PathReader(const QByteArray& data):at_(data.constData()){}
    std::optional<QPainterPath> read(){
        QPainterPath path;char command=0,previous=0;QPointF lastControl;
        int budget=100000;
        while(valid_&&--budget>0){space();if(!*at_)break;if(std::isalpha(static_cast<unsigned char>(*at_)))command=*at_++;else if(!command){valid_=false;break;}
            const bool relative=std::islower(static_cast<unsigned char>(command));const char type=char(std::toupper(static_cast<unsigned char>(command)));
            const auto origin=path.currentPosition();
            auto point=[&]{const double x=number(),y=number();return QPointF(x,y)+(relative?origin:QPointF{});};
            if(type=='M'){const auto p=point();path.moveTo(p);command=relative?'l':'L';}
            else if(type=='L')path.lineTo(point());
            else if(type=='H'){const auto x=number();path.lineTo(x+(relative?origin.x():0),origin.y());}
            else if(type=='V'){const auto y=number();path.lineTo(origin.x(),y+(relative?origin.y():0));}
            else if(type=='Q'){const auto c=point(),end=point();path.quadTo(c,end);lastControl=c;}
            else if(type=='T'){const auto c=previous=='Q'||previous=='T'?origin*2-lastControl:origin;path.quadTo(c,point());lastControl=c;}
            else if(type=='C'){const auto a=point(),b=point(),end=point();path.cubicTo(a,b,end);lastControl=b;}
            else if(type=='S'){const auto a=previous=='C'||previous=='S'?origin*2-lastControl:origin;const auto b=point(),end=point();path.cubicTo(a,b,end);lastControl=b;}
            else if(type=='Z'){path.closeSubpath();command=0;}
            else valid_=false;
            previous=type;
        }
        if(!valid_||budget<=0)return {};return path;
    }
};
QTransform transform(const QString& text){
    QTransform result;
    const QRegularExpression operations("([a-zA-Z]+)\\s*\\(([^)]*)\\)");auto matches=operations.globalMatch(text);
    while(matches.hasNext()){
        const auto m=matches.next();const auto fields=m.captured(2).split(QRegularExpression("[\\s,]+"),Qt::SkipEmptyParts);std::vector<double> v;for(const auto& field:fields)v.push_back(field.toDouble());
        const auto kind=m.captured(1);
        if(kind=="translate"&&!v.empty())result.translate(v[0],v.size()>1?v[1]:0);
        else if(kind=="scale"&&!v.empty())result.scale(v[0],v.size()>1?v[1]:v[0]);
        else if(kind=="rotate"&&!v.empty()){if(v.size()==3)result.translate(v[1],v[2]);result.rotate(v[0]);if(v.size()==3)result.translate(-v[1],-v[2]);}
        else if(kind=="matrix"&&v.size()==6)result=QTransform(v[0],v[1],v[2],v[3],v[4],v[5])*result;
    }return result;
}
struct Edge {QPointF a,b;double x(double y) const{return a.x()+(y-a.y())*(b.x()-a.x())/(b.y()-a.y());}};
bool tessellate(const QPainterPath& path,std::vector<Point>& mesh){
    std::vector<Edge> edges;std::vector<double> ys;
    // Flatten glyph curves in SVG font units (1000 units per em), before scaling.
    for(const auto& polygon:path.toSubpathPolygons())for(int i=0;i<polygon.size();++i){const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];if(std::abs(a.y()-b.y())<1e-9)continue;edges.push_back({a,b});ys.push_back(a.y());ys.push_back(b.y());}
    if(edges.size()>10000)return false;
    std::sort(ys.begin(),ys.end());ys.erase(std::unique(ys.begin(),ys.end(),[](double a,double b){return std::abs(a-b)<1e-9;}),ys.end());
    for(std::size_t i=1;i<ys.size();++i){
        const double top=ys[i-1],bottom=ys[i],mid=(top+bottom)/2;std::vector<const Edge*> active;
        for(const auto& edge:edges)if(mid>std::min(edge.a.y(),edge.b.y())&&mid<std::max(edge.a.y(),edge.b.y()))active.push_back(&edge);
        std::sort(active.begin(),active.end(),[mid](const auto* a,const auto* b){return a->x(mid)<b->x(mid);});
        int winding=0;const Edge* left=nullptr;
        for(const auto* edge:active){const auto before=winding;winding=path.fillRule()==Qt::OddEvenFill?(winding^1):winding+(edge->b.y()>edge->a.y()?1:-1);
            if(before==0&&winding!=0)left=edge;
            if(before!=0&&winding==0&&left){const Point a{left->x(top),top},b{edge->x(top),top},c{left->x(bottom),bottom},d{edge->x(bottom),bottom};mesh.insert(mesh.end(),{a,b,c,c,b,d});}
        }
        if(mesh.size()>2000000)return false;
    }return true;
}
}
MathMesh svgMathMesh(const std::string& svg){
    MathMesh result;if(!validMathSvg(svg)){result.error="SVG matemático inválido.";return result;}
    QXmlStreamReader xml(QString::fromStdString(svg));
    struct State {QTransform matrix;bool fill=true;double strokeWidth=0;Qt::FillRule rule=Qt::WindingFill;};State state;std::vector<State> stack;double viewX=0,viewY=0;
    while(!xml.atEnd()){
        xml.readNext();if(xml.isEndElement()){if(!stack.empty()){state=stack.back();stack.pop_back();}continue;}if(!xml.isStartElement())continue;
        stack.push_back(state);const auto attrs=xml.attributes();state.matrix=transform(attrs.value("transform").toString())*state.matrix;
        if(attrs.hasAttribute("fill"))state.fill=attrs.value("fill").toString()!="none";
        if(attrs.hasAttribute("stroke-width"))state.strokeWidth=attrs.value("stroke-width").toDouble();
        if(attrs.hasAttribute("fill-rule"))state.rule=attrs.value("fill-rule").toString()=="evenodd"?Qt::OddEvenFill:Qt::WindingFill;
        const auto name=xml.name().toString();QPainterPath path;bool consumedText=false;
        if(name=="svg"){const auto v=attrs.value("viewBox").toString().split(QRegularExpression("[\\s,]+"),Qt::SkipEmptyParts);if(v.size()==4){viewX=v[0].toDouble();viewY=v[1].toDouble();}}
        else if(name=="path"){const auto bytes=attrs.value("d").toUtf8();auto parsed=PathReader(bytes).read();if(!parsed){result.error="Caminho SVG matemático não suportado.";return result;}path=*parsed;}
        else if(name=="rect")path.addRect(attrs.value("x").toDouble(),attrs.value("y").toDouble(),attrs.value("width").toDouble(),attrs.value("height").toDouble());
        else if(name=="line"){path.moveTo(attrs.value("x1").toDouble(),attrs.value("y1").toDouble());path.lineTo(attrs.value("x2").toDouble(),attrs.value("y2").toDouble());}
        else if(name=="circle"||name=="ellipse"){const auto rx=attrs.value(name=="circle"?"r":"rx").toDouble(),ry=attrs.value(name=="circle"?"r":"ry").toDouble();path.addEllipse(QPointF(attrs.value("cx").toDouble(),attrs.value("cy").toDouble()),rx,ry);}
        else if(name=="text"){
            QFont font(attrs.value("font-family").toString());auto size=attrs.value("font-size").toString();size.remove(QRegularExpression("[^0-9.]+"));font.setPixelSize(std::clamp(size.toInt(),1,5000));
            path.addText(QPointF(attrs.value("x").toDouble(),attrs.value("y").toDouble()),font,xml.readElementText());consumedText=true;
        }else if(name=="polygon"||name=="polyline"){
            const auto fields=attrs.value("points").toString().split(QRegularExpression("[\\s,]+"),Qt::SkipEmptyParts);
            for(int i=0;i+1<fields.size();i+=2){const QPointF p(fields[i].toDouble(),fields[i+1].toDouble());if(i==0)path.moveTo(p);else path.lineTo(p);}if(name=="polygon")path.closeSubpath();
        }else if(name=="use"){result.error="O SVG precisa incorporar os caminhos dos caracteres.";return result;}
        if(path.isEmpty()){if(consumedText){state=stack.back();stack.pop_back();}continue;}path.setFillRule(state.rule);
        const auto original=path;
        if(!state.fill)path=QPainterPath();
        if(state.strokeWidth>0){QPainterPathStroker stroker;stroker.setWidth(state.strokeWidth);auto outline=stroker.createStroke(original);path.addPath(outline);}
        path=state.matrix.map(path);if(consumedText){state=stack.back();stack.pop_back();}if(!tessellate(path,result.vertices)){result.error="A fórmula excede o limite de geometria vetorial.";return result;}
    }
    for(auto& p:result.vertices){p.x=(p.x-viewX)/1000;p.y=(p.y-viewY)/1000;if(!std::isfinite(p.x)||!std::isfinite(p.y)){result.error="Coordenadas SVG inválidas.";result.vertices.clear();break;}}
    return result;
}
}
