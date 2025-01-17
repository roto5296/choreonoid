/**
   @author Shin'ichiro Nakaoka
*/

#include "TimeBar.h"
#include "ExtensionManager.h"
#include "Archive.h"
#include "OptionManager.h"
#include "SpinBox.h"
#include "Slider.h"
#include "Buttons.h"
#include "CheckBox.h"
#include "Dialog.h"
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <cmath>
#include <limits>
#include <iostream>
#include "gettext.h"

using namespace std;
using namespace cnoid;

namespace {

const bool TRACE_FUNCTIONS = false;

const double DEFAULT_FRAME_RATE = 1000.0;

// The following value shoud be same as the display refresh rate to make the animation smooth
const double DEFAULT_PLAYBACK_FRAMERATE = 60.0;

enum ElementId {
    PlayButton = 0,
    ResumeButton = 1,
    RefreshButton = 2,
    TimeSpin = 3,
    TimeSlider = 4,
    TimeRangeMinSpin = 5,
    TimeRangeMaxSpin = 6,
    ConfigButton = 7
};

class ConfigDialog : public Dialog
{
public:
    SpinBox frameRateSpin;
    SpinBox playbackFrameRateSpin;
    CheckBox idleLoopDrivenCheck;
    DoubleSpinBox playbackSpeedRatioSpin;
    CheckBox ongoingTimeSyncCheck;
    CheckBox autoExpandCheck;
    QCheckBox beatModeCheck;
    DoubleSpinBox tempoSpin;
    SpinBox beatcSpin;
    SpinBox beatmSpin;
    DoubleSpinBox beatOffsetSpin;

    ConfigDialog() {
        setWindowTitle(_("Time Bar Config"));

        QVBoxLayout* vbox = new QVBoxLayout;
        setLayout(vbox);
        
        QHBoxLayout* hbox = new QHBoxLayout;
        hbox->addWidget(new QLabel(_("Internal frame rate")));
        frameRateSpin.setAlignment(Qt::AlignCenter);
        frameRateSpin.setRange(1, 10000);
        hbox->addWidget(&frameRateSpin);
        hbox->addStretch();
        vbox->addLayout(hbox);

        hbox = new QHBoxLayout;
        hbox->addWidget(new QLabel(_("Playback frame rate")));
        playbackFrameRateSpin.setAlignment(Qt::AlignCenter);
        playbackFrameRateSpin.setRange(0, 1000);
        playbackFrameRateSpin.setValue(DEFAULT_PLAYBACK_FRAMERATE);
        hbox->addWidget(&playbackFrameRateSpin);
        hbox->addStretch();
        vbox->addLayout(hbox);

        hbox = new QHBoxLayout;
        idleLoopDrivenCheck.setText(_("Idle loop driven mode"));
        hbox->addWidget(&idleLoopDrivenCheck);
        hbox->addStretch();
        vbox->addLayout(hbox);
            
        hbox = new QHBoxLayout;
        hbox->addWidget(new QLabel(_("Playback speed ratio")));
        playbackSpeedRatioSpin.setAlignment(Qt::AlignCenter);
        playbackSpeedRatioSpin.setDecimals(1);
        playbackSpeedRatioSpin.setRange(0.1, 99.9);
        playbackSpeedRatioSpin.setSingleStep(0.1);
        playbackSpeedRatioSpin.setValue(1.0);
        hbox->addWidget(&playbackSpeedRatioSpin);
        hbox->addStretch();
        vbox->addLayout(hbox);

        hbox = new QHBoxLayout;
        ongoingTimeSyncCheck.setText(_("Sync with ongoing updates"));
        ongoingTimeSyncCheck.setChecked(true);
        hbox->addWidget(&ongoingTimeSyncCheck);
        hbox->addStretch();
        vbox->addLayout(hbox);

        hbox = new QHBoxLayout;
        autoExpandCheck.setText(_("Automatically expand the time range"));
        autoExpandCheck.setChecked(true);
        hbox->addWidget(&autoExpandCheck);
        hbox->addStretch();
        vbox->addLayout(hbox);
            
        /*
          hbox = new QHBoxLayout;
          vbox->addLayout(hbox);
            
          beatModeCheck = new QCheckBox(_("Beat mode"));
          hbox->addWidget(beatModeCheck);

          beatcSpin = new SpinBox;
          beatcSpin->setRange(1, 99);
          hbox->addWidget(beatcSpin);

          hbox->addWidget(new QLabel("/"));

          beatmSpin = new SpinBox;
          beatmSpin->setRange(1, 99);
          hbox->addWidget(beatmSpin);

          hbox->addStretch();
          hbox = new QHBoxLayout();
          vbox->addLayout(hbox);

          hbox->addWidget(new QLabel(_("Tempo")));
          tempoSpin = new DoubleSpinBox;
          tempoSpin->setRange(1.0, 999.99);
          tempoSpin->setDecimals(2);
          hbox->addWidget(tempoSpin);

          hbox->addWidget(new QLabel(_("Offset")));
          beatOffsetSpin = new DoubleSpinBox;
          beatOffsetSpin->setRange(-9.99, 9.99);
          beatOffsetSpin->setDecimals(2);
          beatOffsetSpin->setSingleStep(0.1);
          hbox->addWidget(beatOffsetSpin);
          hbox->addWidget(new QLabel(_("[s]")));

          hbox->addStretch();
        */

        vbox->addStretch();

        PushButton* okButton = new PushButton(_("&OK"));
        okButton->setDefault(true);
        QDialogButtonBox* buttonBox = new QDialogButtonBox(this);
        buttonBox->addButton(okButton, QDialogButtonBox::AcceptRole);
        connect(buttonBox,SIGNAL(accepted()), this, SLOT(accept()));
        vbox->addWidget(buttonBox);
    }
};

}

