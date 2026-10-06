#pragma once
#include <QSqlDatabase>
#include <QVariantList>
namespace scalar {
class Library {
public:
    explicit Library(const QString& databasePath);
    ~Library();
    Library(const Library&)=delete;
    Library& operator=(const Library&)=delete;
    QString error() const { return error_; }
    QVariantList recent() const;
    QVariantList folders() const;
    QString saveFolder(const QString& id,const QString& name,const QString& color,const QString& parentId={});
    bool deleteFolder(const QString& id,const QString& trashDirectory={});
    bool moveToFolder(const QString& projectId,const QString& folderId);
    void remember(const QString& id,const QString& name,const QString& path,const QString& updated);
    QVariant setting(const QString& key,const QVariant& fallback={}) const;
    void setSetting(const QString& key,const QVariant& value);
    QVariantList backgroundPresets() const;
    bool saveBackgroundPreset(const QString& name,const QVariantMap& values);
    QVariantList trash() const;
    QVariantMap project(const QString& id) const;
    bool recordTrash(const QVariantMap& entry);
    bool forgetTrash(const QString& id,bool permanent);
private:
    QSqlDatabase db_;
    QString connection_,error_;
};
}
