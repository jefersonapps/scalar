#include "RegionFill.h"
#include "geometry/Geometry.h"
#include "tools/StrokeEraser.h"
#include <QPainter>
#include <QPainterPath>
#include <QBuffer>
namespace scalar {
ImportedImage fillInkRegion(const Page& page,Point seed,QColor color,double gapMm,double opacity){
    const auto fail=[](const QString& message){return ImportedImage{{},{},message};};
    const auto domain=page.size.infinite?Bounds{seed.x-200,seed.y-200,seed.x+200,seed.y+200}:Bounds{0,0,page.size.widthMm,page.size.heightMm};
    const double worldWidth=domain.width(),worldHeight=domain.height();
    if(!page.size.valid()||!color.isValid()||!std::isfinite(opacity)||opacity<.01||opacity>1||!std::isfinite(gapMm)||gapMm<0||gapMm>2||!std::isfinite(seed.x)||!std::isfinite(seed.y)
        ||seed.x<=domain.left||seed.y<=domain.top||seed.x>=domain.right||seed.y>=domain.bottom)return fail("Clique dentro de uma região delimitada por traços.");
    const double scale=std::min({4.,2048/worldWidth,2048/worldHeight,
        std::sqrt(2000000./(worldWidth*worldHeight))});
    if(scale<2)return fail("A página é grande demais para preencher com precisão.");
    const int width=int(std::ceil(worldWidth*scale))+2,height=int(std::ceil(worldHeight*scale))+2;
    QImage walls(width,height,QImage::Format_RGB32);walls.fill(Qt::black);
    struct Segment{Point a,b;int owner;double distance;};
    struct Endpoint{Point point;int owner;double distance;};
    std::vector<Segment> segments;std::vector<Endpoint> endpoints;int owner=0;
    QPainter painter(&walls);painter.scale(scale,scale);painter.translate(1/scale-domain.left,1/scale-domain.top);
    const auto addPath=[&](const std::vector<Point>& points,bool closed,double inkWidth){
        if(points.size()<2)return;
        QPainterPath path;path.moveTo(points[0].x,points[0].y);
        double distance=0;
        for(std::size_t i=1;i<points.size();++i){path.lineTo(points[i].x,points[i].y);segments.push_back({points[i-1],points[i],owner,distance});distance+=length(points[i]-points[i-1]);}
        if(closed){path.closeSubpath();segments.push_back({points.back(),points.front(),owner,distance});}
        else {endpoints.push_back({points.front(),owner,0});endpoints.push_back({points.back(),owner,distance});}
        ++owner;
        painter.setPen(QPen(Qt::white,std::max(inkWidth,1.5/scale),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPath(path);
    };
    for(const auto& stroke:page.strokes)if(!stroke.marker&&stroke.properties.visible&&(stroke.style.rgba&255)>=128){
        if(stroke.samples.size()+segments.size()>30000)return fail("Há traços demais para identificar uma região com segurança.");
        std::vector<Point> points;for(const auto& sample:stroke.samples)points.push_back(sample.position);
        addPath(points,false,stroke.style.maxWidthMm);
    }
    for(const auto& shape:page.shapes)if(shape.properties.visible&&(shape.style.rgba&255)>=128){
        const auto outline=shapeOutline(shape);
        if(outline.size()+segments.size()>30000)return fail("Há contornos demais para identificar uma região com segurança.");
        const bool closed=shape.kind!=ShapeKind::Line&&shape.kind!=ShapeKind::CircularArc;
        if(shape.erasedRegions.empty())addPath(outline,closed,shape.style.maxWidthMm);
        else {
            StrokeObject border;border.id=shape.id;border.style=shape.style;for(auto point:outline)border.samples.push_back({point});
            if(closed&&!border.samples.empty())border.samples.push_back(border.samples.front());
            std::vector<StrokeObject> fragments{border};
            for(const auto& erased:shape.erasedRegions){std::vector<StrokeObject> remaining;for(const auto& fragment:fragments){auto split=eraseStroke(fragment,erased.from,erased.to,erased.radius);remaining.insert(remaining.end(),split.begin(),split.end());}if(erased.restore){auto restored=inkInsideEraser(border,erased.from,erased.to,erased.radius);remaining.insert(remaining.end(),restored.begin(),restored.end());}fragments=std::move(remaining);}
            for(const auto& fragment:fragments){std::vector<Point> points;for(const auto& sample:fragment.samples)points.push_back(sample.position);addPath(points,false,shape.style.maxWidthMm);}
        }
    }
    if(segments.empty()||segments.size()>30000||endpoints.size()>1024)return fail("Não foi possível identificar uma região segura para preencher.");
    // Bridge only short endpoint gaps. Do not thicken all walls or invent a page boundary.
    painter.setPen(QPen(Qt::white,1.5/scale,Qt::SolidLine,Qt::RoundCap));
    for(auto end:endpoints){
        const auto endpoint=end.point;
        double nearest=gapMm;std::optional<Point> target;
        for(const auto& segment:segments){
            // Adjacent segments cannot serve as their own closing bridge.
            if(length(endpoint-segment.a)<1e-6||length(endpoint-segment.b)<1e-6)continue;
            if(segment.owner==end.owner&&std::abs(segment.distance-end.distance)<gapMm*2)continue;
            const auto d=segment.b-segment.a;const double squared=d.x*d.x+d.y*d.y;if(squared<1e-12)continue;
            const auto offset=endpoint-segment.a;
            const auto candidate=segment.a+d*std::clamp((offset.x*d.x+offset.y*d.y)/squared,0.,1.);
            const double distance=length(candidate-endpoint);
            if(distance<nearest){nearest=distance;target=candidate;}
        }
        if(target)painter.drawLine(QPointF(endpoint.x,endpoint.y),QPointF(target->x,target->y));
    }
    painter.end();
    const int sx=int((seed.x-domain.left)*scale)+1,sy=int((seed.y-domain.top)*scale)+1;
    if(qRed(walls.pixel(sx,sy)))return fail("Clique no interior da região, longe do contorno.");
    std::vector<unsigned char> visited(std::size_t(width)*height);std::vector<int> queue;
    const int start=sy*width+sx;queue.push_back(start);visited[start]=1;
    int left=sx,right=sx,top=sy,bottom=sy;
    for(std::size_t head=0;head<queue.size();++head){
        const int index=queue[head],x=index%width,y=index/width;
        if(x<=1||y<=1||x>=width-2||y>=height-2)return fail("Região aberta: aproxime as pontas dos traços e tente novamente.");
        left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);
        for(const int neighbor:{index-1,index+1,index-width,index+width})if(!visited[neighbor]&&!qRed(walls.pixel(neighbor%width,neighbor/width))){visited[neighbor]=1;queue.push_back(neighbor);}
    }
    if(queue.size()<4||queue.size()>visited.size()*.95)return fail("Não foi possível identificar uma região segura para preencher.");
    QImage bitmap(right-left+1,bottom-top+1,QImage::Format_ARGB32);bitmap.fill(Qt::transparent);color.setAlphaF(opacity);
    for(int index:queue)bitmap.setPixelColor(index%width-left,index/width-top,color);
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);
    if(!bitmap.save(&buffer,"PNG"))return fail("Não foi possível salvar o preenchimento.");
    ImageObject object;object.id=newId();object.inkFill=true;object.pixelWidth=bitmap.width();object.pixelHeight=bitmap.height();
    object.png=std::make_shared<const std::vector<std::uint8_t>>(bytes.begin(),bytes.end());
    const double x=domain.left+(left-1)/scale,y=domain.top+(top-1)/scale,w=bitmap.width()/scale,h=bitmap.height()/scale;
    object.corners={{x,y},{x+w,y},{x+w,y+h},{x,y+h}};
    return {std::move(object),std::move(bitmap),{}};
}
}
