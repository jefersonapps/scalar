#include "History.h"
namespace scalar {
void History::add(Page& page, StrokeObject stroke) {
    commands_.erase(commands_.begin()+static_cast<std::ptrdiff_t>(cursor_),commands_.end());
    page.strokes.push_back(stroke);
    commands_.push_back({std::move(stroke)});
    ++cursor_;
}
bool History::undo(Page& page) {
    if (!canUndo()) return false;
    const auto& id = commands_[cursor_-1].object.id;
    const auto it = std::find_if(page.strokes.begin(),page.strokes.end(),[&](const auto& s){return s.id==id;});
    if (it == page.strokes.end()) return false;
    page.strokes.erase(it); --cursor_; return true;
}
bool History::redo(Page& page) {
    if (!canRedo()) return false;
    page.strokes.push_back(commands_[cursor_++].object); return true;
}
void History::clear() { commands_.clear(); cursor_=0; }
}
