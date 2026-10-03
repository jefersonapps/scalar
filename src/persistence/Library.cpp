#include "Library.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QUrl>
#include <QFile>
#include <QLoggingCategory>
#include <QJsonDocument>
Q_LOGGING_CATEGORY(libraryLog,"scalar.persistence.sqlite")
namespace scalar {
Library::Library(const QString& path):connection_(QUuid::createUuid().toString()) {
    db_=QSqlDatabase::addDatabase("QSQLITE",connection_); db_.setDatabaseName(path);
    if(!db_.open()){error_=db_.lastError().text();return;}
    QSqlQuery q(db_);
    for(const auto& sql:{"PRAGMA journal_mode=WAL","CREATE TABLE IF NOT EXISTS projects(id TEXT PRIMARY KEY,name TEXT NOT NULL,path TEXT NOT NULL,updated TEXT NOT NULL)","CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT)","CREATE TABLE IF NOT EXISTS background_presets(name TEXT PRIMARY KEY,settings TEXT NOT NULL)","CREATE TABLE IF NOT EXISTS trash(id TEXT PRIMARY KEY,name TEXT NOT NULL,originalPath TEXT NOT NULL,trashPath TEXT NOT NULL,deletedAt TEXT NOT NULL)"})
        if(!q.exec(QString::fromUtf8(sql))){error_=q.lastError().text();qCWarning(libraryLog)<<error_;}
}
Library::~Library(){db_.close();db_=QSqlDatabase();QSqlDatabase::removeDatabase(connection_);}
QVariantList Library::recent() const {
    QVariantList result; QSqlQuery q(db_);
    if(q.exec("SELECT id,name,path,updated FROM projects WHERE id NOT IN (SELECT id FROM trash) ORDER BY updated DESC LIMIT 24"))while(q.next())result.append(QVariantMap{{"id",q.value(0)},{"name",q.value(1)},{"path",q.value(2)},{"updated",q.value(3)},{"thumbnail",QFile::exists(q.value(2).toString()+".png") ? QUrl::fromLocalFile(q.value(2).toString()+".png").toString() : QString{}}});
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
QVariantList Library::backgroundPresets() const {
    QVariantList result;QSqlQuery q(db_);
    if(q.exec("SELECT name,settings FROM background_presets ORDER BY name"))while(q.next()){
        auto m=QJsonDocument::fromJson(q.value(1).toByteArray()).toVariant().toMap();m.insert("name",q.value(0));m.insert("builtin",false);result.append(m);
    }return result;
}
bool Library::saveBackgroundPreset(const QString& name,const QVariantMap& values){
    QSqlQuery q(db_);q.prepare("INSERT INTO background_presets VALUES(?,?) ON CONFLICT(name) DO UPDATE SET settings=excluded.settings");
    q.addBindValue(name);q.addBindValue(QString::fromUtf8(QJsonDocument::fromVariant(values).toJson(QJsonDocument::Compact)));return q.exec();
}
QVariantMap Library::project(const QString& id) const {
    QSqlQuery q(db_);q.prepare("SELECT name,path,updated FROM projects WHERE id=? AND id NOT IN (SELECT id FROM trash)");q.addBindValue(id);
    if(!q.exec()||!q.next())return {};return {{"id",id},{"name",q.value(0)},{"originalPath",q.value(1)},{"updated",q.value(2)}};
}
QVariantList Library::trash() const {
    QVariantList result;QSqlQuery q(db_);if(q.exec("SELECT id,name,originalPath,trashPath,deletedAt FROM trash ORDER BY deletedAt DESC"))while(q.next())result.append(QVariantMap{{"id",q.value(0)},{"name",q.value(1)},{"originalPath",q.value(2)},{"trashPath",q.value(3)},{"deletedAt",q.value(4)},{"thumbnail",QUrl::fromLocalFile(q.value(3).toString()+".png").toString()}});return result;
}
bool Library::recordTrash(const QVariantMap& e){
    QSqlQuery q(db_);q.prepare("INSERT INTO trash VALUES(?,?,?,?,?)");for(const char* key:{"id","name","originalPath","trashPath","deletedAt"})q.addBindValue(e.value(key));return q.exec();
}
bool Library::forgetTrash(const QString& id,bool permanent){
    if(!db_.transaction())return false;QSqlQuery q(db_);
    if(permanent){q.prepare("DELETE FROM projects WHERE id=?");q.addBindValue(id);if(!q.exec()){db_.rollback();return false;}}
    q.prepare("DELETE FROM trash WHERE id=?");q.addBindValue(id);if(!q.exec()){db_.rollback();return false;}return db_.commit();
}
}
