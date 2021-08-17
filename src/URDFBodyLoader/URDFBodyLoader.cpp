#include "URDFBodyLoader.h"

#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/range/size.hpp>  // becomes STL from C++17

#include <cnoid/Body>
#include <cnoid/BodyLoader>
#include <cnoid/EigenUtil>
#include <cnoid/MeshGenerator>
#include <cnoid/NullOut>
#include <cnoid/SceneLoader>
#include <cnoid/UTF8>
#include <cnoid/stdx/filesystem>
#include <fmt/format.h>
#include <pugixml.hpp>

using namespace cnoid;

namespace filesystem = cnoid::stdx::filesystem;
using fmt::format;
using pugi::xml_attribute;
using pugi::xml_node;
using std::endl;
using std::string;
using std::vector;

const char BOX[] = "box";
const char CHILD[] = "child";
const char COLLISION[] = "collision";
const char CYLINDER[] = "cylinder";
const char FILENAME[] = "filename";
const char GEOMETRY[] = "geometry";
const char INERTIA[] = "inertia";
const char INERTIAL[] = "inertial";
const char IXX[] = "ixx";
const char IXY[] = "ixy";
const char IXZ[] = "ixz";
const char IYY[] = "iyy";
const char IYZ[] = "iyz";
const char IZZ[] = "izz";
const char JOINT[] = "joint";
const char LENGTH[] = "length";
const char LINK[] = "link";
const char MASS[] = "mass";
const char MESH[] = "mesh";
const char NAME[] = "name";
const char ORIGIN[] = "origin";
const char PARENT[] = "parent";
const char RADIUS[] = "radius";
const char ROBOT[] = "robot";
const char RPY[] = "rpy";
const char SCALE[] = "scale";
const char SIZE[] = "size";
const char SPHERE[] = "sphere";
const char VALUE[] = "value";
const char VISUAL[] = "visual";
const char XYZ[] = "xyz";

namespace {
struct Registration
{
    Registration()
    {
        BodyLoader::registerLoader({"urdf", "xacro"},
                                   []() -> AbstractBodyLoaderPtr {
                                       return std::make_shared<URDFBodyLoader>();
                                   });
    }
} registration;


class ROSPackageSchemeHandler
{
    vector<string> packagePaths;

public:
    ROSPackageSchemeHandler()
    {
        const char* str = getenv("ROS_PACKAGE_PATH");
        if (str) {
            do {
                const char* begin = str;
                while (*str != ':' && *str)
                    str++;
                packagePaths.push_back(string(begin, str));
            } while (0 != *str++);
        }
    }

    string operator()(const string& path, std::ostream& os)
    {
        const string prefix = "package://";
        if (!(path.size() >= prefix.size()
              && std::equal(prefix.begin(), prefix.end(), path.begin()))) {
            return path;
        }

        filesystem::path filepath(fromUTF8(path.substr(prefix.size())));
        auto iter = filepath.begin();
        if (iter == filepath.end()) {
            return string();
        }

        filesystem::path directory = *iter++;
        filesystem::path relativePath;
        while (iter != filepath.end()) {
            relativePath /= *iter++;
        }

        bool found = false;
        filesystem::path combined;

        for (auto element : packagePaths) {
            filesystem::path packagePath(element);
            combined = packagePath / filepath;
            if (exists(combined)) {
                found = true;
                break;
            }
            combined = packagePath / relativePath;
            if (exists(combined)) {
                found = true;
                break;
            }
        }

        if (found) {
            return toUTF8(combined.string());
        } else {
            os << format("\"{}\" is not found in the ROS package directories.",
                         path)
               << endl;
            return string();
        }
    }
};

}  // namespace

namespace cnoid {
class URDFBodyLoader::Impl
{
public:
    std::ostream* os_;
    std::ostream& os() { return *os_; }

    Impl();
    bool load(Body* body, const string& filename);

private:
    SceneLoader sceneLoader_;
    ROSPackageSchemeHandler ROSPackageSchemeHandler_;

    vector<xml_node> findChildrenByGrandchildAttribute(
        const xml_node& node,
        const char* child_name,
        const char* grandchild_name,
        const char* attr_name,
        const char* attr_value);
    vector<xml_node> findRootLinks(const xml_node& robot);
    bool loadLink(LinkPtr link, const xml_node& linkNode);
    bool loadInertialTag(LinkPtr& link, const xml_node& inertialNode);
    bool loadVisualTag(LinkPtr& link, const xml_node& visualNode);
    bool loadCollisionTag(LinkPtr& link, const xml_node& collisionNode);
    bool readOriginTag(const xml_node& originNode,
                       const string& elementName,
                       Vector3& translation,
                       Matrix3& rotation);
    void printReadingInertiaTagError(const string& attribute_name);
    bool readInertiaTag(const xml_node& inertiaNode, Matrix3& inertiaMatrix);
    bool readGeometryTag(const xml_node& geometryNode, SgNodePtr& mesh);
};
}  // namespace cnoid

