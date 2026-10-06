#include "Library.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QLoggingCategory>
#include <QJsonDocument>
#include <algorithm>
Q_LOGGING_CATEGORY(libraryLog,"scalar.persistence.sqlite")
namespace scalar {
Library::Library(const QString& path):connection_(QUuid::createUuid().toString()) {
    db_=QSqlDatabase::addDatabase("QSQLITE",connection_); db_.setDatabaseName(path);
    if(!db_.open()){error_=db_.lastError().text();return;}
    QSqlQuery q(db_);
    for(const auto& sql:{"PRAGMA journal_mode=WAL","CREATE TABLE IF NOT EXISTS projects(id TEXT PRIMARY KEY,name TEXT NOT NULL,path TEXT NOT NULL,updated TEXT NOT NULL)","CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT)","CREATE TABLE IF NOT EXISTS background_presets(name TEXT PRIMARY KEY,settings TEXT NOT NULL)","CREATE TABLE IF NOT EXISTS trash(id TEXT PRIMARY KEY,name TEXT NOT NULL,originalPath TEXT NOT NULL,trashPath TEXT NOT NULL,deletedAt TEXT NOT NULL)"})
        if(!q.exec(QString::fromUtf8(sql))){error_=q.lastError().text();qCWarning(libraryLog)<<error_;}
    if(!q.exec("PRAGMA user_version")||!q.next()){error_=q.lastError().text();return;}
    if(q.value(0).toInt()<1){
        if(!db_.transaction()){error_=db_.lastError().text();return;}
        for(const auto& sql:{"CREATE TABLE IF NOT EXISTS folders(id TEXT PRIMARY KEY,name TEXT NOT NULL UNIQUE COLLATE NOCASE,color TEXT NOT NULL)","CREATE TABLE IF NOT EXISTS project_folders(project_id TEXT PRIMARY KEY,folder_id TEXT NOT NULL)","PRAGMA user_version=1"})
            if(!q.exec(QString::fromUtf8(sql))){error_=q.lastError().text();db_.rollback();qCWarning(libraryLog)<<error_;return;}
        if(!db_.commit())error_=db_.lastError().text();
    }
    if(error_.isEmpty() && q.exec("PRAGMA user_version") && q.next() && q.value(0).toInt()<2){
        if(!db_.transaction()){error_=db_.lastError().text();return;}
        for(const auto& sql:{"ALTER TABLE folders ADD COLUMN parent_id TEXT NOT NULL DEFAULT ''","CREATE INDEX IF NOT EXISTS folders_parent_idx ON folders(parent_id)","PRAGMA user_version=2"})
            if(!q.exec(QString::fromUtf8(sql))){error_=q.lastError().text();db_.rollback();qCWarning(libraryLog)<<error_;return;}
        if(!db_.commit())error_=db_.lastError().text();
    }
    if(error_.isEmpty() && q.exec("PRAGMA user_version") && q.next() && q.value(0).toInt()<3){
        if(!db_.transaction()){error_=db_.lastError().text();return;}
        for(const auto& sql:{"CREATE TABLE trash_folders(id TEXT PRIMARY KEY,name TEXT NOT NULL,deletedAt TEXT NOT NULL)","CREATE TABLE trashed_folders(folder_id TEXT PRIMARY KEY,root_id TEXT NOT NULL)","CREATE TABLE trash_folder_members(project_id TEXT PRIMARY KEY,root_id TEXT NOT NULL)","PRAGMA user_version=3"})
            if(!q.exec(QString::fromUtf8(sql))){error_=q.lastError().text();db_.rollback();return;}
        if(!db_.commit())error_=db_.lastError().text();
    }
}
Library::~Library(){db_.close();db_=QSqlDatabase();QSqlDatabase::removeDatabase(connection_);}
QVariantList Library::recent() const {
    QVariantList result; QSqlQuery q(db_);
    if(q.exec("SELECT p.id,p.name,p.path,p.updated,COALESCE(f.folder_id,'') FROM projects p LEFT JOIN project_folders f ON f.project_id=p.id WHERE p.id NOT IN (SELECT id FROM trash) ORDER BY p.updated DESC"))while(q.next())result.append(QVariantMap{{"id",q.value(0)},{"name",q.value(1)},{"path",q.value(2)},{"updated",q.value(3)},{"folderId",q.value(4)},{"thumbnail",QFile::exists(q.value(2).toString()+".png") ? QUrl::fromLocalFile(q.value(2).toString()+".png").toString() : QString{}}});
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
    QVariantList result;QSqlQuery q(db_);
    const auto entry=[](QSqlQuery& row){return QVariantMap{{"id",row.value(0)},{"name",row.value(1)},{"originalPath",row.value(2)},{"trashPath",row.value(3)},{"deletedAt",row.value(4)},{"thumbnail",QUrl::fromLocalFile(row.value(3).toString()+".png").toString()}};};
    if(q.exec("SELECT id,name,originalPath,trashPath,deletedAt FROM trash WHERE id NOT IN (SELECT project_id FROM trash_folder_members) ORDER BY deletedAt DESC"))while(q.next())result.append(entry(q));
    if(q.exec("SELECT id,name,deletedAt FROM trash_folders ORDER BY deletedAt DESC"))while(q.next()){
        QVariantList projects;QSqlQuery members(db_);members.prepare("SELECT t.id,t.name,t.originalPath,t.trashPath,t.deletedAt FROM trash t JOIN trash_folder_members m ON m.project_id=t.id WHERE m.root_id=?");members.addBindValue(q.value(0));if(members.exec())while(members.next())projects.append(entry(members));
        result.append(QVariantMap{{"id",q.value(0)},{"name",q.value(1)},{"deletedAt",q.value(2)},{"isFolder",true},{"projects",projects},{"count",projects.size()}});
    }
    std::stable_sort(result.begin(),result.end(),[](const QVariant& a,const QVariant& b){return a.toMap().value("deletedAt").toString()>b.toMap().value("deletedAt").toString();});return result;
}
bool Library::recordTrash(const QVariantMap& e){
    QSqlQuery q(db_);q.prepare("INSERT INTO trash VALUES(?,?,?,?,?)");for(const char* key:{"id","name","originalPath","trashPath","deletedAt"})q.addBindValue(e.value(key));return q.exec();
}
bool Library::forgetTrash(const QString& id,bool permanent){
    if(!db_.transaction())return false;QSqlQuery q(db_);
    q.prepare("SELECT id FROM trash_folders WHERE id=?");q.addBindValue(id);
    if(q.exec()&&q.next()){
        const auto run=[&](const QString& sql){QSqlQuery action(db_);action.prepare(sql);action.addBindValue(id);return action.exec();};
        if(permanent){
            for(const auto& sql:{"DELETE FROM project_folders WHERE project_id IN (SELECT project_id FROM trash_folder_members WHERE root_id=?)","DELETE FROM projects WHERE id IN (SELECT project_id FROM trash_folder_members WHERE root_id=?)","DELETE FROM project_folders WHERE folder_id IN (SELECT folder_id FROM trashed_folders WHERE root_id=?)","DELETE FROM folders WHERE id IN (SELECT folder_id FROM trashed_folders WHERE root_id=?)"})if(!run(QString::fromUtf8(sql))){db_.rollback();return false;}
        }else if(!run("UPDATE folders SET parent_id='' WHERE id=? AND (parent_id NOT IN (SELECT id FROM folders) OR parent_id IN (SELECT folder_id FROM trashed_folders))")){db_.rollback();return false;}
        for(const auto& sql:{"DELETE FROM trash WHERE id IN (SELECT project_id FROM trash_folder_members WHERE root_id=?)","DELETE FROM trash_folder_members WHERE root_id=?","DELETE FROM trashed_folders WHERE root_id=?","DELETE FROM trash_folders WHERE id=?"})if(!run(QString::fromUtf8(sql))){db_.rollback();return false;}
        return db_.commit();
    }
    if(!permanent){q.prepare("DELETE FROM project_folders WHERE project_id=? AND (folder_id NOT IN (SELECT id FROM folders) OR folder_id IN (SELECT folder_id FROM trashed_folders))");q.addBindValue(id);if(!q.exec()){db_.rollback();return false;}}
    if(permanent){q.prepare("DELETE FROM project_folders WHERE project_id=?");q.addBindValue(id);if(!q.exec()){db_.rollback();return false;}q.prepare("DELETE FROM projects WHERE id=?");q.addBindValue(id);if(!q.exec()){db_.rollback();return false;}}
    q.prepare("DELETE FROM trash WHERE id=?");q.addBindValue(id);if(!q.exec()){db_.rollback();return false;}return db_.commit();
}
QVariantList Library::folders() const {
    QVariantList result;QSqlQuery q(db_);
    if(q.exec("SELECT f.id,f.name,f.color,f.parent_id,(SELECT COUNT(*) FROM project_folders m JOIN projects p ON p.id=m.project_id WHERE m.folder_id=f.id AND p.id NOT IN (SELECT id FROM trash)),(SELECT COUNT(*) FROM folders c WHERE c.parent_id=f.id AND c.id NOT IN (SELECT folder_id FROM trashed_folders)) FROM folders f WHERE f.id NOT IN (SELECT folder_id FROM trashed_folders) ORDER BY f.name COLLATE NOCASE"))while(q.next())result.append(QVariantMap{{"id",q.value(0)},{"name",q.value(1)},{"color",q.value(2)},{"parentId",q.value(3)},{"count",q.value(4)},{"childCount",q.value(5)}});
    return result;
}
QString Library::saveFolder(const QString& id,const QString& name,const QString& color,const QString& parentId){
    const auto label=name.trimmed();const QStringList colors{"#268fb5","#3ca889","#b98a38","#9765c5","#ce7284"};
    if(label.isEmpty()||label.size()>80||!colors.contains(color))return {};
    const auto key=id.isEmpty()?QUuid::createUuid().toString(QUuid::WithoutBraces):id;QSqlQuery q(db_);
    if(id.isEmpty()){
        if(!parentId.isEmpty()){q.prepare("SELECT id FROM folders WHERE id=? AND id NOT IN (SELECT folder_id FROM trashed_folders)");q.addBindValue(parentId);if(!q.exec()||!q.next())return {};}
        q.prepare("INSERT INTO folders(id,name,color,parent_id) VALUES(?,?,?,?)");q.addBindValue(key);q.addBindValue(label);q.addBindValue(color);q.addBindValue(parentId.isEmpty()?QStringLiteral(""):parentId);
    }else{q.prepare("UPDATE folders SET name=?,color=? WHERE id=? AND id NOT IN (SELECT folder_id FROM trashed_folders)");q.addBindValue(label);q.addBindValue(color);q.addBindValue(key);}
    if(!q.exec()||(!id.isEmpty()&&q.numRowsAffected()!=1))return {};
    return key;
}
bool Library::deleteFolder(const QString& id,const QString& trashDirectory){
    QSqlQuery q(db_);q.prepare("SELECT name FROM folders WHERE id=? AND id NOT IN (SELECT folder_id FROM trashed_folders)");q.addBindValue(id);if(!q.exec()||!q.next())return false;
    const auto name=q.value(0).toString();const auto deleted=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QStringList subtree;
    q.prepare("WITH RECURSIVE tree(id) AS (SELECT id FROM folders WHERE id=? UNION SELECT f.id FROM folders f JOIN tree t ON f.parent_id=t.id WHERE f.id NOT IN (SELECT folder_id FROM trashed_folders)) SELECT id FROM tree");q.addBindValue(id);if(!q.exec())return false;while(q.next())subtree.append(q.value(0).toString());
    const auto directory=trashDirectory.isEmpty()?QFileInfo(db_.databaseName()).absolutePath()+"/trash":trashDirectory;
    if(!db_.transaction())return false;
    const auto fail=[&]{db_.rollback();return false;};
    q.prepare("INSERT INTO trash_folders VALUES(?,?,?)");q.addBindValue(id);q.addBindValue(name);q.addBindValue(deleted);if(!q.exec())return fail();
    for(const auto& folder:subtree){
        QSqlQuery projects(db_);projects.prepare("SELECT p.id,p.name,p.path FROM projects p JOIN project_folders f ON f.project_id=p.id WHERE f.folder_id=? AND p.id NOT IN (SELECT id FROM trash)");projects.addBindValue(folder);if(!projects.exec())return fail();
        while(projects.next()){
            const auto projectId=projects.value(0).toString();
            if(!recordTrash({{"id",projectId},{"name",projects.value(1)},{"originalPath",projects.value(2)},{"trashPath",directory+"/"+id+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".board"},{"deletedAt",deleted}}))return fail();
            q.prepare("INSERT INTO trash_folder_members VALUES(?,?)");q.addBindValue(projectId);q.addBindValue(id);if(!q.exec())return fail();
        }
        q.prepare("INSERT INTO trashed_folders VALUES(?,?)");q.addBindValue(folder);q.addBindValue(id);if(!q.exec())return fail();
    }
    return db_.commit();
}
bool Library::moveToFolder(const QString& projectId,const QString& folderId){
    if(project(projectId).isEmpty())return false;QSqlQuery q(db_);
    if(folderId.isEmpty()){q.prepare("DELETE FROM project_folders WHERE project_id=?");q.addBindValue(projectId);return q.exec();}
    q.prepare("SELECT id FROM folders WHERE id=? AND id NOT IN (SELECT folder_id FROM trashed_folders)");q.addBindValue(folderId);if(!q.exec()||!q.next())return false;
    q.prepare("INSERT INTO project_folders VALUES(?,?) ON CONFLICT(project_id) DO UPDATE SET folder_id=excluded.folder_id");q.addBindValue(projectId);q.addBindValue(folderId);return q.exec();
}
}
