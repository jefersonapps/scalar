#pragma once
#include "geometry/Geometry.h"
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <cmath>
#include <algorithm>
namespace scalar {
// Gesture-local broad phase; large bounds never allocate enormous grids.
class EraserIndex {
public:
    void clear(){entries_.clear();cells_.clear();large_.clear();}
    void erase(const std::string& id){
        const auto found=entries_.find(id);if(found==entries_.end())return;
        for(const auto& key:found->second.cells){auto cell=cells_.find(key);if(cell!=cells_.end()){cell->second.erase(id);if(cell->second.empty())cells_.erase(cell);}}
        large_.erase(id);entries_.erase(found);
    }
    void insert(CanvasObjectView object){
        std::visit([&](const auto* value){
            if(!value->properties.visible||value->properties.locked){erase(value->id);return;}
            auto b=bounds(CanvasObject(*value));
            if constexpr(requires {value->style;}){const double pad=value->style.maxWidthMm*.5;b={b.left-pad,b.top-pad,b.right+pad,b.bottom+pad};}
            insert(value->id,b);
        },object);
    }
    void insert(const std::string& id,Bounds bounds){
        erase(id);auto& entry=entries_[id];entry.bounds=bounds;
        const auto range=cellRange(bounds);
        if(!range||count(*range)>256){large_.insert(id);return;}
        for(int y=range->top;y<=range->bottom;++y)for(int x=range->left;x<=range->right;++x){const Key key{x,y};entry.cells.push_back(key);cells_[key].insert(id);}
    }
    std::unordered_set<std::string> query(Point from,Point to,double radius) const {
        const Bounds area{std::min(from.x,to.x)-radius,std::min(from.y,to.y)-radius,std::max(from.x,to.x)+radius,std::max(from.y,to.y)+radius};
        std::unordered_set<std::string> result;
        const auto include=[&](const std::string& id){const auto& b=entries_.at(id).bounds;if(b.right>=area.left&&b.left<=area.right&&b.bottom>=area.top&&b.top<=area.bottom)result.insert(id);};
        const auto range=cellRange(area);
        if(!range||count(*range)>1024){for(const auto& [id,entry]:entries_)include(id);return result;}
        for(const auto& id:large_)include(id);
        for(int y=range->top;y<=range->bottom;++y)for(int x=range->left;x<=range->right;++x){const auto cell=cells_.find({x,y});if(cell!=cells_.end())for(const auto& id:cell->second)include(id);}
        return result;
    }
private:
    using Key=std::pair<int,int>;
    struct Entry {Bounds bounds;std::vector<Key> cells;};
    struct Range {int left,top,right,bottom;};
    static std::optional<Range> cellRange(Bounds b){
        constexpr double limit=double(std::numeric_limits<int>::max())-2;
        const double l=std::floor(b.left/32),t=std::floor(b.top/32),r=std::floor(b.right/32),d=std::floor(b.bottom/32);
        if(!std::isfinite(l)||!std::isfinite(t)||!std::isfinite(r)||!std::isfinite(d)||std::max({std::abs(l),std::abs(t),std::abs(r),std::abs(d)})>limit)return {};
        return Range{int(l),int(t),int(r),int(d)};
    }
    static double count(Range r){return (double(r.right)-r.left+1)*(double(r.bottom)-r.top+1);}
    std::unordered_map<std::string,Entry> entries_;
    std::map<Key,std::unordered_set<std::string>> cells_;
    std::unordered_set<std::string> large_;
};
}