namespace cnoid {

class TimeBar::Impl : public QObject
{
public:
    Impl(TimeBar* self);
    ~Impl();

    double quantizedTime(double time) const;
    bool setTime(double time, bool calledFromPlaybackLoop, QWidget* callerWidget = nullptr);
    void onTimeSpinChanged(double value);
    bool onTimeSliderValueChanged(int value);

    void setTimeRange(double minTime, double maxTime);
    void setFrameRate(double rate);
    void updateTimeProperties(bool forceUpdate);
    void onPlaybackSpeedRatioChanged(double value);
    void onPlaybackFrameRateChanged(int value);
    void onPlayActivated();
    void onResumeActivated();
    void startPlayback();
    void startPlayback(double time);
    void stopPlayback(bool isStoppedManually);
    int startOngoingTimeUpdate(double time);
    void updateOngoingTime(int id, double time);
    void updateMinOngoingTime();
    void stopOngoingTimeUpdate(int id);

    void onTimeRangeSpinsChanged();
    void onFrameRateSpinChanged(int value);

    virtual void timerEvent(QTimerEvent* event);
        
    bool storeState(Archive& archive);
    bool restoreState(const Archive& archive);

    TimeBar* self;
    ConfigDialog config;

    ToolButton* resumeButton;
    ToolButton* frameModeToggle;
    QIcon resumeIcon;
    QIcon stopIcon;
        
    DoubleSpinBox* timeSpin;
    Slider* timeSlider;
    DoubleSpinBox* minTimeSpin;
    DoubleSpinBox* maxTimeSpin;
    int decimals;
    double minTime;
    double maxTime;
    double playbackSpeedRatio;
    double playbackFrameRate;
    double animationTimeOffset;
    int timerId;
    QElapsedTimer elapsedTimer;
    bool repeatMode;
    bool isDoingPlayback;
    map<int, double> ongoingTimeMap;
    double ongoingTime;
    bool hasOngoingTime;

    Signal<bool(double time), LogicalProduct> sigPlaybackInitialized;
    Signal<void(double time)> sigPlaybackStarted;
    Signal<bool(double time), LogicalSum> sigTimeChanged;
    Signal<void(double time, bool isStoppedManually)> sigPlaybackStopped;
};
}


static void onSigOptionsParsed(boost::program_options::variables_map& v)
{
    if(v.count("start-playback")){
        TimeBar::instance()->startPlayback();
    }
}


void TimeBar::initialize(ExtensionManager* ext)
{
    static bool initialized = false;
    if(!initialized){
        ext->addToolBar(TimeBar::instance());

        ext->optionManager()
            .addOption("start-playback", "start playback automatically")
            .sigOptionsParsed(1).connect(onSigOptionsParsed);
            
        initialized = true;
    }
}


TimeBar* TimeBar::instance()
{
    static TimeBar* timeBar = new TimeBar;
    return timeBar;
}


TimeBar::TimeBar()
    : ToolBar(N_("TimeBar"))
{
    impl = new Impl(this);
}


