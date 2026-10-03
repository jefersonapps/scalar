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
    void remember(const QString& id,const QString& name,const QString& path,const QString& updated);
    QVariant setting(const QString& key,const QVariant& fallback={}) const;
    void setSetting(const QString& key,const QVariant& value);
private:
    QSqlDatabase db_;
    QString connection_,error_;
};
}
