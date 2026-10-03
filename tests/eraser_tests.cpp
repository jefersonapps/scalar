#include "tools/StrokeEraser.h"
#include "commands/History.h"
#include <iostream>
#include <stdexcept>
using namespace scalar;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){try{
    StrokeObject stroke{newId(),{},{{{0,10},0.2},{{100,10},0.8}}};
    const auto split=eraseStroke(stroke,{50,10},{50,10},2);
    check(split.size()==2,"central eraser must leave two fragments");
    check(split[0].samples.front().position==Point{0,10}&&split[1].samples.back().position==Point{100,10},"endpoints lost");
    check(split[0].samples.back().position.x<48&&split[1].samples.front().position.x>52,"erased area not clear");
    check(split[0].samples.back().pressure>0.2&&split[0].samples.back().pressure<0.8,"pressure not interpolated");
    check(eraseStroke(stroke,{20,0},{20,20},1).size()==2,"swept crossing missed");
    check(eraseStroke(stroke,{0,100},{100,100},2)[0].id==stroke.id,"unrelated stroke changed");
    check(eraseStroke(stroke,{0,10},{100,10},2).empty(),"full coverage");
    const std::array<Point,4> shield{{{40,0},{60,0},{60,20},{40,20}}};
    const std::vector<std::array<Point,4>> shields{shield};
    const auto protectedSplit=eraseStroke(stroke,{0,10},{100,10},2,shields);
    check(protectedSplit.size()==1,"covered portion erased");
    check(protectedSplit[0].samples.front().position==Point{40,10}&&protectedSplit[0].samples.back().position==Point{60,10},"image shield boundaries");
    check(eraseStroke(stroke,{50,10},{50,10},2,shields)[0].id==stroke.id,"tap through image");
    auto rotated=shield;for(auto& point:rotated)point=rotatePoint(point,{50,10},0.5);
    const auto protectedRotated=eraseStroke(stroke,{0,10},{100,10},2,std::vector<std::array<Point,4>>{rotated});
    check(protectedRotated.size()==1&&protectedRotated[0].samples.front().position.x<50&&protectedRotated[0].samples.back().position.x>50,"rotated image shield");
    auto reverse=shield;std::reverse(reverse.begin(),reverse.end());
    check(eraseStroke(stroke,{50,10},{50,10},2,std::vector<std::array<Point,4>>{reverse})[0].id==stroke.id,"reversed polygon winding");
    auto tap=stroke;tap.samples.resize(1);check(eraseStroke(tap,{0,10},{0,10},1).empty(),"tap not erased");
    Page page;History history;history.add(page,stroke);std::vector<ObjectChange> changes{{CanvasObject(stroke),{}}};
    for(auto s:split)changes.push_back({{},CanvasObject(s)});history.apply(page,changes,CommandKind::DeleteObject);
    check(page.strokes.size()==2,"split command");history.undo(page);check(page.strokes.size()==1&&page.strokes[0].id==stroke.id,"undo eraser");history.redo(page);check(page.strokes.size()==2,"redo eraser");
    std::cout<<"PASS: partial eraser, capsule crossing, pressure interpolation, unaffected strokes, taps, undo/redo\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
