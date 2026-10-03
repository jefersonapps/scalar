#include "Backgrounds.h"
#include <QColor>
namespace scalar {
QVariantMap backgroundValues(std::uint32_t color,const BackgroundStyle& s){
    const auto name=[](QColor c){return c.name(c.alpha()==255?QColor::HexRgb:QColor::HexArgb);};
    return {{"color",name(QColor((color>>24)&255,(color>>16)&255,(color>>8)&255,color&255))},
        {"gridType",int(s.gridType)},{"gridColor",name(QColor((s.gridColor>>24)&255,(s.gridColor>>16)&255,(s.gridColor>>8)&255,s.gridColor&255))},
        {"opacity",s.opacity},{"thicknessMm",s.thicknessMm},{"spacingX",s.spacingX},{"spacingY",s.spacingY}};
}
bool parseBackground(const QVariantMap& m,std::uint32_t& color,BackgroundStyle& s){
    const QColor c(m.value("color").toString()),g(m.value("gridColor").toString());
    const auto rgba=[](QColor c){return (std::uint32_t(c.red())<<24)|(std::uint32_t(c.green())<<16)|(std::uint32_t(c.blue())<<8)|std::uint32_t(c.alpha());};
    if(!c.isValid()||!g.isValid())return false;
    bool ok=false;const auto type=m.value("gridType").toInt(&ok);if(!ok)return false;
    s.gridType=GridType(type);s.gridColor=rgba(g);
    for(const auto& field:std::vector<std::pair<const char*,double*>>{{"opacity",&s.opacity},{"thicknessMm",&s.thicknessMm},{"spacingX",&s.spacingX},{"spacingY",&s.spacingY}}){*field.second=m.value(field.first).toDouble(&ok);if(!ok)return false;}
    color=rgba(c);return s.valid();
}
QVariantList builtinBackgrounds(){
    QVariantList result;
    auto add=[&](QString name,std::uint32_t color,GridType type,double x=5,double y=5){BackgroundStyle s;s.gridType=type;s.spacingX=x;s.spacingY=y;auto m=backgroundValues(color,s);m.insert("name",name);m.insert("builtin",true);result.append(m);};
    add("Branco",0xffffffff,GridType::None);add("Preto",0x18221eff,GridType::None);add("Verde",0x214f43ff,GridType::None);
    add("Pautado",0xffffffff,GridType::Ruled,8,8);add("Quadriculado fino",0xffffffff,GridType::Square,5,5);
    add("Quadriculado médio",0xffffffff,GridType::Square,10,10);add("Pontilhado",0xffffffff,GridType::Dots);
    add("Milimetrado",0xffffffff,GridType::Millimetric,10,10);add("Isométrico",0xffffffff,GridType::Isometric,5,5);return result;
}
}
