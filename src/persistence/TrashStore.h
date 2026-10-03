#pragma once
#include <QVariantMap>
#include <QVariantList>
#include <QDateTime>
namespace scalar {
enum class TrashAction { Move, Restore, Delete, Maintain };
struct TrashResult {TrashAction action;QVariantMap entry;QString error;QVariantList expired;};
TrashResult moveProjectToTrash(const QVariantMap& entry);
TrashResult restoreTrashedProject(const QVariantMap& entry);
TrashResult deleteTrashedProject(const QVariantMap& entry);
TrashResult maintainTrash(const QVariantList& entries,QDateTime now=QDateTime::currentDateTimeUtc());
}
