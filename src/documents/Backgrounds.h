#pragma once
#include "Document.h"
#include <QVariantMap>
#include <QVariantList>
namespace scalar {
QVariantMap backgroundValues(std::uint32_t color,const BackgroundStyle& style);
bool parseBackground(const QVariantMap& values,std::uint32_t& color,BackgroundStyle& style);
QVariantList builtinBackgrounds();
}