URDFBodyLoader::URDFBodyLoader()
{
    impl = new Impl;
}


URDFBodyLoader::Impl::Impl()
{
    os_ = &nullout();
}


URDFBodyLoader::~URDFBodyLoader()
{
    delete impl;
}


void URDFBodyLoader::setMessageSink(std::ostream& os)
{
    impl->os_ = &os;
}


bool URDFBodyLoader::load(Body* body, const std::string& filename)
{
    return impl->load(body, filename);
}


bool URDFBodyLoader::Impl::load(Body* body, const string& filename)
{
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(filename.c_str());
    if (!result) {
        os() << "Error: parsing XML failed: " << result.description() << endl;
    }

    // checks if only one 'robot' tag exists in the URDF
    if (++doc.children(ROBOT).begin() != doc.children(ROBOT).end()) {
        os() << "Error: Multiple 'robot' tags are found.";
        return false;
    }

    // parses the robot structure ('robot' tag)
    xml_node robot = doc.child(ROBOT);
    auto linkNodes = robot.children(LINK);
    auto jointNodes = robot.children(JOINT);

    // creates a link dictionary by loading all links for tree construction
    std::unordered_map<string, LinkPtr> linkMap;
    linkMap.reserve(boost::size(linkNodes));

    for (xml_node linkNode : linkNodes) {
        LinkPtr link = new Link;
        if (!loadLink(link, linkNode)) {
            // some err msg
            return false;
        }
        linkMap.emplace(link->name(), link);
    }

    // TEST
    for (auto element : linkMap) {
        os() << "[TEST]" << endl
             << "\tName: " << element.first << endl
             << "\tMass: " << element.second->mass() << endl
             << "\tInertia: " << element.second->I().row(0) << endl
             << "\t         " << element.second->I().row(1) << endl
             << "\t         " << element.second->I().row(2) << endl
             << "\tCoM: " << element.second->centerOfMass().transpose() << endl;
    }

    return true;
}

vector<xml_node> URDFBodyLoader::Impl::findChildrenByGrandchildAttribute(
    const xml_node& node,
    const char* child_name,
    const char* grandchild_name,
    const char* attr_name,
    const char* attr_value)
{
    vector<xml_node> result;
    for (xml_node child : node.children(child_name)) {
        xml_node candidate = child.find_child_by_attribute(grandchild_name,
                                                           attr_name,
                                                           attr_value);
        if (candidate != xml_node()) {
            result.push_back(candidate);
        }
    }
    return result;
}

vector<xml_node> URDFBodyLoader::Impl::findRootLinks(const xml_node& robot)
{
    vector<xml_node> rootLinks;
    for (xml_node link : robot.children(LINK)) {
        if (findChildrenByGrandchildAttribute(robot,
                                              JOINT,
                                              CHILD,
                                              LINK,
                                              link.attribute(NAME).value())
                .size()
            == 0) {
            rootLinks.push_back(link);
        }
    }
    return rootLinks;
}


bool URDFBodyLoader::Impl::loadLink(LinkPtr link, const xml_node& linkNode)
{
    // sets name (requrired)
    std::string name(linkNode.attribute(NAME).as_string());
    if (name.empty()) {
        os() << "\033[31m Error: There exist a unnamed link.\033[m" << endl;
        return false;
    }
    link->setName(name);

    // 'inertial' (optional)
    const xml_node& inertialNode = linkNode.child(INERTIAL);
    if (inertialNode == xml_node()) {
        os() << "Debug: link '" << name << "' has no inertial data." << endl;
    } else {
        if (!loadInertialTag(link, inertialNode)) {
            os() << "Note: The above error occurs while loading link '" << name
                 << "'." << endl;
            return false;
        }
    }

    // 'visual' (optional)
    const xml_node& visualNode = linkNode.child(VISUAL);
    if (visualNode.empty()) {
        os() << "Debug: link '" << name << "' has no visual data." << endl;
    } else {
        if (!loadVisualTag(link, visualNode)) {
            os() << "Note: The above error occurs while loading link '" << name
                 << "'." << endl;
            return false;
        }
    }

    // 'collision' (optional)
    const xml_node& collisionNode = linkNode.child(COLLISION);
    if (collisionNode.empty()) {
        os() << "Debug: link '" << name << "' has no collision data." << endl;
    } else {
        if (!loadCollisionTag(link, collisionNode)) {
            os() << "Note: The above error occurs while loading link '" << name
                 << "'." << endl;
            return false;
        }
    }

    return true;
}


