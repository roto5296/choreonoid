/**
   @author Shin'ichiro Nakaoka
*/

#ifndef CNOID_BASE_POSITION_DRAGGER_H
#define CNOID_BASE_POSITION_DRAGGER_H

#include "SceneWidgetEditable.h"
#include "exportdecl.h"

namespace cnoid {

class CNOID_EXPORT PositionDragger : public SgPosTransform, public SceneWidgetEditable
{
public:
    enum AxisBit {
        TX = 1 << 0, TY = 1 << 1, TZ = 1 << 2,
        TranslationAxes = (TX | TY | TZ),
        RX = 1 << 3, RY = 1 << 4, RZ = 1 << 5,
        RotationAxes = (RX | RY | RZ),
        AllAxes = (TX | TY | TZ | RX | RY | RZ),

        // deprecated
        TRANSLATION_AXES = TranslationAxes,
        ROTATION_AXES = RotationAxes,
        ALL_AXES = AllAxes
    };

    enum HandleType {
        StandardHandle = 0,
        PositiveOnlyHandle = 1,
        WideHandle = 2,
    };
        
    PositionDragger(int axes = AllAxes, int handleType = StandardHandle);
    PositionDragger(const PositionDragger& org) = delete;

    //! \param T Local position from the virtual origin to the dragger central position
    void setOffset(const Isometry3& T);

    void setDraggableAxes(int axisBitSet, SgUpdateRef update);
    int draggableAxes() const;
    SignalProxy<void(int axisBitSet)> sigDraggableAxesChanged();

    double handleSize() const;
    void setHandleSize(double s);
    void setHandleWidthRatio(double w); // width ratio

    double rotationHandleSizeRatio() const;
    void setRotationHandleSizeRatio(double r);

    [[deprecated("Use setHandleSize and setRotationHandlerSizeRatio")]]
    void setRadius(double r, double translationAxisRatio = 2.0f);
    [[deprecated("Use handleSize")]]
    double radius() const;
    
    bool adjustSize();
    bool adjustSize(const BoundingBox& bb);

    void setPixelSize(int length, int width);

    /**
       \todo Implement the following function, which ajudsts the size considering
       the PPI of the display, and use it for the fixed size marker.
       \note The unit of the parameters is meter.
    */
    void setScreenFixedSize(double length, double width);
    
    bool isScreenFixedSizeMode() const;

    [[deprecated("Use setFixedPixelSize")]]
    void setFixedPixelSizeMode(bool on, double pixelSizeRatio = 1.0);
    [[deprecated("Use isFixedScreenSizeMode")]]
    bool isFixedPixelSizeMode() const;

    void setTransparency(float t);
    float transparency() const;

    void setOverlayMode(bool on);
    bool isOverlayMode() const;
    
    bool isContainerMode() const;
    void setContainerMode(bool on);

    void setContentsDragEnabled(bool on);
    bool isContentsDragEnabled() const;

    enum DisplayMode { DisplayAlways, DisplayInEditMode, DisplayInFocus, DisplayNever };
    DisplayMode displayMode() const;
    void setDisplayMode(DisplayMode mode, SgUpdateRef update = nullptr);

    [[deprecated("This function does nothing.")]]
    void setUndoEnabled(bool on);
    [[deprecated("This function always returns false.")]]
    bool isUndoEnabled() const;
    [[deprecated("This function does nothing.")]]
    void storeCurrentPositionToHistory();

    bool isDragEnabled() const;
    void setDragEnabled(bool on);
    bool isDragging() const;

    [[deprecated("Use globalDraggingPosition to get the global coordinate, or "
                 "draggingPosition to get the local position in the parent node coordinate.")]]
    Isometry3 draggedPosition() const { return globalDraggingPosition(); }

    Isometry3 draggingPosition() const;
    Isometry3 globalDraggingPosition() const;
    
    SignalProxy<void()> sigDragStarted();
    SignalProxy<void()> sigPositionDragged();
    SignalProxy<void()> sigDragFinished();

    virtual void onSceneModeChanged(const SceneWidgetEvent& event) override;
    virtual bool onButtonPressEvent(const SceneWidgetEvent& event) override;
    virtual bool onButtonReleaseEvent(const SceneWidgetEvent& event) override;
    virtual bool onPointerMoveEvent(const SceneWidgetEvent& event) override;
    virtual void onPointerLeaveEvent(const SceneWidgetEvent& event) override;
    virtual void onFocusChanged(const SceneWidgetEvent& event, bool on) override;

    // Thw following functions are deprecated. Use displayMode and setDisplayMode instead.
    [[deprecated("Use setDisplayMode.")]]
    void setDraggerAlwaysShown(bool on, SgUpdateRef update);
    [[deprecated("Use displayMode.")]]
    bool isDraggerAlwaysShown() const;
    [[deprecated("Use setDisplayMode.")]]
    void setDraggerAlwaysHidden(bool on, SgUpdateRef update);
    [[deprecated("Use displayMode.")]]
    bool isDraggerAlwaysHidden() const;

    class Impl;

private:
    Impl* impl;
};
    
typedef ref_ptr<PositionDragger> PositionDraggerPtr;

}

#endif
