/*
 ** Surge Synthesizer is Free and Open Source Software
 **
 ** Surge is made available under the Gnu General Public License, v3.0
 ** https://www.gnu.org/licenses/gpl-3.0.en.html
 **
 ** Copyright 2004-2021 by various individuals as described by the Git transaction log
 **
 ** All source at: https://github.com/surge-synthesizer/surge.git
 **
 ** Surge was a commercial product from 2004-2018, with Copyright and ownership
 ** in that period held by Claes Johanson at Vember Audio. Claes made Surge
 ** open source in September 2018.
 */

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "OverlayComponent.h"
#include "SkinSupport.h"
#include "SurgeGUICallbackInterfaces.h"
#include "SurgeGUIEditor.h"
#include "SurgeStorage.h"
#include "widgets/ModulatableSlider.h"
#include "widgets/MultiSwitch.h"

#include "juce_core/juce_core.h"
#include "juce_dsp/juce_dsp.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "sst/cpputils.h"

namespace Surge
{
namespace Overlays
{

namespace internal
{
constexpr int fftOrder = 13;
constexpr int fftSize = 8192;

// Really wish span was available.
using FftScopeType = std::array<float, fftSize / 2>;
} // namespace internal

// Waveform-specific display taken from s(m)exoscope GPL code and adapted to use with Surge.
class WaveformDisplay : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
  public:
    enum TriggerType
    {
        kTriggerFree = 0,
        kTriggerRising,
        kTriggerFalling,
        kTriggerInternal,
        kNumTriggerTypes
    };

    struct Parameters
    {
        float trigger_speed = 0.5f;              // internal trigger speed, knob
        TriggerType trigger_type = kTriggerFree; // trigger type, selection
        float trigger_level = 0.5f;              // trigger level, slider
        float trigger_limit = 0.5f;              // retrigger threshold, knob
        float time_window = 0.75f;               // X-range, knob
        float amp_window = 0.5f;                 // Y-range, knob
        bool freeze = false;                     // freeze display, on/off
        bool dc_kill = false;                    // kill DC, on/off
        bool sync_draw = false;                  // sync redraw, on/off

        // Number of pixels per sample, calculated from the time window 0-1 slider value.
        float counterSpeed() const;

        // Calculate the amplitude "gain" value from the amp window 0-1 slider value.
        float gain() const;

        // Calculate the final trigger level value from the 0-1 slider value.
        float triggerLevel() const;
    };

    WaveformDisplay(SurgeGUIEditor *e, SurgeStorage *s);

    const Parameters &getParameters() const;
    void setParameters(Parameters parameters);

    void mouseDown(const juce::MouseEvent &event) override;
    void paint(juce::Graphics &g) override;
    void resized() override;
    void process(std::vector<float> data);

  private:
    SurgeGUIEditor *editor_;
    SurgeStorage *storage_;
    Parameters params_;

    // Global lock for the class.
    std::mutex lock_;

    std::vector<juce::Point<float>> peaks;
    std::vector<juce::Point<float>> copy; // Copy of peaks, for sync drawing.

    // Index into the peak-array.
    std::size_t index;

    // counter which is used to set the amount of samples/pixel.
    float counter;

    // max/min peak in this block.
    float max, min, maxR, minR;

    // the last peak we encountered was a maximum?
    bool lastIsMax;

    // the previous sample (for edge-triggers)
    float previousSample;

    // the internal trigger oscillator
    float triggerPhase;

    // trigger limiter
    int triggerLimitPhase;

    // DC killer.
    float dcKill, dcFilterTemp;

    // Point that marks where a click happened.
    juce::Point<int> clickPoint;
};

// Spectrum-specific display.
class SpectrumDisplay : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
  public:
    static constexpr float lowFreq = 10;
    static constexpr float highFreq = 24000;

    struct Parameters
    {
        float noise_floor = 0.f; // Noise floor level, bottom of the scope. Min -100. Slider.
        float max_db = 1.f;      // Maximum dB displayed. Slider. Maxes out at 0. Slider.
        bool freeze = false;     // Freeze display, on/off.

        // Range of decibels shown in the display, calculated from slider values.
        float dbRange() const;

        // Calculate the noise floor in decibels from the slider value (noise_floor).
        float noiseFloor() const;

        // Calculate the maximum decibels shown from the slider value (max_db).
        float maxDb() const;
    };

    SpectrumDisplay(SurgeGUIEditor *e, SurgeStorage *s);

    const Parameters &getParameters() const;
    void setParameters(Parameters parameters);

    void paint(juce::Graphics &g) override;
    void resized() override;
    void updateScopeData(internal::FftScopeType::iterator begin,
                         internal::FftScopeType::iterator end);

  private:
    float interpolate(const float y0, const float y1,
                      std::chrono::time_point<std::chrono::steady_clock> t) const;
    // data_lock_ *must* be held by the caller.
    void recalculateScopeData();

    SurgeGUIEditor *editor_;
    SurgeStorage *storage_;
    Parameters params_;
    std::chrono::duration<float> mtbs_;
    std::chrono::time_point<std::chrono::steady_clock> last_updated_time_;
    std::mutex data_lock_;
    internal::FftScopeType new_scope_data_;
    internal::FftScopeType displayed_data_;
    // Why a third array? We calculate into the other two, and if the parameters
    // change we have to update our calculations from the beginning.
    internal::FftScopeType incoming_scope_data_;
    bool display_dirty_;
};

