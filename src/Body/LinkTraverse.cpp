/** 
    \file
    \brief Implementations of the LinkTraverse class
    \author Shin'ichiro Nakaoka
*/
  
#include "LinkTraverse.h"

using namespace std;
using namespace cnoid;


LinkTraverse::LinkTraverse()
{
    numUpwardConnections = 0;
}


LinkTraverse::LinkTraverse(int size)
    : links_(size)
{
    links_.clear();
}


LinkTraverse::LinkTraverse(Link* root, bool doUpward, bool doDownward)
{
    find(root, doUpward, doDownward);
}


LinkTraverse::LinkTraverse(const LinkTraverse& org)
    : links_(org.links_),
      numUpwardConnections(org.numUpwardConnections)
{

}


LinkTraverse::~LinkTraverse()
{

}


void LinkTraverse::clear()
{
    links_.clear();
    numUpwardConnections = 0;
}

void LinkTraverse::find(Link* root, bool doUpward, bool doDownward)
{
    numUpwardConnections = 0;
    links_.clear();
    traverse(root, doUpward, doDownward, false, 0);
}


void LinkTraverse::traverse(Link* link, bool doUpward, bool doDownward, bool isUpward, Link* prev)
{
    links_.push_back(link);
    if(isUpward){
        ++numUpwardConnections;
    }

    auto parent = link->parent();
    if(doUpward && parent && (parent->body() == link->body())){
        traverse(parent, doUpward, true, true, link);
    }
    if(doDownward){
        for(Link* child = link->child(); child; child = child->sibling()){
            if(child != prev){
                traverse(child, false, true, false, 0);
            }
        }
    }
}


void LinkTraverse::append(Link* link, bool isDownward)
{
    links_.push_back(link);
    if(!isDownward){
        ++numUpwardConnections;
    }
}


bool LinkTraverse::remove(Link* link)
{
    int index = -1;
    for(size_t i=0; i < links_.size(); ++i){
        if(links_[i] == link){
            index = i;
            break;
        }
    }
    if(index >= 0){
        if(index <= numUpwardConnections){
            --numUpwardConnections;
        }
        links_.erase(links_.begin() + index);
        return true;
    }

    return false;
}


Link* LinkTraverse::prependRootAdjacentLinkToward(Link* link)
{
    if(!empty()){
        bool isUpward = true;
        auto linkToPrepend = findRootAdjacentLink(link, nullptr, links_.front(), isUpward);
        if(linkToPrepend){
            links_.insert(links_.begin(), linkToPrepend);
            if(isUpward){
                ++numUpwardConnections;
            }
            return linkToPrepend;
        }
    }
    return nullptr;
}


