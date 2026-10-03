#include "Library.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QUrl>
#include <QFile>
#include <QLoggingCategory>
Q_LOGGING_CATEGORY(libraryLog,"scalar.persistence.sqlite")
namespace scalar {
Library::Library(const QString& path):connection_(QUuid::createUuid().toString()) {
    db_=QSqlDatabase::addDatabase("QSQLITE",connection_); db_.setDatabaseName(path);
    if(!db_.open()){error_=db_.lastError().text();return;}
    QSqlQuery q(db_);
    for(const auto& sql:{"PRAGMA journal_mode=WAL","CREATE TABLE IF NOT EXISTS projects(id TEXT PRIMARY KEY,name TEXT NOT NULL,path TEXT NOT NULL,updated TEXT NOT NULL)","CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT)"})
        if(!q.exec(QString::fromUtf8(sql))){error_=q.lastError().text();qCWarning(libraryLog)<<error_;}
}
Library::~Library(){db_.close();db_=QSqlDatabase();QSqlDatabase::removeDatabase(connection_);}
QVariantList Library::recent() const {
    QVariantList result; QSqlQuery q(db_);
    if(q.exec("SELECT id,name,path,updated FROM projects ORDER BY updated DESC LIMIT 24"))while(q.next())result.append(QVariantMap{{"id",q.value(0)},{"name",q.value(1)},{"path",q.value(2)},{"updated",q.value(3)},{"thumbnail",QFile::exists(q.value(2).toString()+".png") ? QUrl::fromLocalFile(q.value(2).toString()+".png").toString() : QString{}}});
    return result;
}
void Library::remember(const QString& id,const QString& name,const QString& path,const QString& updated) {
    QSqlQuery q(db_); q.prepare("INSERT INTO projects VALUES(?,?,?,?) ON CONFLICT(id) DO UPDATE SET name=excluded.name,path=excluded.path,updated=excluded.updated");
    q.addBindValue(id);q.addBindValue(name);q.addBindValue(path);q.addBindValue(updated);if(!q.exec())qCWarning(libraryLog)<<q.lastError().text();
}
QVariant Library::setting(const QString& key,const QVariant& fallback) const {
    QSqlQuery q(db_); q.prepare("SELECT value FROM settings WHERE key=?");q.addBindValue(key);return q.exec()&&q.next()?q.value(0):fallback;
}
void Library::setSetting(const QString& key,const QVariant& value) {
    QSqlQuery q(db_);q.prepare("INSERT INTO settings VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value");q.addBindValue(key);q.addBindValue(value);if(!q.exec())qCWarning(libraryLog)<<q.lastError().text();
}
}
