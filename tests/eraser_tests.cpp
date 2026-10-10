#include "tools/StrokeEraser.h"
#include "commands/History.h"
#include "tools/EraserIndex.h"
#include <random>
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
    const auto recoverable=inkInsideEraser(stroke,{50,10},{50,10},2);
    const auto paired=cutStroke(stroke,{50,10},{50,10},2);
    check(paired.visible.size()==2&&paired.removed.size()==1,"paired cut lost a half");
    check(paired.visible[0].samples.back().position==paired.removed[0].samples.front().position&&paired.removed[0].samples.back().position==paired.visible[1].samples.front().position,"paired cut boundary mismatch");
    check(recoverable.size()==1&&recoverable[0].samples.front().position==split[0].samples.back().position&&recoverable[0].samples.back().position==split[1].samples.front().position,"recoverable ink does not match removed interval");
    check(inkInsideEraser(stroke,{50,100},{50,100},2).empty(),"restore invents distant ink");
    check(recoverable[0].samples.front().pressure==split[0].samples.back().pressure,"restoration loses pressure");
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
    check(cutStroke(tap,{0,10},{0,10},1).removed.size()==1,"paired cut lost dot recovery");
    const auto sameInk=[](const auto& a,const auto& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i){if(a[i].samples.size()!=b[i].samples.size())return false;for(std::size_t j=0;j<a[i].samples.size();++j){if(length(a[i].samples[j].position-b[i].samples[j].position)>1e-8||std::abs(a[i].samples[j].pressure-b[i].samples[j].pressure)>1e-8)return false;}}return true;};
    std::mt19937 generator(1729);std::uniform_real_distribution<double> coordinate(-100,100),pressure(0,1);
    for(int trial=0;trial<100;++trial){auto curved=stroke;curved.samples.clear();for(int i=0;i<100;++i)curved.samples.push_back({{coordinate(generator),coordinate(generator)},pressure(generator)});
        const Point from{coordinate(generator),coordinate(generator)},to{coordinate(generator),coordinate(generator)};
        const auto areas=trial%2?std::span<const std::array<Point,4>>(shields):std::span<const std::array<Point,4>>{};
        const auto both=cutStroke(curved,from,to,3,areas);
        check(sameInk(both.visible,eraseStroke(curved,from,to,3,areas)),"paired visible ink differs");check(sameInk(both.removed,inkInsideEraser(curved,from,to,3,areas)),"paired restored ink differs");
    }
    EraserIndex index;std::vector<Bounds> indexed;
    for(int i=0;i<1000;++i){const double x=coordinate(generator)*20,y=coordinate(generator)*20;indexed.push_back({x,y,x+20,y+30});index.insert(std::to_string(i),indexed.back());}
    for(int trial=0;trial<100;++trial){const Point from{coordinate(generator)*20,coordinate(generator)*20},to{coordinate(generator)*20,coordinate(generator)*20};const auto matches=index.query(from,to,3);
        const Bounds area{std::min(from.x,to.x)-3,std::min(from.y,to.y)-3,std::max(from.x,to.x)+3,std::max(from.y,to.y)+3};
        for(std::size_t i=0;i<indexed.size();++i){const auto b=indexed[i];const bool touches=b.right>=area.left&&b.left<=area.right&&b.bottom>=area.top&&b.top<=area.bottom;check(matches.contains(std::to_string(i))==touches,"spatial index missed swept bounds");}
    }
    index.insert("large",{-1e18,-1e18,1e18,1e18});check(index.query({0,0},{1,1},1).contains("large"),"large bounds index fallback");index.erase("large");check(!index.query({0,0},{1,1},1).contains("large"),"stale removed bounds");
    index.insert("moved",{0,0,1,1});index.insert("moved",{100,100,101,101});check(!index.query({0,0},{1,1},1).contains("moved")&&index.query({100,100},{101,101},1).contains("moved"),"moved object index stale");
    Page page;History history;history.add(page,stroke);std::vector<ObjectChange> changes{{CanvasObject(stroke),{}}};
    for(auto s:split)changes.push_back({{},CanvasObject(s)});history.apply(page,changes,CommandKind::DeleteObject);
    check(page.strokes.size()==2,"split command");history.undo(page);check(page.strokes.size()==1&&page.strokes[0].id==stroke.id,"undo eraser");history.redo(page);check(page.strokes.size()==2,"redo eraser");
    std::cout<<"PASS: partial eraser, capsule crossing, pressure interpolation, unaffected strokes, taps, undo/redo\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
