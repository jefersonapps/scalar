#pragma once
#include "documents/Document.h"
#include <QString>
namespace scalar {
// Snapshot rendering in physical millimetres, independent of the canvas viewport.
QString exportProjectPdf(const Project& project,const QString& path);
}
