#pragma once
#include "geometry/Geometry.h"
namespace scalar {
class SelectionModel {
public:
    const std::vector<std::string>& ids() const{return ids_;}
    void clear(){ids_.clear();}
    void select(const std::string& id,bool toggle=false){
        if(!toggle){ids_={id};return;}const auto it=std::find(ids_.begin(),ids_.end(),id);if(it==ids_.end())ids_.push_back(id);else ids_.erase(it);
    }
    bool contains(const std::string& id) const{return std::find(ids_.begin(),ids_.end(),id)!=ids_.end();}
    void prune(const Page& page){std::erase_if(ids_,[&](const auto& id){return !findObject(page,id).has_value();});}
    void marquee(const Page& page,Bounds box,bool additive){
        if(!additive)clear();for(const auto& object:objects(page)){const auto& p=properties(object);const auto b=bounds(object);
            if(!p.locked&&p.visible&&box.contains({b.left,b.top})&&box.contains({b.right,b.bottom})&&!contains(objectId(object)))ids_.push_back(objectId(object));}
    }
private:
    std::vector<std::string> ids_;
};
}
