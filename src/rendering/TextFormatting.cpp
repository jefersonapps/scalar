#include "TextFormatting.h"
#include <QQuickTextDocument>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextFragment>
#include <QSyntaxHighlighter>
#include <QFontDatabase>
#include <QPointer>
#include <algorithm>
namespace scalar {
namespace {
class MathSourceHighlighter final : public QSyntaxHighlighter {
public:
    explicit MathSourceHighlighter(QTextDocument* doc,QObject* editor):QSyntaxHighlighter(doc),editor_(editor){
        setObjectName("scalarMathSourceHighlighter");
        format_.setFontFamilies({QFontDatabase::systemFont(QFontDatabase::FixedFont).family()});
        format_.setFontStyleHint(QFont::Monospace);format_.setFontFixedPitch(true);format_.setFontItalic(false);
    }
protected:
    void highlightBlock(const QString& text) override {
        int delimiter=std::max(0,previousBlockState()),start=delimiter?0:-1;
        for(int i=0;i<text.size();++i){
            if(delimiter&&text[i]=='%')break;
            if(text[i]=='\\'){++i;continue;}
            if(text[i]!='$')continue;
            const bool doubled=i+1<text.size()&&text[i+1]=='$';
            if(!delimiter){delimiter=doubled?2:1;start=i;i+=delimiter-1;}
            else if(delimiter==1||doubled){highlightMath(text,start,i+delimiter);i+=delimiter-1;delimiter=0;start=-1;}
        }
        if(delimiter)highlightMath(text,start,text.size());
        setCurrentBlockState(delimiter);
    }
private:
    void highlightMath(const QString& text,int start,int end){
        setFormat(start,end-start,format_);
        const auto background=editor_?editor_->property("syntaxBackground").value<QColor>():QColor{};
        const bool dark=!background.isValid()||background.lightnessF()<.5;
        const QColor command(dark?"#93c5fd":"#1d4ed8"),symbol(dark?"#c4b5fd":"#6d28d9"),number(dark?"#fcd34d":"#92400e"),comment(dark?"#94a3b8":"#64748b");
        const auto paint=[&](int offset,int length,const QColor& color){auto token=format_;token.setForeground(color);setFormat(offset,length,token);};
        // A linear scan of changed blocks; no LaTeX rendering or parsing runtime.
        for(int i=start;i<end;++i){
            const auto c=text[i];
            if(c=='%'){paint(i,end-i,comment);break;}
            if(c=='\\'){
                const int begin=i++;
                if(i<end&&text[i].isLetter()){while(i<end&&text[i].isLetter())++i;}
                else if(i<end)++i;
                paint(begin,i-begin,command);--i;
            }else if(c.isDigit()||(c=='.'&&i+1<end&&text[i+1].isDigit())){
                const int begin=i;while(i+1<end&&(text[i+1].isDigit()||text[i+1]=='.'))++i;
                paint(begin,i-begin+1,number);
            }else if(QStringView(u"${}[]()_^&=+-*/<>|").contains(c))paint(i,1,symbol);
        }
    }
    QTextCharFormat format_;
    QPointer<QObject> editor_;
};
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
    const auto color=range(doc,cursor.selectionStart(),cursor.selectionStart()+1).charFormat().foreground();
    return {{"bold",bold},{"italic",italic},{"color",color.style()==Qt::NoBrush?QString{}:color.color().name(QColor::HexArgb)}};
}
void formatTextSelection(QObject* wrapper,int start,int end,bool bold,bool enabled){
    auto* doc=document(wrapper);if(!doc||start==end)return;
    auto cursor=range(doc,start,end);QTextCharFormat format;
    if(bold)format.setFontWeight(enabled?QFont::Bold:QFont::Normal);else format.setFontItalic(enabled);
    cursor.mergeCharFormat(format);
}
void colorTextSelection(QObject* wrapper,int start,int end,const QColor& color){
    auto* doc=document(wrapper);if(!doc||!color.isValid())return;
    auto cursor=range(doc,start,end);QTextCharFormat format;format.setForeground(color);cursor.mergeCharFormat(format);
}
QVariantList textFormats(QObject* wrapper,bool defaultBold,bool defaultItalic){
    QVariantList result;auto* doc=document(wrapper);if(!doc)return result;
    for(auto block=doc->begin();block.isValid();block=block.next())for(auto it=block.begin();!it.atEnd();++it){
        const auto fragment=it.fragment();if(!fragment.isValid())continue;
        const auto font=fragment.charFormat().font().resolve(doc->defaultFont());
        const auto brush=fragment.charFormat().foreground();const bool colored=brush.style()!=Qt::NoBrush;
        if(font.bold()==defaultBold&&font.italic()==defaultItalic&&!colored)continue;
        QVariantMap span{{"start",fragment.position()},{"length",fragment.length()},{"bold",font.bold()},{"italic",font.italic()}};
        if(colored)span.insert("color",brush.color().name(QColor::HexArgb));
        if(!result.isEmpty()){
            auto previous=result.back().toMap();
            if(previous["start"].toInt()+previous["length"].toInt()==fragment.position()&&previous["bold"]==span["bold"]&&previous["italic"]==span["italic"]&&previous.value("color")==span.value("color")){
                previous["length"]=previous["length"].toInt()+fragment.length();result.back()=previous;continue;
            }
        }
        result.append(span);
    }
    return result;
}
void restoreTextFormats(QObject* wrapper,const QVariantList& formats){
    auto* doc=document(wrapper);if(!doc)return;
    auto all=range(doc,0,doc->characterCount()-1);all.setCharFormat(QTextCharFormat{});
    for(const auto& value:formats){const auto span=value.toMap();auto cursor=range(doc,span["start"].toInt(),span["start"].toInt()+span["length"].toInt());
        QTextCharFormat format;format.setFontWeight(span["bold"].toBool()?QFont::Bold:QFont::Normal);format.setFontItalic(span["italic"].toBool());
        if(span.contains("color"))format.setForeground(QColor(span["color"].toString()));cursor.mergeCharFormat(format);
    }
    // Additional layout formats affect editing only, leaving saved source and
    // emphasis spans intact. Qt updates changed blocks as the user types.
    auto* highlighter=doc->findChild<QSyntaxHighlighter*>("scalarMathSourceHighlighter");
    if(!highlighter)highlighter=new MathSourceHighlighter(doc,wrapper->parent());
    highlighter->rehighlight();
}
}
