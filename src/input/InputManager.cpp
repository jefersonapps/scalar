#include "InputManager.h"
#include <numbers>
namespace scalar {
PointerSample InputManager::tablet(const QTabletEvent& e,Point world){return {world,std::clamp(double(e.pressure()),0.,1.),double(e.xTilt()),double(e.yTilt()),e.rotation(),e.timestamp(),std::uint32_t(e.buttons().toInt()),DeviceType::Stylus};}
PointerSample InputManager::mouse(const QMouseEvent& e,Point world){return {world,1,0,0,0,e.timestamp(),std::uint32_t(e.buttons().toInt()),DeviceType::Mouse};}
PointerSample InputManager::filter(PointerSample s,bool smoothPosition){
    const auto raw=s.position;
    if(hasPrevious_&&smoothPosition){
        // One Euro filter: suppress slow handwriting jitter, follow fast moves.
        const double dt=s.timestamp>previous_.timestamp?std::clamp(double(s.timestamp-previous_.timestamp)/1000.,.001,.05):1./120;
        const auto alpha=[dt](double cutoff){return 1./(1.+1./(2*std::numbers::pi*cutoff*dt));};
        const double derivativeAlpha=alpha(8.);
        velocity_=velocity_*(1-derivativeAlpha)+(raw-rawPrevious_)*(derivativeAlpha/dt);
        const double positionAlpha=alpha(3.+.25*length(velocity_));
        s.position=previous_.position*(1-positionAlpha)+raw*positionAlpha;
    }
    if(hasPrevious_&&s.device==DeviceType::Stylus)s.pressure=previous_.pressure*0.25+s.pressure*0.75;
    rawPrevious_=raw;previous_=s;hasPrevious_=true;return s;
}
}