bool URDFBodyLoader::Impl::loadInertialTag(LinkPtr& link,
                                           const xml_node& inertialNode)
{
    // 'origin' tag
    const xml_node& originNode = inertialNode.child(ORIGIN);
    Vector3 translation;
    Matrix3 rotation;
    if (!readOriginTag(originNode, INERTIAL, translation, rotation)) {
        return false;
    }
    link->setCenterOfMass(translation);

    // 'mass' tag
    const double mass = inertialNode.child(MASS).attribute(VALUE).as_double();
    if (mass > 0.0) {
        link->setMass(mass);
    } else {
        os() << "Error: mass value is invalid or not defined." << endl;
        return false;
    }

    // 'inertia' tag
    Matrix3 inertiaMatrix = Matrix3::Identity();
    const xml_node& inertiaNode = inertialNode.child(INERTIA);
    if (inertiaNode != xml_node()) {
        if (!readInertiaTag(inertiaNode, inertiaMatrix)) {
            return false;
        }
    }
    link->setInertia(rotation * inertiaMatrix * rotation.transpose());

    return true;
}


bool URDFBodyLoader::Impl::loadVisualTag(LinkPtr& link,
                                         const xml_node& visualNode)
{
    // 'origin' tag
    const xml_node& originNode = visualNode.child(ORIGIN);
    Vector3 translation = Vector3::Zero();
    Matrix3 rotation = Matrix3::Identity();
    if (!readOriginTag(originNode, INERTIAL, translation, rotation)) {
        return false;
    }
    Isometry3 originalPose;
    originalPose.linear() = rotation;
    originalPose.translation() = translation;

    // 'geometry' tag
    const xml_node& geometryNode = visualNode.child(GEOMETRY);
    if (geometryNode.empty()) {
        os() << "Error: Visual geometry is not found." << endl;
        return false;
    }

    SgNodePtr mesh = new SgNode;
    if (!readGeometryTag(geometryNode, mesh)) {
        os() << "Error: Failed to load visual geometry" << endl;
    }
    SgPosTransformPtr transformation = new SgPosTransform(originalPose);
    transformation->addChild(mesh);
    link->addVisualShapeNode(transformation);

    // TODO: 'material' tag

    return true;
}


bool URDFBodyLoader::Impl::loadCollisionTag(LinkPtr& link,
                                            const xml_node& collisionNode)
{
    // 'origin' tag
    const xml_node& originNode = collisionNode.child(ORIGIN);
    Vector3 translation = Vector3::Zero();
    Matrix3 rotation = Matrix3::Identity();
    if (!readOriginTag(originNode, INERTIAL, translation, rotation)) {
        return false;
    }
    Isometry3 originalPose;
    originalPose.linear() = rotation;
    originalPose.translation() = translation;

    // 'geometry' tag
    const xml_node& geometryNode = collisionNode.child(GEOMETRY);
    if (geometryNode.empty()) {
        os() << "Error: Collision geometry is not found." << endl;
        return false;
    }

    SgNodePtr mesh = new SgNode;
    if (!readGeometryTag(geometryNode, mesh)) {
        os() << "Error: Failed to load collision geometry" << endl;
    }
    SgPosTransformPtr transformation = new SgPosTransform(originalPose);
    transformation->addChild(mesh);
    link->addCollisionShapeNode(transformation);

    return true;
}


bool URDFBodyLoader::Impl::readOriginTag(const xml_node& originNode,
                                         const string& parentName,
                                         Vector3& translation,
                                         Matrix3& rotation)
{
    const string origin_xyz_str = originNode.attribute(XYZ).as_string();
    if (origin_xyz_str.empty()) {
        translation = Vector3::Zero();
    } else {
        Vector3 origin_xyz;
        if (!toVector3(origin_xyz_str, translation)) {
            os() << "Error: origin xyz of " << parentName
                 << " is written in invalid format." << endl;
            return false;
        }
    }

    const string origin_rpy_str = originNode.attribute(RPY).as_string();
    if (origin_rpy_str.empty()) {
        rotation = Matrix3::Identity();
    } else {
        Vector3 origin_rpy;
        if (!toVector3(origin_rpy_str, origin_rpy)) {
            os() << "Error: origin rpy of " << parentName
                 << " is written in invalid format.";
            return false;
        }
        rotation = rotFromRpy(origin_rpy);
    }
    return true;
}


void URDFBodyLoader::Impl::printReadingInertiaTagError(
    const string& attribute_name)
{
    os() << "Error: " << attribute_name << " value is not defined." << endl;
    return;
}


