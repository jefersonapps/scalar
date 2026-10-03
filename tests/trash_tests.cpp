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
