#pragma once
#include "documents/Document.h"
#include <QTabletEvent>
#include <QMouseEvent>
namespace scalar {
class InputManager {
public:
    static PointerSample tablet(const QTabletEvent& event,Point world);
    static PointerSample mouse(const QMouseEvent& event,Point world);
    PointerSample filter(PointerSample sample,bool smoothPosition=false);
    void reset(){previous_={};rawPrevious_={};velocity_={};hasPrevious_=false;}
private:
    PointerSample previous_;
    Point rawPrevious_,velocity_;
    bool hasPrevious_=false;
};
}