class Oscilloscope : public OverlayComponent,
                     public Surge::GUI::SkinConsumingComponent,
                     public Surge::GUI::Hoverable
{
  public:
    Oscilloscope(SurgeGUIEditor *e, SurgeStorage *s);
    virtual ~Oscilloscope();

    void onSkinChanged() override;
    void paint(juce::Graphics &g) override;
    void resized() override;
    void updateDrawing();
    void visibilityChanged() override;

  private:
    enum ChannelSelect
    {
        LEFT = 1,
        RIGHT = 2,
        STEREO = 3,
        OFF = 4,
    };

    enum ScopeMode
    {
        WAVEFORM = 1,
        SPECTRUM = 2,
    };

    // Child component for handling the drawing of the background. Done as a separate child instead
    // of in the Oscilloscope class so the display, which is repainting at 20-30 hz, doesn't mark
    // this as dirty as it repaints. It sucks up a ton of CPU otherwise.
    class Background : public juce::Component, public Surge::GUI::SkinConsumingComponent
    {
      public:
        explicit Background(SurgeStorage *s);
        void paint(juce::Graphics &g) override;
        void updateBackgroundType(ScopeMode mode);
        void updateBounds(juce::Rectangle<int> local_bounds, juce::Rectangle<int> scope_bounds);
        void updateParameters(SpectrumDisplay::Parameters params);
        void updateParameters(WaveformDisplay::Parameters params);

      private:
        void paintSpectrumBackground(juce::Graphics &g);
        void paintWaveformBackground(juce::Graphics &g);

        // No lock on these because they can only get updated during the same thread as the one
        // doing the painting.
        SurgeStorage *storage_;
        ScopeMode mode_;
        juce::Rectangle<int> scope_bounds_;
        SpectrumDisplay::Parameters spectrum_params_;
        WaveformDisplay::Parameters waveform_params_;
    };

    class SpectrumParameters : public juce::Component, public Surge::GUI::SkinConsumingComponent
    {
      public:
        SpectrumParameters(SurgeGUIEditor *e, SurgeStorage *s, juce::Component *parent);

        std::optional<SpectrumDisplay::Parameters> getParamsIfDirty();

        void onSkinChanged() override;
        void paint(juce::Graphics &g) override;
        void resized() override;

      private:
        SurgeGUIEditor *editor_;
        SurgeStorage *storage_;
        juce::Component
            *parent_; // Saved here so we can provide it to the children at construction time.
        SpectrumDisplay::Parameters params_;
        bool params_changed_;
        std::mutex params_lock_;

        Surge::Widgets::SelfUpdatingModulatableSlider noise_floor_;
        Surge::Widgets::SelfUpdatingModulatableSlider max_db_;
        Surge::Widgets::SelfDrawToggleButton freeze_;
    };

    class WaveformParameters : public juce::Component, public Surge::GUI::SkinConsumingComponent
    {
      public:
        WaveformParameters(SurgeGUIEditor *e, SurgeStorage *s, juce::Component *parent);

        std::optional<WaveformDisplay::Parameters> getParamsIfDirty();

        void onSkinChanged() override;
        void paint(juce::Graphics &g) override;
        void resized() override;

      private:
        SurgeGUIEditor *editor_;
        SurgeStorage *storage_;
        juce::Component
            *parent_; // Saved here so we can provide it to the children at construction time.
        WaveformDisplay::Parameters params_;
        bool params_changed_;
        std::mutex params_lock_;

        Surge::Widgets::SelfUpdatingModulatableSlider trigger_speed_;
        Surge::Widgets::SelfUpdatingModulatableSlider trigger_level_;
        Surge::Widgets::SelfUpdatingModulatableSlider trigger_limit_;
        Surge::Widgets::SelfUpdatingModulatableSlider time_window_;
        Surge::Widgets::SelfUpdatingModulatableSlider amp_window_;
        Surge::Widgets::ClosedMultiSwitchSelfDraw trigger_type_;
        Surge::Widgets::SelfDrawToggleButton freeze_;
        Surge::Widgets::SelfDrawToggleButton dc_kill_;
        Surge::Widgets::SelfDrawToggleButton sync_draw_;
    };

    // Child component for handling the switch between waveform/spectrum.
    class SwitchButton : public Surge::Widgets::MultiSwitchSelfDraw,
                         public Surge::GUI::IComponentTagValue::Listener
    {
      public:
        explicit SwitchButton(Oscilloscope &parent);
        void valueChanged(Surge::GUI::IComponentTagValue *p) override;

      private:
        Oscilloscope &parent_;
    };

    // Height of parameter window, in pixels.
    static constexpr const int paramsHeight = 78;

    void calculateSpectrumData();
    void changeScopeType(ScopeMode type);
    juce::Rectangle<int> getScopeRect();
    void pullData();
    void toggleChannel();

    SurgeGUIEditor *editor_{nullptr};
    SurgeStorage *storage_{nullptr};
    juce::dsp::FFT forward_fft_;
    juce::dsp::WindowingFunction<float> window_;
    std::array<float, 2 * internal::fftSize> fft_data_;
    int pos_;
    internal::FftScopeType scope_data_;
    ChannelSelect channel_selection_;
    ScopeMode scope_mode_;
    // Global lock for all data members accessed concurrently.
    std::mutex data_lock_;

    // Members for the data-pulling thread.
    std::thread fft_thread_;
    std::atomic_bool complete_;
    std::condition_variable channels_off_;

    // Visual elements.
    Surge::Widgets::SelfDrawToggleButton left_chan_button_;
    Surge::Widgets::SelfDrawToggleButton right_chan_button_;
    SwitchButton scope_mode_button_;
    Background background_;
    SpectrumDisplay spectrum_;
    WaveformDisplay waveform_;
    SpectrumParameters spectrum_parameters_;
    WaveformParameters waveform_parameters_;
};

} // namespace Overlays
} // namespace Surge
