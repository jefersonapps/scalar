#pragma once
#include "geometry/Geometry.h"
namespace scalar {
enum class CommandKind { AddObject,DeleteObject,TransformObject,ConvertStrokeToShape,ChangeStyle,ChangeBackground };
struct ObjectChange {std::optional<CanvasObject> before,after;};
class History {
public:
    void add(Page& page,StrokeObject stroke);
    void apply(Page& page,std::vector<ObjectChange> changes,CommandKind kind);
    void background(Page& page,std::uint32_t color,BackgroundStyle style);
    bool undo(Page& page);
    bool redo(Page& page);
    void clear();
    bool canUndo() const {return cursor_>0;}
    bool canRedo() const {return cursor_<commands_.size();}
private:
    struct BackgroundChange {std::uint32_t beforeColor,afterColor;BackgroundStyle before,after;};
    struct Command {std::vector<ObjectChange> changes;CommandKind kind;std::optional<BackgroundChange> background;};
    std::vector<Command> commands_;
    std::size_t cursor_=0;
};
}
