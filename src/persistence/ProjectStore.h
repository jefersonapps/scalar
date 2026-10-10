#pragma once
#include "documents/Document.h"
#include <QByteArray>
#include <QString>
#include <QHash>
#include <QImage>
namespace scalar {
struct LoadResult { Project project; QString error; QHash<QString,QImage> images{}; bool compacted=false; explicit operator bool() const { return error.isEmpty(); } };
class ProjectStore {
public:
    static QString discardErasureHistory(Project& project);
    static QByteArray serialize(const Project& project);
    static LoadResult deserialize(const QByteArray& json);
    static QString save(const QString& path,const Project& project);
    static LoadResult load(const QString& path);
    static QByteArray archive(const QByteArray& json);
    static QByteArray unpack(const QByteArray& zip,QString& error);
};
}
