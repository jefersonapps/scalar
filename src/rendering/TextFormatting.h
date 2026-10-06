#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
namespace scalar {
QVariantMap textEmphasis(QObject* document,int start,int end);
void formatTextSelection(QObject* document,int start,int end,bool bold,bool enabled);
QVariantList textFormats(QObject* document,bool defaultBold,bool defaultItalic);
void restoreTextFormats(QObject* document,const QVariantList& formats);
}
