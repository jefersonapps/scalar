#include "commands/History.h"
#include "rendering/StrokeMesh.h"
#include "rendering/BackgroundMesh.h"
#include <iostream>
#include <fstream>
#include "persistence/ZipArchive.h"
#include <stdexcept>
#include <set>
using namespace scalar;
void check(bool b,const char* message) { if(!b) throw std::runtime_error(message); }
void near(double a,double b) { check(std::abs(a-b)<1e-9,"numeric mismatch"); }
int main(int argc,char** argv) {
    try {
        near(PageSize::a4().widthMm,210); near(PageSize::a4().heightMm,297);
        near(PageSize::a4(true).widthMm,297); near(PageSize::a4(true).heightMm,210);
        near(PageSize::letter().widthMm,215.9); near(PageSize::letter(true).widthMm,279.4);
        check(!PageSize{0,297}.valid(),"zero size accepted");
        check(!PageSize{NAN,297}.valid(),"NaN accepted");
        ViewTransform view;
        for(double z:{0.05,1.,1.25,1.5,2.,8.}) {
            view.zoom=z; auto p=view.screenToWorld(view.worldToScreen({43.7,-12})); near(p.x,43.7); near(p.y,-12);
            const auto w=view.screenToWorld({300,240}); view.zoomAt({300,240},1.1);
            near(view.screenToWorld({300,240}).x,w.x); near(view.screenToWorld({300,240}).y,w.y);
        }
        PenStyle style; near(style.width(0),style.minWidthMm); near(style.width(1),style.maxWidthMm);
        check(style.width(0.3)<style.width(0.8),"pressure not monotonic");
        StrokeObject stroke{newId(),style,{{{0,0},0.2},{{10,0},0.8},{{10,0},1}}};
        const auto mesh=strokeMesh(stroke); check(!mesh.empty() && mesh.size()%3==0,"invalid mesh");
        for(auto p:mesh) check(std::isfinite(p.x)&&std::isfinite(p.y),"non-finite mesh");
        check(strokeMesh({newId(),style,{{{2,3},1}}}).size()==36,"tap lost");
        Page page; History history; history.add(page,stroke); history.add(page,{newId(),style,{{{5,5},1}}});
        check(history.undo(page)&&page.strokes.size()==1,"undo");
        check(history.redo(page)&&page.strokes.size()==2,"redo");
        check(history.undo(page),"undo branch"); history.add(page,{newId(),style,{{{7,7},1}}});
        check(!history.canRedo() && !history.redo(page),"redo branch retained");
        const auto stored=page.strokes.front().samples.front().position;
        view.zoomAt({100,100},2); check(page.strokes.front().samples.front().position==stored,"zoom mutated document");
        BackgroundStyle background;History backgroundHistory;Page gridPage;
        background.gridType=GridType::Square;background.spacingX=5;background.spacingY=10;
        backgroundHistory.background(gridPage,0x18221eff,background);
        check(backgroundHistory.undo(gridPage)&&gridPage.background==0xffffffff&&gridPage.backgroundStyle.gridType==GridType::None,"background undo");
        check(backgroundHistory.redo(gridPage)&&gridPage.backgroundStyle.spacingY==10,"background redo");
        for(auto type:{GridType::Ruled,GridType::Square,GridType::Dots,GridType::Millimetric,GridType::Isometric}){
            background.gridType=type;const auto grid=backgroundMesh(background,{0,0,210,297},1);
            check(!grid.empty()&&grid.size()%3==0,"invalid grid mesh");for(auto p:grid)check(std::isfinite(p.x)&&std::isfinite(p.y),"nonfinite grid");
        }
        background.spacingX=0;check(backgroundMesh(background,{0,0,210,297}).empty(),"zero grid spacing accepted");
        TextObject text;text.id=newId();text.source="Aula";text.corners={{10,10},{30,10},{30,20},{10,20}};
        check(hitTest(text,{15,15},0),"text hit test");
        const auto moved=transformed(text,{},{5,8},2,2);near(bounds(moved).left,25);near(bounds(moved).top,28);
        History textHistory;Page textPage;textHistory.apply(textPage,{{{},CanvasObject(text)}},CommandKind::AddObject);
        check(textPage.texts.size()==1&&textHistory.undo(textPage)&&textPage.texts.empty()&&textHistory.redo(textPage)&&textPage.texts.size()==1,"text history");
        const std::string fixture="{\"format\":\"scalar.board\",\"version\":1,\"units\":\"mm\"}";
        const Bytes payload(fixture.begin(),fixture.end());
        const auto archive=packBoard(payload); const auto decoded=unpackBoard(archive);
        check(bool(decoded)&&decoded.json==payload,"ZIP roundtrip");
        for(std::size_t n=0;n<archive.size();++n)check(!unpackBoard(std::span(archive.data(),n)),"truncated ZIP accepted");
        auto damaged=archive;damaged[45]^=1;check(!unpackBoard(damaged),"CRC failure accepted");
        damaged=archive;damaged[18]=255;damaged[19]=255;damaged[20]=255;damaged[21]=255;check(!unpackBoard(damaged),"oversized ZIP accepted");
        damaged=archive;damaged[8]=8;check(!unpackBoard(damaged),"unsupported compression accepted");
        std::ofstream out(argc>1?argv[1]:"fixture.board",std::ios::binary);check(bool(out),"fixture file open failed");out.write(reinterpret_cast<const char*>(archive.data()),std::streamsize(archive.size()));
        std::set<std::string> ids; for(int i=0;i<10000;++i) check(ids.insert(newId()).second,"duplicate id");
        std::cout << "PASS: physical sizes, orientation, coordinate transforms, zoom anchor, pressure, mesh, taps, undo/redo, IDs, ZIP, CRC, truncation\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