TimeBar::Impl::Impl(TimeBar* self)
    : self(self),
      resumeIcon(QIcon(":/Base/icon/resume.svg")),
      stopIcon(QIcon(":/Base/icon/stop.svg"))
{
    self->setVisibleByDefault(true);
    self->setStretchable(true);
    
    self->time_ = 0.0;
    self->frameRate_ = DEFAULT_FRAME_RATE;
    decimals = 2;
    minTime = 0.0;
    maxTime = 30.0;
    repeatMode = false;
    timerId = 0;
    isDoingPlayback = false;
    ongoingTime = 0.0;
    hasOngoingTime = false;

    auto playButton = self->addButton(QIcon(":/Base/icon/play.svg"), PlayButton);
    playButton->setToolTip(_("Start playback"));
    playButton->sigClicked().connect([&](){ onPlayActivated(); });

    resumeButton = self->addButton(resumeIcon, ResumeButton);
    resumeButton->setToolTip(_("Resume playback"));
    resumeButton->sigClicked().connect([&](){ onResumeActivated(); });

    auto refreshButton = self->addButton(QIcon(":/Base/icon/refresh.svg"), RefreshButton);
    refreshButton->setToolTip(_("Refresh state at the current time"));
    refreshButton->sigClicked().connect([this](){ this->self->refresh(); });
    
    timeSpin = new DoubleSpinBox;
    timeSpin->setAlignment(Qt::AlignCenter);
    timeSpin->sigValueChanged().connect([&](double value){ onTimeSpinChanged(value); });
    self->addWidget(timeSpin, TimeSpin);

    timeSlider = new Slider(Qt::Horizontal);
    timeSlider->sigValueChanged().connect([&](int value){ onTimeSliderValueChanged(value); });
    timeSlider->setMinimumWidth(timeSlider->sizeHint().width());
    self->addWidget(timeSlider, TimeSlider);

    minTimeSpin = new DoubleSpinBox;
    minTimeSpin->setAlignment(Qt::AlignCenter);
    minTimeSpin->setRange(-9999.0, 9999.0);
    minTimeSpin->sigValueChanged().connect([&](double){ onTimeRangeSpinsChanged(); });
    self->addWidget(minTimeSpin, TimeRangeMinSpin);

    self->addLabel(" : ");

    maxTimeSpin = new DoubleSpinBox;
    maxTimeSpin->setAlignment(Qt::AlignCenter);
    maxTimeSpin->setRange(-9999.0, 9999.0);
    maxTimeSpin->sigValueChanged().connect([&](double){ onTimeRangeSpinsChanged(); });
    self->addWidget(maxTimeSpin, TimeRangeMaxSpin);

    auto configButton = self->addButton(QIcon(":/Base/icon/setup.svg"));
    configButton->setToolTip(_("Show the config dialog"));
    configButton->sigClicked().connect([&](){ config.show(); });

    config.frameRateSpin.sigValueChanged().connect([&](int value){ onFrameRateSpinChanged(value); });
    config.playbackFrameRateSpin.sigValueChanged().connect([&](int value){ onPlaybackFrameRateChanged(value); });
    config.playbackSpeedRatioSpin.sigValueChanged().connect([&](double value){ onPlaybackSpeedRatioChanged(value); });

    playbackSpeedRatio = config.playbackSpeedRatioSpin.value();
    playbackFrameRate = config.playbackFrameRateSpin.value();

    updateTimeProperties(true);
}


TimeBar::~TimeBar()
{
    delete impl;
}


TimeBar::Impl::~Impl()
{

}


SignalProxy<bool(double time), LogicalProduct> TimeBar::sigPlaybackInitialized()
{
    return impl->sigPlaybackInitialized;
}


SignalProxy<void(double time)> TimeBar::sigPlaybackStarted()
{
    return impl->sigPlaybackStarted;
}


/**
   Signal emitted when the TimeBar's time changes.
   
   In the function connected to this signal, please return true if the time is valid for it,
   and return false if the time is not valid. The example of the latter case is that
   the time is over the length of the data processed in the function.
*/
SignalProxy<bool(double time), LogicalSum> TimeBar::sigTimeChanged()
{
    return impl->sigTimeChanged;
}


SignalProxy<void(double time, bool isStoppedManually)> TimeBar::sigPlaybackStopped()
{
    return impl->sigPlaybackStopped;
}


void TimeBar::Impl::onTimeSpinChanged(double value)
{
    if(TRACE_FUNCTIONS){
        cout << "TimeBar::Impl::onTimeSpinChanged()" << endl;
    }
    if(isDoingPlayback){
        stopPlayback(true);
    }
    setTime(value, false, timeSpin);
}


