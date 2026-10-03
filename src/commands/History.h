#pragma once
#include "documents/Document.h"
namespace scalar {
// Milestone 1 command: AddObject. Cursor determines undo/redo availability.
class History {
public:
    void add(Page& page, StrokeObject stroke);
    bool undo(Page& page);
    bool redo(Page& page);
    void clear();
    bool canUndo() const { return cursor_ > 0; }
    bool canRedo() const { return cursor_ < commands_.size(); }
private:
    struct AddObjectCommand { StrokeObject object; };
    std::vector<AddObjectCommand> commands_;
    std::size_t cursor_ = 0;
};
}