Link* LinkTraverse::findRootAdjacentLink(Link* link, Link* prev, Link* root, bool& isUpward)
{
    if(link == root){
        return prev;
    }
    if(isUpward){
        auto parent = link->parent();
        if(parent && parent != prev){
            auto found = findRootAdjacentLink(parent, link, root, isUpward);
            if(found){
                return found;
            }
        }
    }
    isUpward = false;
    for(auto child = link->child(); child; child = child->sibling()){
        if(child != prev){
            auto found = findRootAdjacentLink(child, link, root, isUpward);
            if(found){
                return found;
            }
        }
    }
    return nullptr;
}

    
void LinkTraverse::calcForwardKinematics(bool calcVelocity, bool calcAcceleration) const
{
    Vector3 arm;
    int i;
    for(i=1; i <= numUpwardConnections; ++i){

        Link* link = links_[i];
        const Link* child = links_[i-1];

        switch(child->jointType()){

        case Link::ROTATIONAL_JOINT:
            link->R().noalias() = child->R() * AngleAxisd(child->q(), child->a()).inverse() * child->Rb().transpose();
            arm.noalias() = link->R() * child->b();
            link->p().noalias() = child->p() - arm;

            if(calcVelocity){
                const Vector3 sw(link->R() * (child->Rb() * child->a()));
                link->w().noalias() = child->w() - child->dq() * sw;
                link->v().noalias() = child->v() - link->w().cross(arm);
                
                if(calcAcceleration){
                    link->dw().noalias() = child->dw() - child->dq() * link->w().cross(sw) - (child->ddq() * sw);
                    link->dv().noalias() = child->dv() - link->w().cross(link->w().cross(arm)) - link->dw().cross(arm);
                }
            }
            break;
            
        case Link::SLIDE_JOINT:
            link->R().noalias() = child->R() * child->Rb().transpose();
            arm.noalias() = link->R() * (child->b() + child->Rb() * (child->q() * child->d()));
            link->p().noalias() = child->p() - arm;

            if(calcVelocity){
                const Vector3 sv(link->R() * (child->Rb() * child->d()));
                link->w() = child->w();
                link->v().noalias() = child->v() - child->dq() * sv;

                if(calcAcceleration){
                    link->dw() = child->dw();
                    link->dv().noalias() =
                        child->dv() - child->w().cross(child->w().cross(arm)) - child->dw().cross(arm)
                        - 2.0 * child->dq() * child->w().cross(sv) - child->ddq() * sv;
                }
            }
            break;
            
        case Link::FIXED_JOINT:
        default:
            link->R().noalias() = child->R() * child->Rb().transpose();
            arm.noalias() = link->R() * child->b();
            link->p().noalias() = child->p() - arm;

            if(calcVelocity){
                link->w() = child->w();
                link->v().noalias() = child->v() - link->w().cross(arm);
				
                if(calcAcceleration){
                    link->dw() = child->dw();
                    link->dv().noalias() = child->dv() - child->w().cross(child->w().cross(arm)) - child->dw().cross(arm);
                }
            }
            break;
        }
    }

    const int n = links_.size();
    for( ; i < n; ++i){
        
        Link* link = links_[i];
        const Link* parent = link->parent();

        switch(link->jointType()){
            
        case Link::ROTATIONAL_JOINT:
            link->R().noalias() = parent->R() * link->Rb() * AngleAxisd(link->q(), link->a());
            arm.noalias() = parent->R() * link->b();
            link->p().noalias() = parent->p() + arm;

            if(calcVelocity){
                const Vector3 sw(parent->R() * (link->Rb() * link->a()));
                link->w().noalias() = parent->w() + sw * link->dq();
                link->v().noalias() = parent->v() + parent->w().cross(arm);

                if(calcAcceleration){
                    link->dw().noalias() = parent->dw() + link->dq() * parent->w().cross(sw) + (link->ddq() * sw);
                    link->dv().noalias() = parent->dv() + parent->w().cross(parent->w().cross(arm)) + parent->dw().cross(arm);
                }
            }
            break;
            
        case Link::SLIDE_JOINT:
            link->R().noalias() = parent->R() * link->Rb();
            arm.noalias() = parent->R() * (link->b() + link->Rb() * (link->q() * link->d()));
            link->p().noalias() = parent->p() + arm;

            if(calcVelocity){
                const Vector3 sv(parent->R() * (link->Rb() * link->d()));
                link->w() = parent->w();
                link->v().noalias() = parent->v() + sv * link->dq();

                if(calcAcceleration){
                    link->dw() = parent->dw();
                    link->dv().noalias() = parent->dv() + parent->w().cross(parent->w().cross(arm)) + parent->dw().cross(arm)
                        + 2.0 * link->dq() * parent->w().cross(sv) + link->ddq() * sv;
                }
            }
            break;

        case Link::FIXED_JOINT:
        default:
            link->R().noalias() = parent->R() * link->Rb();
            arm.noalias() = parent->R() * link->b();
            link->p().noalias() = parent->p() + arm;

            if(calcVelocity){
                link->w() = parent->w();
                link->v().noalias() = parent->v() + parent->w().cross(arm);

                if(calcAcceleration){
                    link->dw() = parent->dw();
                    link->dv().noalias() = parent->dv() +
                        parent->w().cross(parent->w().cross(arm)) + parent->dw().cross(arm);
                }
            }
            break;
        }
    }
}
