#include "History.h"
namespace scalar {
void History::add(Page& page,StrokeObject s){apply(page,{{{},CanvasObject(std::move(s))}},CommandKind::AddObject);
    }
void History::apply(Page& page,std::vector<ObjectChange> changes,CommandKind kind){
    if(changes.empty())return;

    commands_.erase(commands_.begin()+static_cast<std::ptrdiff_t>(cursor_),commands_.end());

    for(const auto& c:changes){if(c.before||c.after)replaceObject(page,objectId(c.before?*c.before:*c.after),c.after);
        }
    commands_.push_back({std::move(changes),kind});
    ++cursor_;

}
bool History::undo(Page& page){
    if(!canUndo())return false;
    const auto& command=commands_[--cursor_];

    for(auto i=command.changes.rbegin();i!=command.changes.rend();++i)replaceObject(page,objectId(i->before?*i->before:*i->after),i->before);

    return true;

}
bool History::redo(Page& page){
    if(!canRedo())return false;
    for(const auto& c:commands_[cursor_++].changes)replaceObject(page,objectId(c.before?*c.before:*c.after),c.after);
    return true;

}
void History::clear(){commands_.clear();
    cursor_=0;
    }
}
