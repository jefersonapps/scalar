#include "ProjectStore.h"
#include "ZipArchive.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <limits>
namespace scalar {
namespace {
constexpr qsizetype maxBytes=128*1024*1024;
QString text(const std::string& s) { return QString::fromStdString(s); }
bool finite(const QJsonValue& v) { return v.isDouble()&&std::isfinite(v.toDouble()); }
bool color(const QJsonValue& v) { return finite(v)&&v.toDouble()>=0&&v.toDouble()<=4294967295.0&&std::floor(v.toDouble())==v.toDouble(); }
}
QByteArray ProjectStore::serialize(const Project& p) {
    QJsonArray pages;
    for(const auto& page:p.pages) {
        QJsonArray strokes;
        for(const auto& s:page.strokes) {
            QJsonArray points;
            for(const auto& v:s.samples) points.append(QJsonArray{v.position.x,v.position.y,v.pressure,v.tiltX,v.tiltY,v.rotation,double(v.timestamp),double(v.buttons),int(v.device)});
            strokes.append(QJsonObject{{"id",text(s.id)},{"type","stroke"},{"style",QJsonObject{{"rgba",double(s.style.rgba)},{"minWidthMm",s.style.minWidthMm},{"maxWidthMm",s.style.maxWidthMm},{"gamma",s.style.gamma},{"sensitivity",s.style.sensitivity}}},{"samples",points}});
        }
        pages.append(QJsonObject{{"id",text(page.id)},{"widthMm",page.size.widthMm},{"heightMm",page.size.heightMm},{"background",double(page.background)},{"objects",strokes}});
    }
    return QJsonDocument(QJsonObject{{"format","scalar.board"},{"version",1},{"units","mm"},{"id",text(p.id)},{"name",text(p.name)},{"createdAt",text(p.createdAt)},{"updatedAt",text(p.updatedAt)},{"pages",pages}}).toJson(QJsonDocument::Compact);
}
LoadResult ProjectStore::deserialize(const QByteArray& data) {
    auto fail=[](const QString& reason){return LoadResult{{},reason};};
    if(data.size()>maxBytes) return fail("Projeto excede o limite de 128 MiB.");
    QJsonParseError error; const auto doc=QJsonDocument::fromJson(data,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject()) return fail("JSON inválido.");
    const auto root=doc.object();
    if(root["format"]!="scalar.board"||root["version"].toInt()!=1||root["units"]!="mm") return fail("Formato ou versão não suportado.");
    Project p; p.id=root["id"].toString().toStdString(); p.name=root["name"].toString().toStdString();
    p.createdAt=root["createdAt"].toString().toStdString(); p.updatedAt=root["updatedAt"].toString().toStdString();
    if(p.id.empty()||p.name.empty()||!root["pages"].isArray()) return fail("Metadados incompletos.");
    const auto pages=root["pages"].toArray(); if(pages.isEmpty()||pages.size()>1000) return fail("Quantidade de páginas inválida.");
    QSet<QString> ids; ids.insert(text(p.id)); qsizetype pointCount=0;
    auto claim=[&](const QString& id){if(id.isEmpty()||ids.contains(id))return false; ids.insert(id); return true;};
    for(const auto& pageValue:pages) {
        if(!pageValue.isObject()) return fail("Página inválida."); const auto o=pageValue.toObject(); Page page;
        if(!claim(o["id"].toString())||!finite(o["widthMm"])||!finite(o["heightMm"])||!color(o["background"])||!o["objects"].isArray()) return fail("Página inválida.");
        page.id=o["id"].toString().toStdString(); page.size={o["widthMm"].toDouble(),o["heightMm"].toDouble()}; page.background=std::uint32_t(o["background"].toDouble());
        if(!page.size.valid())return fail("Dimensões físicas inválidas.");
        for(const auto& value:o["objects"].toArray()) {
            const auto s=value.toObject(); const auto style=s["style"].toObject(); StrokeObject stroke;
            if(s["type"]!="stroke"||!claim(s["id"].toString())||!color(style["rgba"])||!s["samples"].isArray())return fail("Objeto inválido.");
            stroke.id=s["id"].toString().toStdString();
            for(const char* key:{"minWidthMm","maxWidthMm","gamma","sensitivity"}) if(!finite(style[key]))return fail("Estilo inválido.");
            stroke.style={std::uint32_t(style["rgba"].toDouble()),style["minWidthMm"].toDouble(),style["maxWidthMm"].toDouble(),style["gamma"].toDouble(),style["sensitivity"].toDouble()};
            const auto& st=stroke.style;
            if(st.minWidthMm<=0||st.maxWidthMm<st.minWidthMm||st.maxWidthMm>100||st.gamma<=0||st.gamma>10||st.sensitivity<=0||st.sensitivity>10) return fail("Pressão inválida.");
            const auto samples=s["samples"].toArray(); if(samples.isEmpty()) return fail("Traço vazio.");
            pointCount+=samples.size(); if(pointCount>2000000)return fail("Projeto excede 2 milhões de amostras.");
            for(const auto& sample:samples) {
                if(!sample.isArray())return fail("Amostra inválida."); const auto a=sample.toArray();
                if(a.size()!=9)return fail("Amostra incompleta."); for(const auto& n:a)if(!finite(n))return fail("Amostra não numérica.");
                if(std::abs(a[0].toDouble())>1e6||std::abs(a[1].toDouble())>1e6||a[2].toDouble()<0||a[2].toDouble()>1||a[6].toDouble()<0||a[6].toDouble()>9007199254740991.0||a[7].toDouble()<0||a[7].toDouble()>4294967295.0||a[8].toInt(-1)<0||a[8].toInt(-1)>2) return fail("Amostra fora dos limites.");
                stroke.samples.push_back({{a[0].toDouble(),a[1].toDouble()},a[2].toDouble(),a[3].toDouble(),a[4].toDouble(),a[5].toDouble(),std::uint64_t(a[6].toDouble()),std::uint32_t(a[7].toDouble()),DeviceType(a[8].toInt())});
            }
            page.strokes.push_back(std::move(stroke));
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
    QString error; const auto json=unpack(file.readAll(),error); if(!error.isEmpty())return {{},error}; return deserialize(json);
}
}
