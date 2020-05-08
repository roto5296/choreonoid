#ifndef CNOID_BASE_LOCATABLE_ITEM_H
#define CNOID_BASE_LOCATABLE_ITEM_H

#include <cnoid/Signal>
#include <cnoid/EigenTypes>
#include "exportdecl.h"

namespace cnoid {

class CNOID_EXPORT LocatableItem
{
public:
    LocatableItem();
    
    enum LocationType {
        InvalidLocation,
        GlobalLocation,
        ParentRelativeLocation,
        OffsetLocation
    };
    virtual int getLocationType() const = 0;
    
    virtual Item* getCorrespondingItem();
    virtual std::string getLocationName() const;
    virtual Position getLocation() const = 0;
    virtual bool isLocationEditable() const;
    virtual void setLocationEditable(bool on);
    virtual SignalProxy<void(bool on)> sigLocationEditableChanged();
    virtual void setLocation(const Position& T) = 0;
    virtual SignalProxy<void()> sigLocationChanged() = 0;
    virtual LocatableItem* getParentLocatableItem();
    
private:
    bool isLocationEditable_;
    Signal<void(bool on)> sigLocationEditableChanged_;
};

}

#endif