bool TimeBar::Impl::onTimeSliderValueChanged(int value)
{
    if(TRACE_FUNCTIONS){
        cout << "TimeBar::Impl::onTimeSliderChanged(): value = " << value << endl;
    }
    if(isDoingPlayback){
        stopPlayback(true);
    }
    setTime(value / pow(10.0, decimals), false, timeSlider);
    return true;
}


void TimeBar::setFrameRate(double rate)
{
    impl->setFrameRate(rate);
}


void TimeBar::Impl::setFrameRate(double rate)
{
    if(rate > 0.0){
        if(self->frameRate_ != rate){
            self->frameRate_ = rate;
            updateTimeProperties(true);
        }
    }
}


double TimeBar::minTime() const
{
    return impl->minTime;
}


double TimeBar::maxTime() const
{
    return impl->maxTime;
}


void TimeBar::setTimeRange(double min, double max)
{
    impl->setTimeRange(min, max);
}


void TimeBar::Impl::setTimeRange(double minTime, double maxTime)
{
    this->minTime = minTime;
    this->maxTime = maxTime;
    updateTimeProperties(false);
}


void TimeBar::Impl::updateTimeProperties(bool forceUpdate)
{
    timeSpin->blockSignals(true);
    timeSlider->blockSignals(true);
    minTimeSpin->blockSignals(true);
    maxTimeSpin->blockSignals(true);
    config.frameRateSpin.blockSignals(true);
    
    const double timeStep = 1.0 / self->frameRate_;
    decimals = static_cast<int>(ceil(log10(self->frameRate_)));
    const double r = pow(10.0, decimals);

    if(forceUpdate ||
       (minTime != timeSpin->minimum() || maxTime != timeSpin->maximum())){
        timeSpin->setRange(minTime, maxTime);
        timeSlider->setRange((int)nearbyint(minTime * r), (int)nearbyint(maxTime * r));
    }

    timeSpin->setDecimals(decimals);
    timeSpin->setSingleStep(timeStep);
    timeSlider->setSingleStep(timeStep * r);
    minTimeSpin->setValue(minTime);
    maxTimeSpin->setValue(maxTime);
    config.frameRateSpin.setValue(self->frameRate_);

    config.frameRateSpin.blockSignals(false);
    maxTimeSpin->blockSignals(false);
    minTimeSpin->blockSignals(false);
    timeSlider->blockSignals(false);
    timeSpin->blockSignals(false);

    setTime(self->time_, false);
}

    
void TimeBar::Impl::onPlaybackSpeedRatioChanged(double value)
{
    playbackSpeedRatio = value;
    
    if(isDoingPlayback){
        startPlayback();
    }
}


double TimeBar::playbackSpeedRatio() const
{
    return impl->config.playbackSpeedRatioSpin.value();
}


void TimeBar::setPlaybackSpeedRatio(double ratio)
{
    impl->config.playbackSpeedRatioSpin.setValue(ratio);
}


void TimeBar::Impl::onPlaybackFrameRateChanged(int value)
{
    playbackFrameRate = value;

    if(isDoingPlayback){
        startPlayback();
    }
}


double TimeBar::playbackFrameRate() const
{
    return impl->config.playbackFrameRateSpin.value();
}


void TimeBar::setPlaybackFrameRate(double rate)
{
    impl->config.playbackFrameRateSpin.setValue(rate);
}


void TimeBar::setRepeatMode(bool on)
{
    impl->repeatMode = on;
}


void TimeBar::Impl::onPlayActivated()
{
    stopPlayback(true);
    startPlayback(minTime);
}


void TimeBar::Impl::onResumeActivated()
{
    if(isDoingPlayback){
        stopPlayback(true);
    } else {
        stopPlayback(true);
        startPlayback();
    }
}


void TimeBar::startPlayback()
{
    impl->startPlayback(time_);
}


void TimeBar::startPlayback(double time)
{
    impl->startPlayback(time);
}


void TimeBar::Impl::startPlayback()
{
    startPlayback(self->time_);
}


