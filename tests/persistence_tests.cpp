#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include "persistence/ProjectStore.h"
#include "persistence/Library.h"
using namespace scalar;
class PersistenceTests : public QObject {
    Q_OBJECT
private slots:
    void saveLoad(){
        QTemporaryDir dir;QVERIFY(dir.isValid());
        Project p{newId(),"Geometria · aula 1","2026-10-03","2026-10-03",{{newId(),PageSize::a4(),0xffffffff,{{newId(),{},{{{5,6},0.2,12,-8,40,1234,1,DeviceType::Stylus},{{8,9},0.8}}}}},{newId(),PageSize::letter(true),0x214f43ff,{}}}};
        const auto path=dir.filePath("test.board");QVERIFY(ProjectStore::save(path,p).isEmpty());
        const auto loaded=ProjectStore::load(path);QVERIFY2(bool(loaded),qPrintable(loaded.error));
        QCOMPARE(ProjectStore::serialize(loaded.project),ProjectStore::serialize(p));
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto original=file.readAll();file.close();
        p.pages[0].size.widthMm=0;QVERIFY(!ProjectStore::save(path,p).isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void shapesImagesAndLegacy(){
        QTemporaryDir dir;Project p{newId(),"Formas","now","now",{{newId(),PageSize::a4(),0xffffffff,{}}}};
        ShapeObject circle;circle.id=newId();circle.kind=ShapeKind::Circle;circle.center={40,60};circle.radiusX=circle.radiusY=12;circle.fillOpacity=0.15;circle.style.pattern=LinePattern::Dotted;circle.style.dotSpacingMm=3.5;circle.fillRgba=0xcc5364ff;p.pages[0].shapes.push_back(circle);
        ShapeObject polygon;polygon.id=newId();polygon.kind=ShapeKind::Polygon;polygon.vertices={{0,0},{40,0},{40,10},{10,10},{10,40},{0,40}};polygon.fillRgba=0x167b69ff;p.pages[0].shapes.push_back(polygon);
        QImage bitmap(32,24,QImage::Format_ARGB32);bitmap.fill(Qt::red);QByteArray png;QBuffer buffer(&png);QVERIFY(buffer.open(QIODevice::WriteOnly));QVERIFY(bitmap.save(&buffer,"PNG"));
        ImageObject image;image.id=newId();image.corners={{10,10},{30,10},{30,25},{10,25}};image.pixelWidth=32;image.pixelHeight=24;image.png=std::make_shared<const std::vector<std::uint8_t>>(png.begin(),png.end());p.pages[0].images.push_back(image);
        const auto path=dir.filePath("v2.board");QVERIFY(ProjectStore::save(path,p).isEmpty());const auto loaded=ProjectStore::load(path);QVERIFY2(bool(loaded),qPrintable(loaded.error));
        QCOMPARE(ProjectStore::serialize(p),ProjectStore::serialize(loaded.project));QCOMPARE(loaded.images.value(QString::fromStdString(image.id)).size(),QSize(32,24));
        Project legacy{newId(),"Legacy","now","now",{{newId(),PageSize::a4(),0xffffffff,{{newId(),{},{{{1,2},0.5},{{3,4},1}}}}}}};
        auto root=QJsonDocument::fromJson(ProjectStore::serialize(legacy)).object();root["version"]=1;
        auto pages=root["pages"].toArray();auto page=pages[0].toObject();auto objects=page["objects"].toArray();auto stroke=objects[0].toObject();auto oldStyle=stroke["style"].toObject();for(const char* key:{"pattern","dashLengthMm","gapLengthMm","dotSpacingMm"})oldStyle.remove(key);stroke["style"]=oldStyle;stroke.remove("zIndex");stroke.remove("locked");stroke.remove("visible");objects[0]=stroke;page["objects"]=objects;pages[0]=page;root["pages"]=pages;
        const auto old=ProjectStore::deserialize(QJsonDocument(root).toJson());QVERIFY(bool(old));QCOMPARE(old.project.pages[0].strokes.size(),std::size_t(1));QCOMPARE(old.project.pages[0].strokes[0].style.pattern,LinePattern::Solid);
    }
    void corruptArchive(){
        auto data=ProjectStore::archive("{}");QString error;data[42]='!';
        QVERIFY(ProjectStore::unpack(data,error).isEmpty());QVERIFY(!error.isEmpty());
    }
    void rejectSchema(){
        QVERIFY(!ProjectStore::deserialize("{}"));
        Project p{newId(),"Test","now","now",{{newId(),PageSize::a4(),0xffffffff,{}}}};
        auto root=QJsonDocument::fromJson(ProjectStore::serialize(p)).object();root["version"]=99;
        QVERIFY(!ProjectStore::deserialize(QJsonDocument(root).toJson()));
    }
    void rejectInvalidPatterns(){
        Project p{newId(),"Styles","now","now",{{newId(),PageSize::a4(),0xffffffff,{{newId(),{},{{{1,2},1},{{3,4},1}}}}}}};
        const auto baseline=QJsonDocument::fromJson(ProjectStore::serialize(p)).object();
        for(const auto& value:std::vector<QJsonValue>{-1,3,1.5,"dashed"}){
            auto root=baseline;auto pages=root["pages"].toArray();auto page=pages[0].toObject();auto objects=page["objects"].toArray();
            auto object=objects[0].toObject();auto style=object["style"].toObject();style["pattern"]=value;object["style"]=style;
            objects[0]=object;page["objects"]=objects;pages[0]=page;root["pages"]=pages;
            QVERIFY(!ProjectStore::deserialize(QJsonDocument(root).toJson()));
        }
    }
    void sqlite(){
        QTemporaryDir dir;const auto path=dir.filePath("library.sqlite");
        {Library lib(path);QVERIFY2(lib.error().isEmpty(),qPrintable(lib.error()));lib.setSetting("theme","Dark");lib.remember("one","Aula",dir.filePath("a.board"),"2026-10-03");lib.remember("one","Aula 2",dir.filePath("a.board"),"2026-10-04");QCOMPARE(lib.recent().size(),1);}
        {Library lib(path);QCOMPARE(lib.setting("theme").toString(),QString("Dark"));QCOMPARE(lib.recent().front().toMap()["name"].toString(),QString("Aula 2"));}
    }
};
QTEST_GUILESS_MAIN(PersistenceTests)
#include "persistence_tests.moc"
