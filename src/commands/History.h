#pragma once
#include "geometry/Geometry.h"
namespace scalar {
enum class CommandKind { AddObject,DeleteObject,TransformObject,ConvertStrokeToShape,ChangeStyle,ChangeBackground,ChangePageSize,ChangeLayer };
struct ObjectChange {std::optional<CanvasObject> before,after;};
class History {
public:
    void add(Page& page,StrokeObject stroke);
    void apply(Page& page,std::vector<ObjectChange> changes,CommandKind kind);
    void erase(Page& page,std::vector<ObjectChange> changes,std::vector<StrokeObject> recoverable);
    void background(Page& page,std::uint32_t color,BackgroundStyle style);
    void resizePage(Page& page,PageSize size);
    bool undo(Page& page);
    bool redo(Page& page);
    void clear();
    bool canUndo() const {return cursor_>0;}
    bool canRedo() const {return cursor_<commands_.size();}
private:
    struct BackgroundChange {std::uint32_t beforeColor,afterColor;BackgroundStyle before,after;};
    struct SizeChange {PageSize before,after;};
    struct InkChange {std::vector<StrokeObject> before,after;};
    struct Command {std::vector<ObjectChange> changes;CommandKind kind;std::optional<BackgroundChange> background;std::optional<SizeChange> size;std::optional<InkChange> ink;};
    std::vector<Command> commands_;
    std::size_t cursor_=0;
};
}
