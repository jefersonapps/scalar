#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include "persistence/ProjectStore.h"
#include "persistence/Library.h"
#include "documents/Backgrounds.h"
using namespace scalar;
class PersistenceTests : public QObject {
    Q_OBJECT
private slots:
    void folderOrganizationPersists(){
        QTemporaryDir dir;const auto database=dir.filePath("folders.sqlite");QString folder;
        { Library library(database);QVERIFY(library.error().isEmpty());library.remember("board","Aula",dir.filePath("a.board"),"2026-10-03");folder=library.saveFolder({},"Geometria","#268fb5");QVERIFY(!folder.isEmpty());QVERIFY(library.saveFolder({},"geometria","#3ca889").isEmpty());QVERIFY(library.saveFolder({}," ","#268fb5").isEmpty());QVERIFY(library.saveFolder({},"Inválida","#ffffff").isEmpty());QVERIFY(!library.moveToFolder("missing",folder));QVERIFY(!library.moveToFolder("board","missing"));QVERIFY(library.moveToFolder("board",folder));QCOMPARE(library.folders()[0].toMap()["count"].toInt(),1);library.remember("board","Aula salva",dir.filePath("a.board"),"2026-10-04");QCOMPARE(library.recent()[0].toMap()["folderId"].toString(),folder);QCOMPARE(library.saveFolder(folder,"Matemática","#9765c5"),folder); }
        { Library library(database);QCOMPARE(library.folders()[0].toMap()["name"].toString(),QString("Matemática"));QCOMPARE(library.folders()[0].toMap()["color"].toString(),QString("#9765c5"));QCOMPARE(library.recent()[0].toMap()["folderId"].toString(),folder);QVERIFY(library.moveToFolder("board",{}));QVERIFY(library.recent()[0].toMap()["folderId"].toString().isEmpty());QCOMPARE(library.folders()[0].toMap()["count"].toInt(),0);const auto child=library.saveFolder({},"Álgebra","#3ca889",folder);QVERIFY(!child.isEmpty());library.remember("nested","Subpasta",dir.filePath("nested.board"),"2026-10-05");QVERIFY(library.moveToFolder("nested",child));QVariantMap childEntry;for(const auto& entry:library.folders())if(entry.toMap()["id"]==child)childEntry=entry.toMap();QCOMPARE(childEntry["parentId"].toString(),folder);QCOMPARE(childEntry["count"].toInt(),1);QVERIFY(library.deleteFolder(folder));childEntry={};for(const auto& entry:library.folders())if(entry.toMap()["id"]==child)childEntry=entry.toMap();QVERIFY(childEntry.isEmpty());QVERIFY(library.folders().isEmpty());QCOMPARE(library.trash().size(),1);QVERIFY(library.trash()[0].toMap()["isFolder"].toBool());QVERIFY(library.forgetTrash(folder,false));QCOMPARE(library.folders().size(),2);QVERIFY(!library.deleteFolder("missing"));for(int i=0;i<30;++i)library.remember(QString::number(i),"Quadro",dir.filePath(QString::number(i)),"2026-10-03");QCOMPARE(library.recent().size(),32); }
    }
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
    void backgroundsTextAndMath(){
        QTemporaryDir dir;Project p{newId(),"Texto e fundo","now","now",{{newId(),PageSize::letter(),0xffffffff,{}}}};
        auto& page=p.pages[0];page.backgroundStyle={GridType::Isometric,0xcc5364ff,0.4,0.2,7,9};
        TextObject t;t.id=newId();t.source="Área $x^2$";t.fontFamily="DejaVu Sans";t.fontSizePt=24;t.bold=true;t.italic=true;t.alignment=2;t.corners={{10,10},{80,10},{80,30},{10,30}};
        t.boxWidthMm=70;t.boxHeightMm=20;
        t.formats={{0,4,false,true}};
        t.math={{"x^2","<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1000 1000\"><path d=\"M0 0L1000 1000\"/></svg>",false,6,5,1,1}};page.texts.push_back(t);
        const auto path=dir.filePath("v3.board");QVERIFY2(ProjectStore::save(path,p).isEmpty(),"v3 save");const auto loaded=ProjectStore::load(path);QVERIFY2(bool(loaded),qPrintable(loaded.error));
        QCOMPARE(ProjectStore::serialize(loaded.project),ProjectStore::serialize(p));QCOMPARE(loaded.project.pages[0].texts[0].math[0].latex,std::string("x^2"));
        auto broken=p;broken.pages[0].texts[0].math[0].start=0;QVERIFY(!ProjectStore::deserialize(ProjectStore::serialize(broken)));
        broken=p;broken.pages[0].texts[0].formats[0].length=100;QVERIFY(!ProjectStore::deserialize(ProjectStore::serialize(broken)));
        broken=p;broken.pages[0].backgroundStyle.spacingY=0;QVERIFY(!ProjectStore::deserialize(ProjectStore::serialize(broken)));
        broken=p;broken.pages[0].texts[0].math[0].svg="<svg><image href=\"file:///tmp/private.png\"/></svg>";QVERIFY(!ProjectStore::deserialize(ProjectStore::serialize(broken)));
        {Library library(dir.filePath("presets.sqlite"));QVERIFY(library.saveBackgroundPreset("Minha grade",backgroundValues(page.background,page.backgroundStyle)));}
        {Library library(dir.filePath("presets.sqlite"));QCOMPARE(library.backgroundPresets().size(),1);std::uint32_t color;BackgroundStyle style;QVERIFY(parseBackground(library.backgroundPresets()[0].toMap(),color,style));QVERIFY(style==page.backgroundStyle);}
    }
    void corruptArchive(){
        auto data=ProjectStore::archive("{}");QString error;data[42]='!';
        QVERIFY(ProjectStore::unpack(data,error).isEmpty());QVERIFY(!error.isEmpty());
    }
    void circularArcRoundTrip(){
        Project p{newId(),"Arco","now","now",{{newId(),PageSize::a4(),0xffffffff,{}}}};
        ShapeObject arc;arc.id=newId();arc.kind=ShapeKind::CircularArc;arc.fillOpacity=0;
        arc.vertices={{20,10},{17.071,17.071},{10,20}};arc.center={10,10};arc.radiusX=arc.radiusY=10;
        p.pages[0].shapes.push_back(arc);
        ShapeObject right;right.id=newId();right.kind=ShapeKind::RightAngle;right.vertices={{20,20},{24,20},{24,24},{20,24}};right.fillOpacity=.10;p.pages[0].shapes.push_back(right);
        p.pages[0].shapes.back().erasedRegions={{{21,21},{23,23},.5}};
        const auto loaded=ProjectStore::deserialize(ProjectStore::serialize(p));QVERIFY2(bool(loaded),qPrintable(loaded.error));
        QCOMPARE(loaded.project.pages[0].shapes[0].kind,ShapeKind::CircularArc);
        QCOMPARE(loaded.project.pages[0].shapes[1].kind,ShapeKind::RightAngle);
        QCOMPARE(ProjectStore::serialize(loaded.project),ProjectStore::serialize(p));
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