void TimeBar::Impl::startPlayback(double time)
{
    stopPlayback(false);

    bool isOngoingTimeValid = hasOngoingTime && config.ongoingTimeSyncCheck.isChecked();
    if(isOngoingTimeValid){
        time = ongoingTime;
    }

    self->time_ = quantizedTime(time);
    animationTimeOffset = self->time_;

    if(sigPlaybackInitialized(self->time_)){

        sigPlaybackStarted(self->time_);
        
        if(!setTime(self->time_, false) && !isOngoingTimeValid){
            sigPlaybackStopped(self->time_, false);

        } else {
            isDoingPlayback = true;

            const static QString tip(_("Stop animation"));
            resumeButton->setIcon(stopIcon);
            resumeButton->setToolTip(tip);
            int interval;
            if(config.idleLoopDrivenCheck.isChecked()){
                interval = 0;
            } else {
                interval = nearbyint(1000.0 / playbackFrameRate);
            }
            timerId = startTimer(interval, Qt::PreciseTimer);
            elapsedTimer.start();
        }
    }
}


void TimeBar::stopPlayback(bool isStoppedManually)
{
    impl->stopPlayback(isStoppedManually);
}


void TimeBar::Impl::stopPlayback(bool isStoppedManually)
{
    if(isDoingPlayback){
        killTimer(timerId);
        isDoingPlayback = false;
        sigPlaybackStopped(self->time_, isStoppedManually);

        const static QString tip(_("Resume animation"));
        resumeButton->setIcon(resumeIcon);
        resumeButton->setToolTip(tip);

        if(ongoingTimeMap.empty()){
            hasOngoingTime = false;
        }
    }
}


bool TimeBar::isDoingPlayback()
{
    return impl->isDoingPlayback;
}


void TimeBar::Impl::timerEvent(QTimerEvent*)
{
    double time = animationTimeOffset + playbackSpeedRatio * (elapsedTimer.elapsed() / 1000.0);

    bool doStopAtLastOngoingTime = false;
    if(hasOngoingTime){
        if(config.ongoingTimeSyncCheck.isChecked() || (time > ongoingTime)){
            animationTimeOffset += (ongoingTime - time);
            time = ongoingTime;
            if(ongoingTimeMap.empty()){
                doStopAtLastOngoingTime = true;
            }
        }
    }

    if(!setTime(time, true) || doStopAtLastOngoingTime){
        stopPlayback(false);
        
        if(!doStopAtLastOngoingTime && repeatMode){
            startPlayback(minTime);
        }
    }
}


double TimeBar::Impl::quantizedTime(double time) const
{
    return floor(time * self->frameRate_) / self->frameRate_;
}


bool TimeBar::setTime(double time)
{
    return impl->setTime(time, false);
}


/**
   @todo check whether block() and unblock() of sigc::connection
   decrease the performance or not.
*/
bool TimeBar::Impl::setTime(double time, bool calledFromPlaybackLoop, QWidget* callerWidget)
{
    if(TRACE_FUNCTIONS){
        cout << "TimeBar::Impl::setTime(" << time << ", " << calledFromPlaybackLoop << ")" << endl;
    }
    
    if(!calledFromPlaybackLoop && isDoingPlayback){
        return false;
    }

    const double newTime = quantizedTime(time);

    // Avoid redundant update
    if(calledFromPlaybackLoop || callerWidget){
        // When the optimization is enabled,
        // the result of (newTime == self->time_) sometimes becomes false,
        // so here the following judgement is used.
        if(fabs(newTime - self->time_) < 1.0e-14){
            return calledFromPlaybackLoop;
        }
    }

    if(newTime > maxTime && config.autoExpandCheck.isChecked()){
        maxTime = newTime;
        timeSpin->blockSignals(true);
        timeSlider->blockSignals(true);
        maxTimeSpin->blockSignals(true);
        timeSpin->setRange(minTime, maxTime);
        const double r = pow(10.0, decimals);
        timeSlider->setRange((int)nearbyint(minTime * r), (int)nearbyint(maxTime * r));
        maxTimeSpin->setValue(maxTime);
        maxTimeSpin->blockSignals(false);
        timeSlider->blockSignals(false);
        timeSpin->blockSignals(false);
    }
        
    self->time_ = newTime;

    if(callerWidget != timeSpin){
        timeSpin->blockSignals(true);
        timeSpin->setValue(self->time_);
        timeSpin->blockSignals(false);
    }
    if(callerWidget != timeSlider){
        timeSlider->blockSignals(true);
        timeSlider->setValue((int)nearbyint(self->time_ * pow(10.0, decimals)));
        timeSlider->blockSignals(false);
    }

    return sigTimeChanged(self->time_);
}


void TimeBar::refresh()
{
    if(!impl->isDoingPlayback){
        impl->setTime(time_, false);
    }
}    