bool URDFBodyLoader::Impl::readInertiaTag(const xml_node& inertiaNode,
                                          Matrix3& inertiaMatrix)
{
    if (inertiaNode.attribute(IXX).empty()) {
        printReadingInertiaTagError(IXX);
        return false;
    } else {
        inertiaMatrix(0, 0) = inertiaNode.attribute(IXX).as_double();
    }
    if (inertiaNode.attribute(IXY).empty()) {
        printReadingInertiaTagError(IXY);
        return false;
    } else {
        inertiaMatrix(0, 1) = inertiaNode.attribute(IXY).as_double();
        inertiaMatrix(1, 0) = inertiaMatrix(0, 1);
    }
    if (inertiaNode.attribute(IXZ).empty()) {
        printReadingInertiaTagError(IXZ);
        return false;
    } else {
        inertiaMatrix(0, 2) = inertiaNode.attribute(IXZ).as_double();
        inertiaMatrix(2, 0) = inertiaMatrix(0, 2);
    }
    if (inertiaNode.attribute(IYY).empty()) {
        printReadingInertiaTagError(IYY);
        return false;
    } else {
        inertiaMatrix(1, 1) = inertiaNode.attribute(IYY).as_double();
    }
    if (inertiaNode.attribute(IYZ).empty()) {
        printReadingInertiaTagError(IYZ);
        return false;
    } else {
        inertiaMatrix(1, 2) = inertiaNode.attribute(IYZ).as_double();
        inertiaMatrix(2, 1) = inertiaMatrix(1, 2);
    }
    if (inertiaNode.attribute(IZZ).empty()) {
        printReadingInertiaTagError(IZZ);
        return false;
    } else {
        inertiaMatrix(2, 2) = inertiaNode.attribute(IZZ).as_double();
    }

    return true;
}


bool URDFBodyLoader::Impl::readGeometryTag(const xml_node& geometryNode,
                                           SgNodePtr& mesh)
{
    if (boost::size(geometryNode.children()) < 1) {
        os() << "Error: no geometry is found." << endl;
    } else if (boost::size(geometryNode.children()) > 1) {
        os() << "Error: one link can have only one geometry." << endl;
    }

    MeshGenerator meshGenerator;
    SgShapePtr shape = new SgShape;
    // const xml_node& elementNode = geometryNode.first_child();

    if (!geometryNode.child(BOX).empty()) {
        Vector3 size = Vector3::Zero();
        if (!toVector3(geometryNode.child(BOX).attribute(SIZE).as_string(),
                       size)) {
            os() << "Error: box size is written in invalid format." << endl;
        }

        shape->setMesh(meshGenerator.generateBox(size));
        mesh = shape;
    } else if (!geometryNode.child(CYLINDER).empty()) {
        if (geometryNode.child(CYLINDER).attribute(RADIUS).empty()) {
            os() << "Error: cylinder radius is not defined." << endl;
            return false;
        }
        if (geometryNode.child(CYLINDER).attribute(LENGTH).empty()) {
            os() << "Error: cylinder length is not defined." << endl;
            return false;
        }
        const double radius
            = geometryNode.child(CYLINDER).attribute(RADIUS).as_double();
        const double length
            = geometryNode.child(CYLINDER).attribute(LENGTH).as_double();

        shape->setMesh(meshGenerator.generateCylinder(radius, length));
        mesh = shape;
    } else if (!geometryNode.child(SPHERE).empty()) {
        if (geometryNode.child(SPHERE).attribute(RADIUS).empty()) {
            os() << "Error: sphere radius is not defined." << endl;
            return false;
        }
        const double radius
            = geometryNode.child(SPHERE).attribute(RADIUS).as_double();

        shape->setMesh(meshGenerator.generateSphere(radius));
        mesh = shape;
    } else if (!geometryNode.child(MESH).empty()) {
        if (geometryNode.child(MESH).attribute(FILENAME).empty()) {
            os() << "Error: mesh file is not specified." << endl;
            return false;
        }

        // loads a mesh file
        const string filename
            = geometryNode.child(MESH).attribute(FILENAME).as_string();
        bool isSupportedFormat = false;
        mesh = sceneLoader_.load(ROSPackageSchemeHandler_(filename, os()),
                                 isSupportedFormat);
        if (!isSupportedFormat) {
            os() << "Error: format of the specified mesh file '" << filename
                 << "' is not supported." << endl;
            return false;
        }

        // scales the mesh
        if (!geometryNode.child(MESH).attribute(SCALE).empty()) {
            Vector3 scale = Vector3::Ones();
            if (!toVector3(geometryNode.child(MESH).attribute(SCALE).as_string(),
                           scale)) {
                os() << "Error: mesh scale is written in invalid format."
                     << endl;

                return false;
            }

            SgScaleTransformPtr scaler = new SgScaleTransform;
            scaler->setScale(scale);
            scaler->addChild(mesh);
            mesh = scaler;
        }
    } else {
        os() << "Error: unsupported geometry "
             << geometryNode.first_child().name() << " is described." << endl;
        return false;
    }

    return true;
}