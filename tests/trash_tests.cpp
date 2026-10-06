#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "persistence/TrashStore.h"
#include "persistence/Library.h"
using namespace scalar;
class TrashTests : public QObject {
    Q_OBJECT
    static void write(const QString& path,const QByteArray& bytes){QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(bytes),bytes.size());}
private slots:
    void folderTrashPreservesHierarchyAndFiles(){
        QTemporaryDir dir;const auto database=dir.filePath("library.sqlite");QString root,child,grandchild;QStringList paths;
        {
            Library library(database);QVERIFY(library.error().isEmpty());
            root=library.saveFolder({},"Raiz","#268fb5");child=library.saveFolder({},"Filha","#3ca889",root);grandchild=library.saveFolder({},"Neta","#9765c5",child);
            for(int i=0;i<3;++i){const auto id=QString::number(i),path=dir.filePath(id+".board");paths.append(path);QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("board-data");file.close();QFile thumbnail(path+".png");QVERIFY(thumbnail.open(QIODevice::WriteOnly));thumbnail.write("thumbnail-data");thumbnail.close();library.remember(id,"Quadro",path,"2026-10-04");QVERIFY(library.moveToFolder(id,QStringList{root,child,grandchild}[i]));}
            QVERIFY(library.deleteFolder(root));QVERIFY(library.folders().isEmpty());QVERIFY(library.recent().isEmpty());
            const auto trash=library.trash();QCOMPARE(trash.size(),1);const auto entry=trash[0].toMap();QVERIFY(entry["isFolder"].toBool());QCOMPARE(entry["projects"].toList().size(),3);
            QVERIFY(moveProjectToTrash(entry).error.isEmpty());
            for(const auto& value:entry["projects"].toList()){const auto project=value.toMap();QVERIFY(!QFile::exists(project["originalPath"].toString()));QFile file(project["trashPath"].toString());QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("board-data"));QVERIFY(QFile::exists(file.fileName()+".png"));}
        }
        Library library(database);QVERIFY(library.error().isEmpty());QVERIFY(library.folders().isEmpty());QCOMPARE(library.trash().size(),1);
        QVERIFY(restoreTrashedProject(library.trash()[0].toMap()).error.isEmpty());QVERIFY(library.forgetTrash(root,false));QVERIFY(library.trash().isEmpty());QCOMPARE(library.recent().size(),3);QCOMPARE(library.folders().size(),3);
        for(const auto& value:library.folders()){const auto folder=value.toMap();if(folder["id"]==child)QCOMPARE(folder["parentId"].toString(),root);if(folder["id"]==grandchild)QCOMPARE(folder["parentId"].toString(),child);}
        for(const auto& path:paths){QVERIFY(QFile::exists(path));QVERIFY(QFile::exists(path+".png"));}
        QVERIFY(library.deleteFolder(root));const auto expired=maintainTrash(library.trash(),QDateTime::currentDateTimeUtc().addDays(31));QVERIFY(expired.error.isEmpty());QCOMPARE(expired.expired.size(),1);QVERIFY(library.forgetTrash(root,true));QVERIFY(library.folders().isEmpty());QVERIFY(library.trash().isEmpty());QVERIFY(library.recent().isEmpty());
        for(const auto& path:paths)QVERIFY(!QFile::exists(path));
        const auto empty=library.saveFolder({},"Vazia","#268fb5");QVERIFY(library.deleteFolder(empty));QCOMPARE(library.trash().size(),1);QVERIFY(restoreTrashedProject(library.trash()[0].toMap()).error.isEmpty());QVERIFY(library.forgetTrash(empty,false));QCOMPARE(library.folders().size(),1);
    }
    void folderTrashResumesPartialMoves(){
        QTemporaryDir dir;Library library(dir.filePath("library.sqlite"));const auto folder=library.saveFolder({},"Pasta","#268fb5");
        const auto write=[](const QString& path){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write("data")==4;};
        for(int i=0;i<2;++i){const auto id=QString::number(i),path=dir.filePath(id+".board");if(i==0)QVERIFY(write(path));library.remember(id,"Quadro",path,"2026-10-04");QVERIFY(library.moveToFolder(id,folder));}
        QVERIFY(library.deleteFolder(folder));const auto entry=library.trash()[0].toMap();QVERIFY(!moveProjectToTrash(entry).error.isEmpty());QCOMPARE(library.trash().size(),1);
        QVERIFY(write(dir.filePath("1.board")));QVERIFY(maintainTrash(library.trash()).error.isEmpty());
        QVERIFY(restoreTrashedProject(entry).error.isEmpty());QVERIFY(library.forgetTrash(folder,false));QCOMPARE(library.recent().size(),2);QCOMPARE(library.folders().size(),1);QVERIFY(QFile::exists(dir.filePath("0.board")));QVERIFY(QFile::exists(dir.filePath("1.board")));
    }
    void restoreFolderWithMissingParent(){
        QTemporaryDir dir;Library library(dir.filePath("library.sqlite"));
        const auto parent=library.saveFolder({},"Pai","#268fb5"),child=library.saveFolder({},"Filha","#3ca889",parent);
        QVERIFY(library.deleteFolder(child));QVERIFY(library.saveFolder(child,"Alterada","#268fb5").isEmpty());
        QVERIFY(library.deleteFolder(parent));QCOMPARE(library.trash().size(),2);
        QVERIFY(library.forgetTrash(parent,true));QVERIFY(library.forgetTrash(child,false));QCOMPARE(library.folders().size(),1);
        QCOMPARE(library.folders().first().toMap()["id"].toString(),child);QCOMPARE(library.folders().first().toMap()["parentId"].toString(),QString{});
    }
    void moveRestoreAndPermanentDelete(){
        QTemporaryDir dir;const auto original=dir.filePath("a.board"),target=dir.filePath("trash/a.board");write(original,"project");write(original+".png","thumbnail");
        QVariantMap entry{{"id","a"},{"name","Aula"},{"originalPath",original},{"trashPath",target},{"deletedAt","2026-10-03T12:00:00.000Z"}};
        Library library(dir.filePath("library.sqlite"));library.remember("a","Aula",original,"now");QVERIFY(library.recordTrash(entry));QVERIFY(library.recent().isEmpty());
        QVERIFY(moveProjectToTrash(entry).error.isEmpty());QVERIFY(!QFile::exists(original));QVERIFY(QFile::exists(target));QVERIFY(QFile::exists(target+".png"));
        QVERIFY(restoreTrashedProject(entry).error.isEmpty());QVERIFY(library.forgetTrash("a",false));QCOMPARE(library.recent().size(),1);QVERIFY(QFile::exists(original));
        QVERIFY(library.recordTrash(entry));QVERIFY(moveProjectToTrash(entry).error.isEmpty());QVERIFY(deleteTrashedProject(entry).error.isEmpty());QVERIFY(library.forgetTrash("a",true));QVERIFY(library.recent().isEmpty());QVERIFY(library.trash().isEmpty());QVERIFY(!QFile::exists(target));QVERIFY(!QFile::exists(target+".png"));
    }
    void thirtyDaysAndInterruptedMove(){
        QTemporaryDir dir;const auto original=dir.filePath("a.board"),target=dir.filePath("trash/a.board");write(original,"keep me");
        QVariantMap entry{{"id","a"},{"name","Aula"},{"originalPath",original},{"trashPath",target},{"deletedAt","2026-10-03T12:00:00.000Z"}};
        const QVariantList entries{entry};auto before=maintainTrash(entries,QDateTime::fromString("2026-11-02T11:59:59.999Z",Qt::ISODateWithMs));
        QVERIFY(before.error.isEmpty());QVERIFY(before.expired.isEmpty());QVERIFY(QFile::exists(target));QVERIFY(!QFile::exists(original));
        const auto at=maintainTrash(entries,QDateTime::fromString("2026-11-02T12:00:00.000Z",Qt::ISODateWithMs));QVERIFY(at.error.isEmpty());QCOMPARE(at.expired.size(),1);QVERIFY(!QFile::exists(target));
    }
    void restorationNeverOverwrites(){
        QTemporaryDir dir;const auto original=dir.filePath("a.board"),target=dir.filePath("trash/a.board");write(original,"old project");
        const QVariantMap entry{{"originalPath",original},{"trashPath",target}};QVERIFY(moveProjectToTrash(entry).error.isEmpty());write(original,"new project");
        QVERIFY(!restoreTrashedProject(entry).error.isEmpty());QFile file(original);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("new project"));QVERIFY(QFile::exists(target));
    }
};
QTEST_GUILESS_MAIN(TrashTests)
#include "trash_tests.moc"