int TimeBar::startOngoingTimeUpdate(double time)
{
    return impl->startOngoingTimeUpdate(time);
}


int TimeBar::Impl::startOngoingTimeUpdate(double time)
{
    int id = 0;
    if(ongoingTimeMap.empty()){
        hasOngoingTime = true;
    } else {
        while(ongoingTimeMap.find(id) == ongoingTimeMap.end()){
            ++id;
        }
    }
    updateOngoingTime(id, time);

    return id;
}


void TimeBar::updateOngoingTime(int id, double time)
{
    impl->updateOngoingTime(id, time);
}


void TimeBar::Impl::updateOngoingTime(int id, double time)
{
    ongoingTimeMap[id] = time;
    updateMinOngoingTime();
}


void TimeBar::Impl::updateMinOngoingTime()
{
    double minOngoingTime = std::numeric_limits<double>::max();
    for(auto& kv : ongoingTimeMap){
        minOngoingTime = std::min(kv.second, minOngoingTime);
    }
    ongoingTime = minOngoingTime;
}    


void TimeBar::stopOngoingTimeUpdate(int id)
{
    impl->stopOngoingTimeUpdate(id);
}


void TimeBar::Impl::stopOngoingTimeUpdate(int id)
{
    ongoingTimeMap.erase(id);

    if(!ongoingTimeMap.empty()){
        updateMinOngoingTime();
    } else {
        if(!isDoingPlayback){
            hasOngoingTime = false;
        }
    }
}


void TimeBar::setOngoingTimeSyncEnabled(bool on)
{
    impl->config.ongoingTimeSyncCheck.setChecked(on);
}


double TimeBar::realPlaybackTime() const
{
    if(impl->isDoingPlayback){
        return impl->animationTimeOffset + impl->playbackSpeedRatio * (impl->elapsedTimer.elapsed() / 1000.0);
    } else {
        return time_;
    }
}


void TimeBar::Impl::onTimeRangeSpinsChanged()
{
    setTimeRange(minTimeSpin->value(), maxTimeSpin->value());
}


void TimeBar::Impl::onFrameRateSpinChanged(int value)
{
    setFrameRate(config.frameRateSpin.value());
}


int TimeBar::stretchableDefaultWidth() const
{
    return sizeHint().width() + impl->timeSlider->sizeHint().width() * 5;
}
    

bool TimeBar::storeState(Archive& archive)
{
    ToolBar::storeState(archive);
    return impl->storeState(archive);
}


bool TimeBar::Impl::storeState(Archive& archive)
{
    archive.write("min_time", minTime);
    archive.write("max_time", maxTime);
    archive.write("frame_rate", self->frameRate_);
    archive.write("playback_frame_rate", playbackFrameRate);
    archive.write("idle_loop_driven_mode", config.idleLoopDrivenCheck.isChecked());
    archive.write("current_time", self->time_);
    archive.write("playback_speed_ratio", playbackSpeedRatio);
    archive.write("sync_to_ongoing_updates", config.ongoingTimeSyncCheck.isChecked());
    archive.write("auto_expansion", config.autoExpandCheck.isChecked());
    return true;
}


bool TimeBar::restoreState(const Archive& archive)
{
    if(ToolBar::restoreState(archive)){
        return impl->restoreState(archive);
    }
    return false;
}


bool TimeBar::Impl::restoreState(const Archive& archive)
{
    archive.read({ "min_time", "minTime" }, minTime);
    archive.read({ "max_time", "maxTime" }, maxTime);
    archive.read({ "current_time", "currentTime" }, self->time_);

    config.playbackFrameRateSpin.setValue(
        archive.get({ "playback_frame_rate", "playbackFrameRate" }, playbackFrameRate));
    config.idleLoopDrivenCheck.setChecked(
        archive.get("idle_loop_driven_mode", config.idleLoopDrivenCheck.isChecked()));
    config.playbackSpeedRatioSpin.setValue(
        archive.get("playback_speed_ratio", playbackSpeedRatio));
    config.ongoingTimeSyncCheck.setChecked(
        archive.get("sync_to_ongoing_updates", config.ongoingTimeSyncCheck.isChecked()));
    config.autoExpandCheck.setChecked(
        archive.get("auto_expansion", config.autoExpandCheck.isChecked()));

    double prevFrameRate = self->frameRate_;
    archive.read("frame_rate", self->frameRate_);

    updateTimeProperties(self->frameRate_ != prevFrameRate);
    
    return true;
}
