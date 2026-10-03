#include "SvgValidation.h"
#include <QXmlStreamReader>
#include <QSet>
namespace scalar {
bool validMathSvg(const std::string& svg){
    if(svg.empty()||svg.size()>8*1024*1024)return false;
    QXmlStreamReader xml(QString::fromStdString(svg));bool root=false;
    const QSet<QString> elements={"svg","g","path","defs","use","rect","line","polygon","polyline","circle","ellipse","text","title","desc"};
    while(!xml.atEnd()){
        xml.readNext();if(xml.isDTD()||xml.tokenType()==QXmlStreamReader::EntityReference)return false;
        if(!xml.isStartElement())continue;
        if(!root){if(xml.name()!=QStringLiteral("svg"))return false;root=true;}
        if(!elements.contains(xml.name().toString()))return false;
        for(const auto& attribute:xml.attributes()){
            const auto name=attribute.name().toString(),value=attribute.value().toString();
            if(name.startsWith("on",Qt::CaseInsensitive)||(name=="href"&&!value.startsWith('#'))||value.contains("url(",Qt::CaseInsensitive))return false;
        }
    }return root&&!xml.hasError();
}
}
