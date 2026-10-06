#include "TextFormatting.h"
#include <QQuickTextDocument>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextFragment>
#include <algorithm>
namespace scalar {
namespace {
QTextDocument* document(QObject* wrapper){auto* quick=qobject_cast<QQuickTextDocument*>(wrapper);return quick?quick->textDocument():nullptr;}
QTextCursor range(QTextDocument* doc,int start,int end){
    QTextCursor cursor(doc);const int limit=std::max(0,doc->characterCount()-1);
    cursor.setPosition(std::clamp(start,0,limit));cursor.setPosition(std::clamp(end,0,limit),QTextCursor::KeepAnchor);return cursor;
}
}
QVariantMap textEmphasis(QObject* wrapper,int start,int end){
    auto* doc=document(wrapper);if(!doc)return {};
    const auto cursor=range(doc,start,end);bool bold=true,italic=true;
    for(int position=cursor.selectionStart();position<std::max(cursor.selectionEnd(),cursor.selectionStart()+1);++position){
        auto character=range(doc,position,position+1);const auto font=character.charFormat().font().resolve(doc->defaultFont());
        bold&=font.bold();italic&=font.italic();
    }
    return {{"bold",bold},{"italic",italic}};
}
void formatTextSelection(QObject* wrapper,int start,int end,bool bold,bool enabled){
    auto* doc=document(wrapper);if(!doc||start==end)return;
    auto cursor=range(doc,start,end);QTextCharFormat format;
    if(bold)format.setFontWeight(enabled?QFont::Bold:QFont::Normal);else format.setFontItalic(enabled);
    cursor.mergeCharFormat(format);
}
QVariantList textFormats(QObject* wrapper,bool defaultBold,bool defaultItalic){
    QVariantList result;auto* doc=document(wrapper);if(!doc)return result;
    for(auto block=doc->begin();block.isValid();block=block.next())for(auto it=block.begin();!it.atEnd();++it){
        const auto fragment=it.fragment();if(!fragment.isValid())continue;
        const auto font=fragment.charFormat().font().resolve(doc->defaultFont());
        if(font.bold()==defaultBold&&font.italic()==defaultItalic)continue;
        result.append(QVariantMap{{"start",fragment.position()},{"length",fragment.length()},{"bold",font.bold()},{"italic",font.italic()}});
    }
    return result;
}
void restoreTextFormats(QObject* wrapper,const QVariantList& formats){
    auto* doc=document(wrapper);if(!doc)return;
    auto all=range(doc,0,doc->characterCount()-1);all.setCharFormat(QTextCharFormat{});
    for(const auto& value:formats){const auto span=value.toMap();auto cursor=range(doc,span["start"].toInt(),span["start"].toInt()+span["length"].toInt());
        QTextCharFormat format;format.setFontWeight(span["bold"].toBool()?QFont::Bold:QFont::Normal);format.setFontItalic(span["italic"].toBool());cursor.mergeCharFormat(format);
    }
}
}
