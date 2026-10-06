#include "History.h"
namespace scalar {
void History::add(Page& page,StrokeObject s){apply(page,{{{},CanvasObject(std::move(s))}},CommandKind::AddObject);
    }
void History::apply(Page& page,std::vector<ObjectChange> changes,CommandKind kind){
    if(changes.empty())return;

    commands_.erase(commands_.begin()+static_cast<std::ptrdiff_t>(cursor_),commands_.end());

    for(const auto& c:changes){if(c.before||c.after)replaceObject(page,objectId(c.before?*c.before:*c.after),c.after);
        }
    commands_.push_back({std::move(changes),kind,{}, {}});
    ++cursor_;

}
bool History::undo(Page& page){
    if(!canUndo())return false;
    const auto& command=commands_[--cursor_];
    if(command.background){page.background=command.background->beforeColor;page.backgroundStyle=command.background->before;}
    if(command.size)page.size=command.size->before;
    if(command.ink)page.erasedInk=command.ink->before;

    for(auto i=command.changes.rbegin();i!=command.changes.rend();++i)replaceObject(page,objectId(i->before?*i->before:*i->after),i->before);

    return true;

}
bool History::redo(Page& page){
    if(!canRedo())return false;
    const auto& command=commands_[cursor_++];
    if(command.background){page.background=command.background->afterColor;page.backgroundStyle=command.background->after;}
    if(command.size)page.size=command.size->after;
    if(command.ink)page.erasedInk=command.ink->after;
    for(const auto& c:command.changes)replaceObject(page,objectId(c.before?*c.before:*c.after),c.after);
    return true;

}
void History::clear(){commands_.clear();
    cursor_=0;
    }
void History::erase(Page& page,std::vector<ObjectChange> changes,std::vector<StrokeObject> recoverable){
    if(changes.empty())return;
    auto before=page.erasedInk;apply(page,std::move(changes),CommandKind::DeleteObject);
    commands_.back().ink=InkChange{std::move(before),std::move(recoverable)};
    page.erasedInk=commands_.back().ink->after;
}
void History::background(Page& page,std::uint32_t color,BackgroundStyle style){
    if(!style.valid()||(page.background==color&&page.backgroundStyle==style))return;
    commands_.erase(commands_.begin()+static_cast<std::ptrdiff_t>(cursor_),commands_.end());
    commands_.push_back({{},CommandKind::ChangeBackground,BackgroundChange{page.background,color,page.backgroundStyle,style},{}});
    ++cursor_;page.background=color;page.backgroundStyle=style;
}
void History::resizePage(Page& page,PageSize size){
    if(!size.valid()||(size.widthMm==page.size.widthMm&&size.heightMm==page.size.heightMm&&size.infinite==page.size.infinite))return;
    commands_.erase(commands_.begin()+static_cast<std::ptrdiff_t>(cursor_),commands_.end());
    commands_.push_back({{},CommandKind::ChangePageSize,{},SizeChange{page.size,size}});
    ++cursor_;page.size=size;
}
}
