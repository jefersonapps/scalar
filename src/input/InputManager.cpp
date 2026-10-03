#include "InputManager.h"
namespace scalar {
PointerSample InputManager::tablet(const QTabletEvent& e,Point world){return {world,std::clamp(double(e.pressure()),0.,1.),double(e.xTilt()),double(e.yTilt()),e.rotation(),e.timestamp(),std::uint32_t(e.buttons().toInt()),DeviceType::Stylus};}
PointerSample InputManager::mouse(const QMouseEvent& e,Point world){return {world,1,0,0,0,e.timestamp(),std::uint32_t(e.buttons().toInt()),DeviceType::Mouse};}
PointerSample InputManager::filter(PointerSample s){
    // Preserve coordinates for latency; low-pass only pressure to suppress jitter.
    if(hasPrevious_&&s.device==DeviceType::Stylus)s.pressure=previous_.pressure*0.25+s.pressure*0.75;
    previous_=s;hasPrevious_=true;return s;
}
}
