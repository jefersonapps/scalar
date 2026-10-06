#include "TrashStore.h"
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QFileInfo>
namespace scalar {
namespace {
QString moveFile(const QString& from,const QString& to){
    if(!QFile::exists(from))return "O arquivo original não foi encontrado.";
    if(QFile::exists(to))return "Já existe um arquivo no destino. O quadro foi preservado.";
    QDir().mkpath(QFileInfo(to).absolutePath());if(QFile::rename(from,to))return {};
    QFile input(from);QSaveFile output(to);output.setDirectWriteFallback(false);
    if(!input.open(QIODevice::ReadOnly))return input.errorString();if(!output.open(QIODevice::WriteOnly))return output.errorString();
    while(!input.atEnd()){const auto bytes=input.read(1024*1024);if(bytes.isEmpty()&&input.error()!=QFileDevice::NoError)return input.errorString();if(output.write(bytes)!=bytes.size())return output.errorString();}
    if(!output.commit())return output.errorString();input.close();
    if(!QFile::remove(from)){QFile::remove(to);return "Não foi possível remover o arquivo de origem.";}return {};
}
}
TrashResult moveProjectToTrash(const QVariantMap& e){
    if(e.value("isFolder").toBool()){
        for(const auto& project:e.value("projects").toList()){
            const auto result=moveProjectToTrash(project.toMap());
            if(!result.error.isEmpty())return {TrashAction::Move,e,result.error,{}};
        }
        return {TrashAction::Move,e,{},{}};
    }
    const auto from=e.value("originalPath").toString(),to=e.value("trashPath").toString();
    // A durable SQLite record precedes the file operation; maintenance resumes interrupted moves.
    QString error;if(!QFile::exists(to))error=moveFile(from,to);
    if(error.isEmpty()&&QFile::exists(from+".png")&&!QFile::exists(to+".png"))moveFile(from+".png",to+".png");
    return {TrashAction::Move,e,error,{}};
}
TrashResult restoreTrashedProject(const QVariantMap& e){
    if(e.value("isFolder").toBool()){
        for(const auto& project:e.value("projects").toList()){
            const auto result=restoreTrashedProject(project.toMap());
            if(!result.error.isEmpty())return {TrashAction::Restore,e,result.error,{}};
        }
        return {TrashAction::Restore,e,{},{}};
    }
    const auto from=e.value("trashPath").toString(),to=e.value("originalPath").toString();
    QString error;if(QFile::exists(from))error=moveFile(from,to);else if(!QFile::exists(to))error="O arquivo da lixeira não foi encontrado.";
    if(error.isEmpty()&&QFile::exists(from+".png")&&!QFile::exists(to+".png"))moveFile(from+".png",to+".png");return {TrashAction::Restore,e,error,{}};
}
TrashResult deleteTrashedProject(const QVariantMap& e){
    if(e.value("isFolder").toBool()){
        for(const auto& project:e.value("projects").toList()){
            const auto entry=project.toMap();
            if(QFile::exists(entry.value("originalPath").toString())&&!QFile::exists(entry.value("trashPath").toString())){
                const auto moved=moveProjectToTrash(entry);
                if(!moved.error.isEmpty())return {TrashAction::Delete,e,moved.error,{}};
            }
            const auto result=deleteTrashedProject(entry);
            if(!result.error.isEmpty())return {TrashAction::Delete,e,result.error,{}};
        }
        return {TrashAction::Delete,e,{},{}};
    }
    const auto path=e.value("trashPath").toString();
    for(const auto& file:QStringList{path+".png",path})if(QFile::exists(file)&&!QFile::remove(file))return {TrashAction::Delete,e,"Não foi possível excluir o arquivo da lixeira.",{}};
    return {TrashAction::Delete,e,{},{}};
}
TrashResult maintainTrash(const QVariantList& entries,QDateTime now){
    TrashResult result{TrashAction::Maintain,{},{},{}};
    for(const auto& value:entries){const auto e=value.toMap();const auto deleted=QDateTime::fromString(e.value("deletedAt").toString(),Qt::ISODateWithMs);if(!deleted.isValid())continue;
        const auto move=moveProjectToTrash(e);if(!move.error.isEmpty()){if(QFile::exists(e.value("trashPath").toString()))continue;result.error=move.error;continue;}
        if(deleted.addDays(30)<=now){const auto removed=deleteTrashedProject(e);if(removed.error.isEmpty())result.expired.append(e);else result.error=removed.error;}
    }return result;
}
}
