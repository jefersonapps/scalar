#pragma once
#include "geometry/Geometry.h"
namespace scalar {
enum class CommandKind { AddObject,DeleteObject,TransformObject,ConvertStrokeToShape,ChangeStyle };
struct ObjectChange {std::optional<CanvasObject> before,after;};
class History {
public:
    void add(Page& page,StrokeObject stroke);
    void apply(Page& page,std::vector<ObjectChange> changes,CommandKind kind);
    bool undo(Page& page);
    bool redo(Page& page);
    void clear();
    bool canUndo() const {return cursor_>0;}
    bool canRedo() const {return cursor_<commands_.size();}
private:
    struct Command {std::vector<ObjectChange> changes;CommandKind kind;};
    std::vector<Command> commands_;
    std::size_t cursor_=0;
};
}
