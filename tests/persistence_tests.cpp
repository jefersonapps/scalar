#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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
    void sqlite(){
        QTemporaryDir dir;const auto path=dir.filePath("library.sqlite");
        {Library lib(path);QVERIFY2(lib.error().isEmpty(),qPrintable(lib.error()));lib.setSetting("theme","Dark");lib.remember("one","Aula",dir.filePath("a.board"),"2026-10-03");lib.remember("one","Aula 2",dir.filePath("a.board"),"2026-10-04");QCOMPARE(lib.recent().size(),1);}
        {Library lib(path);QCOMPARE(lib.setting("theme").toString(),QString("Dark"));QCOMPARE(lib.recent().front().toMap()["name"].toString(),QString("Aula 2"));}
    }
};
QTEST_GUILESS_MAIN(PersistenceTests)
#include "persistence_tests.moc"
