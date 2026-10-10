#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QColor>
namespace scalar {
QVariantMap textEmphasis(QObject* document,int start,int end);
void formatTextSelection(QObject* document,int start,int end,bool bold,bool enabled);
void colorTextSelection(QObject* document,int start,int end,const QColor& color);
QVariantList textFormats(QObject* document,bool defaultBold,bool defaultItalic);
void restoreTextFormats(QObject* document,const QVariantList& formats);
}
