#include "ProjectStore.h"
#include "ZipArchive.h"
#include "documents/Backgrounds.h"
#include "math/SvgValidation.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QBuffer>
#include <QImageReader>
#include <limits>
namespace scalar {
namespace {
constexpr qsizetype maxBytes=128*1024*1024;
QString text(const std::string& s) { return QString::fromStdString(s); }
bool finite(const QJsonValue& v) { return v.isDouble()&&std::isfinite(v.toDouble()); }
bool color(const QJsonValue& v) { return finite(v)&&v.toDouble()>=0&&v.toDouble()<=4294967295.0&&std::floor(v.toDouble())==v.toDouble(); }
}
QByteArray ProjectStore::serialize(const Project& p) {
    QJsonArray pages;QJsonObject pdfAssets;
    for(const auto& page:p.pages) {
        QJsonArray strokes;
        std::vector<const StrokeObject*> ink;ink.reserve(page.strokes.size()+page.erasedInk.size());for(const auto& stroke:page.strokes)ink.push_back(&stroke);for(const auto& stroke:page.erasedInk)ink.push_back(&stroke);
        for(std::size_t index=0;index<ink.size();++index) {const auto& s=*ink[index];
            QJsonArray points;
            for(const auto& v:s.samples) points.append(QJsonArray{v.position.x,v.position.y,v.pressure,v.tiltX,v.tiltY,v.rotation,double(v.timestamp),double(v.buttons),int(v.device)});
            QJsonObject object{{"id",text(s.id)},{"type","stroke"},{"style",QJsonObject{{"rgba",double(s.style.rgba)},{"minWidthMm",s.style.minWidthMm},{"maxWidthMm",s.style.maxWidthMm},{"gamma",s.style.gamma},{"sensitivity",s.style.sensitivity},{"pattern",int(s.style.pattern)},{"dashLengthMm",s.style.dashLengthMm},{"gapLengthMm",s.style.gapLengthMm},{"dotSpacingMm",s.style.dotSpacingMm}}},{"samples",points},{"zIndex",double(s.properties.zIndex)},{"locked",s.properties.locked},{"visible",s.properties.visible}};
            if(!s.erasedRegions.empty()){QJsonArray regions;for(const auto& erased:s.erasedRegions){QJsonArray region{erased.from.x,erased.from.y,erased.to.x,erased.to.y,erased.radius};if(erased.restore)region.append(true);regions.append(region);}object["erasedRegions"]=regions;}
            if(s.marker)object["marker"]=true;if(index>=page.strokes.size())object["type"]="erasedStroke";strokes.append(object);
        }
        for(const auto& shape:page.shapes){
            QJsonArray vertices;for(auto p:shape.vertices)vertices.append(QJsonArray{p.x,p.y});
            const auto& st=shape.style;
            QJsonObject record{{"id",text(shape.id)},{"type","shape"},{"kind",int(shape.kind)},
                {"style",QJsonObject{{"rgba",double(st.rgba)},{"minWidthMm",st.minWidthMm},{"maxWidthMm",st.maxWidthMm},{"gamma",st.gamma},{"sensitivity",st.sensitivity},{"pattern",int(st.pattern)},{"dashLengthMm",st.dashLengthMm},{"gapLengthMm",st.gapLengthMm},{"dotSpacingMm",st.dotSpacingMm}}},
                {"vertices",vertices},{"center",QJsonArray{shape.center.x,shape.center.y}},{"radiusX",shape.radiusX},{"radiusY",shape.radiusY},{"rotation",shape.rotation},{"fillOpacity",shape.fillOpacity},{"fillRgba",shape.fillRgba?QJsonValue(double(*shape.fillRgba)):QJsonValue(QJsonValue::Null)},
                {"zIndex",double(shape.properties.zIndex)},{"locked",shape.properties.locked},{"visible",shape.properties.visible}};
            if(!shape.erasedRegions.empty()){QJsonArray regions;for(const auto& erased:shape.erasedRegions){QJsonArray region{erased.from.x,erased.from.y,erased.to.x,erased.to.y,erased.radius};if(erased.restore)region.append(true);regions.append(region);}record["erasedRegions"]=regions;}
            strokes.append(record);
        }
        for(const auto& image:page.images){
            QJsonArray corners;for(auto p:image.corners)corners.append(QJsonArray{p.x,p.y});
            const auto png=QByteArray(reinterpret_cast<const char*>(image.png->data()),qsizetype(image.png->size()));
            QJsonObject object{{"id",text(image.id)},{"type","image"},{"corners",corners},{"png",QString::fromLatin1(png.toBase64())},{"pixelWidth",image.pixelWidth},{"pixelHeight",image.pixelHeight},{"zIndex",double(image.properties.zIndex)},{"locked",image.properties.locked},{"visible",image.properties.visible}};
            if(image.inkFill)object["inkFill"]=true;
            if(!image.erasedRegions.empty()){QJsonArray regions;for(const auto& e:image.erasedRegions)regions.append(QJsonArray{e.from.x,e.from.y,e.to.x,e.to.y,e.radius,e.restore});object["erasedRegions"]=regions;}strokes.append(object);
        }
        for(const auto& t:page.texts){
            QJsonArray formats;for(const auto& f:t.formats)formats.append(QJsonObject{{"start",double(f.start)},{"length",double(f.length)},{"bold",f.bold},{"italic",f.italic}});
            QJsonArray corners,math;for(auto point:t.corners)corners.append(QJsonArray{point.x,point.y});
            for(const auto& f:t.math)math.append(QJsonObject{{"latex",text(f.latex)},{"svg",text(f.svg)},{"display",f.display},{"start",double(f.start)},{"length",double(f.length)},{"widthEm",f.widthEm},{"heightEm",f.heightEm}});
            strokes.append(QJsonObject{{"id",text(t.id)},{"type","text"},{"source",text(t.source)},{"fontFamily",text(t.fontFamily)},{"fontSizePt",t.fontSizePt},{"boxWidthMm",t.boxWidthMm},{"boxHeightMm",t.boxHeightMm},{"bold",t.bold},{"italic",t.italic},{"alignment",t.alignment},{"rgba",double(t.style.rgba)},{"corners",corners},{"math",math},{"formats",formats},{"zIndex",double(t.properties.zIndex)},{"locked",t.properties.locked},{"visible",t.properties.visible}});
        }
        QJsonObject record{{"id",text(page.id)},{"widthMm",page.size.widthMm},{"heightMm",page.size.heightMm},{"background",double(page.background)},{"backgroundStyle",QJsonObject::fromVariantMap(backgroundValues(page.background,page.backgroundStyle))},{"objects",strokes}};
        if(page.size.infinite)record.insert("infinite",true);
        if(page.pdf){const auto& pdf=*page.pdf;const auto bytes=pdf.data?QByteArray(reinterpret_cast<const char*>(pdf.data->data()),qsizetype(pdf.data->size())):QByteArray{};
            pdfAssets.insert(text(pdf.assetId),QJsonObject{{"data",QString::fromLatin1(bytes.toBase64())},{"pageCount",pdf.sourcePageCount}});
            const auto size=pdf.size.valid()?pdf.size:page.size;
            record.insert("pdf",QJsonObject{{"asset",text(pdf.assetId)},{"pageIndex",pdf.pageIndex},{"widthMm",size.widthMm},{"heightMm",size.heightMm}});
        }pages.append(record);
    }
    return QJsonDocument(QJsonObject{{"format","scalar.board"},{"version",4},{"units","mm"},{"id",text(p.id)},{"name",text(p.name)},{"createdAt",text(p.createdAt)},{"updatedAt",text(p.updatedAt)},{"pages",pages},{"pdfAssets",pdfAssets}}).toJson(QJsonDocument::Compact);
}
LoadResult ProjectStore::deserialize(const QByteArray& data) {
    auto fail=[](const QString& reason){return LoadResult{{},reason};};
    if(data.size()>maxBytes) return fail("Projeto excede o limite de 128 MiB.");
    QJsonParseError error; const auto doc=QJsonDocument::fromJson(data,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject()) return fail("JSON inválido.");
    const auto root=doc.object();
    if(root["format"]!="scalar.board"||(root["version"].toInt()<1||root["version"].toInt()>4)||root["units"]!="mm") return fail("Formato ou versão não suportado.");
    Project p; p.id=root["id"].toString().toStdString(); p.name=root["name"].toString().toStdString();
    p.createdAt=root["createdAt"].toString().toStdString(); p.updatedAt=root["updatedAt"].toString().toStdString();
    if(p.id.empty()||p.name.empty()||!root["pages"].isArray()) return fail("Metadados incompletos.");
    const auto pages=root["pages"].toArray(); if(pages.isEmpty()||pages.size()>1000) return fail("Quantidade de páginas inválida.");
    QSet<QString> ids; ids.insert(text(p.id)); qsizetype pointCount=0;
    auto claim=[&](const QString& id){if(id.isEmpty()||ids.contains(id))return false; ids.insert(id); return true;};
    QHash<QString,PdfPageObject> assets;
    if(root["version"].toInt()>=4){
        if(!root["pdfAssets"].isObject())return fail("Assets PDF inválidos.");
        const auto jsonAssets=root["pdfAssets"].toObject();
        for(auto i=jsonAssets.begin();i!=jsonAssets.end();++i){
            const auto entry=i.value().toObject();const auto count=entry["pageCount"].toInt(-1);
            const auto decoded=QByteArray::fromBase64Encoding(entry["data"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
            if(!claim(i.key())||!entry["data"].isString()||!decoded||decoded.decoded.size()>64*1024*1024||!decoded.decoded.left(1024).contains("%PDF-")||count<1||count>1000||entry["pageCount"].toDouble()!=count)return fail("PDF incorporado inválido.");
            assets.insert(i.key(),PdfPageObject{i.key().toStdString(),std::make_shared<const std::vector<std::uint8_t>>(decoded.decoded.begin(),decoded.decoded.end()),0,count});
        }
    }
    for(const auto& pageValue:pages) {
        if(!pageValue.isObject()) return fail("Página inválida."); const auto o=pageValue.toObject(); Page page;
        if(!claim(o["id"].toString())||!finite(o["widthMm"])||!finite(o["heightMm"])||!color(o["background"])||!o["objects"].isArray()) return fail("Página inválida.");
        page.id=o["id"].toString().toStdString(); page.size={o["widthMm"].toDouble(),o["heightMm"].toDouble()}; page.background=std::uint32_t(o["background"].toDouble());
        if(o.contains("backgroundStyle")){std::uint32_t bg;if(!o["backgroundStyle"].isObject()||!parseBackground(o["backgroundStyle"].toObject().toVariantMap(),bg,page.backgroundStyle)||bg!=page.background)return fail("Fundo de página inválido.");}
        if(o.contains("infinite")){if(!o["infinite"].isBool())return fail("Modo de página inválido.");page.size.infinite=o["infinite"].toBool();}
        if(!page.size.valid())return fail("Dimensões físicas inválidas.");
        if(o.contains("pdf")){
            const auto ref=o["pdf"].toObject();const auto asset=ref["asset"].toString();const int index=ref["pageIndex"].toInt(-1);
            if(root["version"].toInt()<4||!o["pdf"].isObject()||!assets.contains(asset)||index<0||index>=assets[asset].sourcePageCount||ref["pageIndex"].toDouble()!=index)return fail("Página PDF inválida.");
            page.pdf=assets[asset];page.pdf->pageIndex=index;page.pdf->size=page.size;
            if(ref.contains("widthMm")||ref.contains("heightMm")){
                if(!finite(ref["widthMm"])||!finite(ref["heightMm"]))return fail("Dimensões PDF inválidas.");
                page.pdf->size={ref["widthMm"].toDouble(),ref["heightMm"].toDouble()};
                if(!page.pdf->size.valid())return fail("Dimensões PDF inválidas.");
            }
        }
        for(const auto& value:o["objects"].toArray()) {
            const auto s=value.toObject();
            if(s["type"]=="text"){
                if(root["version"].toInt()<3||!claim(s["id"].toString())||!s["source"].isString()||!s["fontFamily"].isString()||!finite(s["fontSizePt"])||!color(s["rgba"])||!s["bold"].isBool()||!s["italic"].isBool()||!finite(s["alignment"])||!s["corners"].isArray()||!s["math"].isArray()||!finite(s["zIndex"])||std::abs(s["zIndex"].toDouble())>9007199254740991.0||std::floor(s["zIndex"].toDouble())!=s["zIndex"].toDouble()||!s["locked"].isBool()||!s["visible"].isBool())return fail("Texto inválido.");
                TextObject t;t.id=s["id"].toString().toStdString();t.source=s["source"].toString().toStdString();t.fontFamily=s["fontFamily"].toString().toStdString();t.fontSizePt=s["fontSizePt"].toDouble();t.bold=s["bold"].toBool();t.italic=s["italic"].toBool();t.alignment=s["alignment"].toInt(-1);t.style.rgba=std::uint32_t(s["rgba"].toDouble());
                if(t.source.empty()||t.source.size()>32768||t.fontFamily.size()>256||t.fontSizePt<6||t.fontSizePt>144||t.alignment<0||t.alignment>2||t.alignment!=s["alignment"].toDouble())return fail("Texto fora dos limites.");
                for(const auto* field:{"boxWidthMm","boxHeightMm"})if(s.contains(field)&&(!finite(s[field])||s[field].toDouble()<0||s[field].toDouble()>10000))return fail("Dimensões de texto inválidas.");
                t.boxWidthMm=s["boxWidthMm"].toDouble();t.boxHeightMm=s["boxHeightMm"].toDouble();
                if(s.contains("formats")){
                    if(!s["formats"].isArray())return fail("Formatação de texto inválida.");
                    std::size_t previous=0;const auto count=s["source"].toString().size();
                    for(const auto& value:s["formats"].toArray()){
                        const auto f=value.toObject();const auto start=f["start"].toDouble(),length=f["length"].toDouble();
                        if(!finite(f["start"])||!finite(f["length"])||start<previous||length<=0||start>count||length>count-start||std::floor(start)!=start||std::floor(length)!=length||!f["bold"].isBool()||!f["italic"].isBool())return fail("Formatação de texto inválida.");
                        t.formats.push_back({std::size_t(start),std::size_t(length),f["bold"].toBool(),f["italic"].toBool()});previous=std::size_t(start+length);
                    }
                }
                const auto corners=s["corners"].toArray();if(corners.size()!=4)return fail("Texto sem quatro cantos.");
                for(const auto& v:corners){const auto c=v.toArray();if(c.size()!=2||!finite(c[0])||!finite(c[1])||std::abs(c[0].toDouble())>1e6||std::abs(c[1].toDouble())>1e6)return fail("Posição de texto inválida.");t.corners.push_back({c[0].toDouble(),c[1].toDouble()});}
                std::size_t previous=0;
                for(const auto& value:s["math"].toArray()){
                    const auto m=value.toObject();MathFragment f;f.latex=m["latex"].toString().toStdString();f.svg=m["svg"].toString().toStdString();f.display=m["display"].toBool();f.widthEm=m["widthEm"].toDouble();f.heightEm=m["heightEm"].toDouble();
                    if(!finite(m["start"])||!finite(m["length"])||m["start"].toDouble()<0||m["length"].toDouble()<3||m["start"].toDouble()>32768||m["length"].toDouble()>32768||std::floor(m["start"].toDouble())!=m["start"].toDouble()||std::floor(m["length"].toDouble())!=m["length"].toDouble()||!finite(m["widthEm"])||!finite(m["heightEm"])||f.widthEm<=0||f.heightEm<=0||f.widthEm>2000||f.heightEm>2000||!validMathSvg(f.svg))return fail("Matemática inválida.");
                    f.start=std::size_t(m["start"].toDouble());f.length=std::size_t(m["length"].toDouble());
                    const std::string delimiter=f.display?"$$":"$";
                    if(f.start<previous||f.start+f.length>t.source.size()||t.source.substr(f.start,f.length)!=delimiter+f.latex+delimiter)return fail("Fonte LaTeX inconsistente.");previous=f.start+f.length;t.math.push_back(std::move(f));
                }
                t.properties={s["locked"].toBool(),s["visible"].toBool(),std::int64_t(s["zIndex"].toDouble()),0};page.texts.push_back(std::move(t));continue;
            }
            if(s["type"]=="image"){
                if(root["version"].toInt()<2||!claim(s["id"].toString())||!s["corners"].isArray()||!s["png"].isString()||!finite(s["zIndex"])||std::abs(s["zIndex"].toDouble())>9007199254740991.0||std::floor(s["zIndex"].toDouble())!=s["zIndex"].toDouble()||!s["locked"].isBool()||!s["visible"].isBool())return fail("Imagem inválida.");
                ImageObject image;image.id=s["id"].toString().toStdString();image.pixelWidth=s["pixelWidth"].toInt();image.pixelHeight=s["pixelHeight"].toInt();
                if(s.contains("inkFill")){if(!s["inkFill"].isBool())return fail("Preenchimento inválido.");image.inkFill=s["inkFill"].toBool();}
            if(s.contains("erasedRegions")){
                if(!s["erasedRegions"].isArray()||s["erasedRegions"].toArray().size()>10000)return fail("Recortes inválidos.");
                for(const auto& value:s["erasedRegions"].toArray()){
                    if(!value.isArray())return fail("Recorte inválido.");const auto region=value.toArray();if(region.size()!=5&&region.size()!=6)return fail("Recorte inválido.");
                    for(int field=0;field<5;++field)if(!finite(region[field])||std::abs(region[field].toDouble())>1e6)return fail("Recorte inválido.");
                    if(region.size()==6&&!region[5].isBool())return fail("Restauração inválida.");
                    if(region[4].toDouble()<=0||region[4].toDouble()>5000)return fail("Raio de recorte inválido.");
                    image.erasedRegions.push_back({{region[0].toDouble(),region[1].toDouble()},{region[2].toDouble(),region[3].toDouble()},region[4].toDouble(),region.size()==6&&region[5].toBool()});
                }
            }
                if(image.pixelWidth<=0||image.pixelHeight<=0||image.pixelWidth>8192||image.pixelHeight>8192||double(image.pixelWidth)*image.pixelHeight>32000000)return fail("Resolução de imagem inválida.");
                const auto corners=s["corners"].toArray();if(corners.size()!=4)return fail("Imagem sem quatro cantos.");
                for(const auto& v:corners){const auto c=v.toArray();if(c.size()!=2||!finite(c[0])||!finite(c[1])||std::abs(c[0].toDouble())>1e6||std::abs(c[1].toDouble())>1e6)return fail("Posição de imagem inválida.");image.corners.push_back({c[0].toDouble(),c[1].toDouble()});}
                const auto decoded=QByteArray::fromBase64Encoding(s["png"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);if(!decoded||decoded.decoded.size()>32*1024*1024||!decoded.decoded.startsWith(QByteArray::fromHex("89504e470d0a1a0a")))return fail("PNG inválido.");
                image.png=std::make_shared<const std::vector<std::uint8_t>>(decoded.decoded.begin(),decoded.decoded.end());image.properties={s["locked"].toBool(),s["visible"].toBool(),std::int64_t(s["zIndex"].toDouble()),0};page.images.push_back(std::move(image));continue;
            }
            const auto style=s["style"].toObject(); StrokeObject stroke;
            if((s["type"]!="stroke"&&s["type"]!="shape"&&s["type"]!="erasedStroke")||!claim(s["id"].toString())||!color(style["rgba"]))return fail("Objeto inválido.");
            stroke.id=s["id"].toString().toStdString();
            if(s.contains("marker")){if(!s["marker"].isBool())return fail("Marcador inválido.");stroke.marker=s["marker"].toBool();}
            for(const char* key:{"minWidthMm","maxWidthMm","gamma","sensitivity"}) if(!finite(style[key]))return fail("Estilo inválido.");
            stroke.style={std::uint32_t(style["rgba"].toDouble()),style["minWidthMm"].toDouble(),style["maxWidthMm"].toDouble(),style["gamma"].toDouble(),style["sensitivity"].toDouble()};
            if(style.contains("pattern")){
                if(!finite(style["pattern"])||style["pattern"].toDouble()!=style["pattern"].toInt(-1)||style["pattern"].toInt(-1)<0||style["pattern"].toInt()>2)return fail("Padrão de contorno inválido.");
                stroke.style.pattern=LinePattern(style["pattern"].toInt());
            }
            for(const auto& field:std::vector<std::pair<const char*,double*>>{{"dashLengthMm",&stroke.style.dashLengthMm},{"gapLengthMm",&stroke.style.gapLengthMm},{"dotSpacingMm",&stroke.style.dotSpacingMm}}){
                if(style.contains(field.first)){
                    if(!finite(style[field.first])||style[field.first].toDouble()<0.1||style[field.first].toDouble()>100)return fail("Espaçamento do contorno inválido.");
                    *field.second=style[field.first].toDouble();
                }
            }
            const auto& st=stroke.style;
            if(st.minWidthMm<=0||st.maxWidthMm<st.minWidthMm||st.maxWidthMm>100||st.gamma<=0||st.gamma>10||st.sensitivity<=0||st.sensitivity>10) return fail("Pressão inválida.");
            if(root["version"].toInt()>=2){
                if(!finite(s["zIndex"])||std::abs(s["zIndex"].toDouble())>9007199254740991.0||std::floor(s["zIndex"].toDouble())!=s["zIndex"].toDouble()||!s["locked"].isBool()||!s["visible"].isBool())return fail("Propriedades inválidas.");
                stroke.properties={s["locked"].toBool(),s["visible"].toBool(),std::int64_t(s["zIndex"].toDouble()),0};
            }else stroke.properties.zIndex=std::int64_t(page.strokes.size());
            if(s.contains("erasedRegions")){
                if(!s["erasedRegions"].isArray()||s["erasedRegions"].toArray().size()>10000)return fail("Recortes inválidos.");
                for(const auto& value:s["erasedRegions"].toArray()){
                    if(!value.isArray())return fail("Recorte inválido.");const auto region=value.toArray();if(region.size()!=5&&region.size()!=6)return fail("Recorte inválido.");
                    for(int field=0;field<5;++field)if(!finite(region[field])||std::abs(region[field].toDouble())>1e6)return fail("Recorte inválido.");
                    if(region.size()==6&&!region[5].isBool())return fail("Restauração inválida.");
                    if(region[4].toDouble()<=0||region[4].toDouble()>5000)return fail("Raio de recorte inválido.");
                    stroke.erasedRegions.push_back({{region[0].toDouble(),region[1].toDouble()},{region[2].toDouble(),region[3].toDouble()},region[4].toDouble(),region.size()==6&&region[5].toBool()});
                }
            }
            if(s["type"]=="shape"){
                if(root["version"].toInt()<2||!finite(s["kind"])||s["kind"].toInt(-1)<0||s["kind"].toInt(-1)>9||!s["vertices"].isArray()||!s["center"].isArray())return fail("Forma inválida.");
                ShapeObject shape;shape.id=stroke.id;shape.style=stroke.style;shape.properties=stroke.properties;shape.kind=ShapeKind(s["kind"].toInt());
                const auto c=s["center"].toArray();if(c.size()!=2||!finite(c[0])||!finite(c[1])||std::abs(c[0].toDouble())>1e6||std::abs(c[1].toDouble())>1e6)return fail("Centro inválido.");shape.center={c[0].toDouble(),c[1].toDouble()};
                for(const char* key:{"radiusX","radiusY","rotation","fillOpacity"})if(!finite(s[key]))return fail("Forma não numérica.");
                shape.radiusX=s["radiusX"].toDouble();shape.radiusY=s["radiusY"].toDouble();shape.rotation=s["rotation"].toDouble();shape.fillOpacity=s["fillOpacity"].toDouble();
                if(s.contains("fillRgba")&&!s["fillRgba"].isNull()){if(!color(s["fillRgba"]))return fail("Cor de preenchimento inválida.");shape.fillRgba=std::uint32_t(s["fillRgba"].toDouble());}
                if(shape.radiusX<=0||shape.radiusY<=0||shape.radiusX>5000||shape.radiusY>5000||shape.fillOpacity<0||shape.fillOpacity>1)return fail("Forma fora dos limites.");
                const auto v=s["vertices"].toArray();const int expected=shape.kind==ShapeKind::Line?2:shape.kind==ShapeKind::Triangle?3:(shape.kind==ShapeKind::Rectangle||shape.kind==ShapeKind::Square||shape.kind==ShapeKind::RightAngle)?4:0;
                if((shape.kind==ShapeKind::Polygon||shape.kind==ShapeKind::CircularArc||shape.kind==ShapeKind::CircularSector)?(v.size()<3||v.size()>2048):(v.size()!=expected))return fail("Vértices inválidos.");
                for(const auto& vertex:v){const auto a=vertex.toArray();if(a.size()!=2||!finite(a[0])||!finite(a[1])||std::abs(a[0].toDouble())>1e6||std::abs(a[1].toDouble())>1e6)return fail("Vértice inválido.");shape.vertices.push_back({a[0].toDouble(),a[1].toDouble()});}
                if(shape.kind==ShapeKind::Circle&&std::abs(shape.radiusX-shape.radiusY)>1e-6)return fail("Círculo inválido.");
                shape.erasedRegions=std::move(stroke.erasedRegions);
                page.shapes.push_back(std::move(shape));continue;
            }
            if(!s["samples"].isArray())return fail("Traço inválido.");
            const auto samples=s["samples"].toArray(); if(samples.isEmpty()) return fail("Traço vazio.");
            pointCount+=samples.size(); if(pointCount>2000000)return fail("Projeto excede 2 milhões de amostras.");
            for(const auto& sample:samples) {
                if(!sample.isArray())return fail("Amostra inválida."); const auto a=sample.toArray();
                if(a.size()!=9)return fail("Amostra incompleta."); for(const auto& n:a)if(!finite(n))return fail("Amostra não numérica.");
                if(std::abs(a[0].toDouble())>1e6||std::abs(a[1].toDouble())>1e6||a[2].toDouble()<0||a[2].toDouble()>1||a[6].toDouble()<0||a[6].toDouble()>9007199254740991.0||a[7].toDouble()<0||a[7].toDouble()>4294967295.0||a[8].toInt(-1)<0||a[8].toInt(-1)>2) return fail("Amostra fora dos limites.");
                stroke.samples.push_back({{a[0].toDouble(),a[1].toDouble()},a[2].toDouble(),a[3].toDouble(),a[4].toDouble(),a[5].toDouble(),std::uint64_t(a[6].toDouble()),std::uint32_t(a[7].toDouble()),DeviceType(a[8].toInt())});
            }
            if(s["type"]=="erasedStroke")page.erasedInk.push_back(std::move(stroke));else page.strokes.push_back(std::move(stroke));
        }
        p.pages.push_back(std::move(page));
    }
    return {std::move(p),{}};
}
QByteArray ProjectStore::archive(const QByteArray& json) {
    const auto bytes=packBoard(std::span(reinterpret_cast<const std::uint8_t*>(json.constData()),std::size_t(json.size())));
    return QByteArray(reinterpret_cast<const char*>(bytes.data()),qsizetype(bytes.size()));
}
QByteArray ProjectStore::unpack(const QByteArray& zip,QString& error) {
    const auto result=unpackBoard(std::span(reinterpret_cast<const std::uint8_t*>(zip.constData()),std::size_t(zip.size())));
    error=QString::fromStdString(result.error);
    return QByteArray(reinterpret_cast<const char*>(result.json.data()),qsizetype(result.json.size()));
}
QString ProjectStore::save(const QString& path,const Project& p) {
    const auto json=serialize(p); if(json.size()+122>maxBytes)return "Projeto excede o limite de armazenamento.";
    const auto validation=deserialize(json); if(!validation)return validation.error;
    const auto bytes=archive(json); QSaveFile file(path); file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly))return file.errorString();
    if(file.write(bytes)!=bytes.size())return file.errorString();
    if(!file.commit())return file.errorString(); return {};
}
LoadResult ProjectStore::load(const QString& path) {
    QFile file(path); if(!file.open(QIODevice::ReadOnly))return {{},file.errorString()};
    if(file.size()>maxBytes)return {{},"Arquivo excede 128 MiB."};
    QString error; const auto json=unpack(file.readAll(),error); if(!error.isEmpty())return {{},error}; auto result=deserialize(json);if(!result)return result;
    for(const auto& page:result.project.pages)for(const auto& image:page.images){
        auto bytes=QByteArray(reinterpret_cast<const char*>(image.png->data()),qsizetype(image.png->size()));QBuffer buffer(&bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer,"PNG");
        if(reader.size()!=QSize(image.pixelWidth,image.pixelHeight))return {{},"Dimensões PNG não correspondem aos metadados."};
        const auto decoded=reader.read();if(decoded.isNull())return {{},"PNG não pôde ser decodificado."};result.images.insert(QString::fromStdString(image.id),decoded);
    }return result;
}
}
