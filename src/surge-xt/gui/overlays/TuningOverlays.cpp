#include "TuningOverlays.h"
#include "RuntimeFont.h"
#include "SurgeStorage.h"
#include "UserDefaults.h"
#include "SurgeGUIUtils.h"
#include "SurgeGUIEditor.h"
#include "SurgeSynthEditor.h" // bit gross but so I can do DnD
#include "widgets/MultiSwitch.h"
#include "fmt/core.h"
#include <chrono>
#include "juce_gui_extra/juce_gui_extra.h"

namespace Surge
{
namespace Overlays
{

class TuningTableListBoxModel : public juce::TableListBoxModel,
                                public Surge::GUI::SkinConsumingComponent
{
  public:
    TuningTableListBoxModel() {}
    ~TuningTableListBoxModel() { table = nullptr; }

    void setTableListBox(juce::TableListBox *t) { table = t; }

    void setupDefaultHeaders(juce::TableListBox *table)
    {
        table->setHeaderHeight(15);
        table->setRowHeight(13);
        table->getHeader().addColumn("Note", 1, 50);
        table->getHeader().addColumn("Freq (hz)", 2, 50);
    }

    virtual int getNumRows() override { return 128; }
    virtual void paintRowBackground(juce::Graphics &g, int rowNumber, int width, int height,
                                    bool rowIsSelected) override
    {
        if (!table)
            return;

        g.fillAll(skin->getColor(Colors::TuningOverlay::FrequencyKeyboard::Separator));
    }

    int mcoff{1};
    void setMiddleCOff(int m)
    {
        mcoff = m;
        if (table)
            table->repaint();
    }

    virtual void paintCell(juce::Graphics &g, int rowNumber, int columnID, int width, int height,
                           bool rowIsSelected) override
    {
        namespace clr = Colors::TuningOverlay::FrequencyKeyboard;
        if (!table)
            return;

        int noteInScale = rowNumber % 12;
        bool whitekey = true;
        bool noblack = false;
        if ((noteInScale == 1 || noteInScale == 3 || noteInScale == 6 || noteInScale == 8 ||
             noteInScale == 10))
        {
            whitekey = false;
        }
        if (noteInScale == 4 || noteInScale == 11)
            noblack = true;

        // Black Key
        auto kbdColour = skin->getColor(clr::BlackKey);
        if (whitekey)
            kbdColour = skin->getColor(clr::WhiteKey);

        bool no = false;
        auto pressedColour = skin->getColor(clr::PressedKey);
        if (notesOn[rowNumber])
        {
            no = true;
            kbdColour = pressedColour;
        }

        g.fillAll(kbdColour);
        if (!whitekey && columnID != 1)
        {
            g.setColour(skin->getColor(clr::Separator));
            // draw an inset top and bottom
            g.fillRect(0, 0, width - 1, 1);
            g.fillRect(0, height - 1, width - 1, 1);
        }

        int txtOff = 0;
        if (columnID == 1)
        {
            // Black Key
            if (!whitekey)
            {
                txtOff = 10;
                // "Black Key"
                auto kbdColour = skin->getColor(clr::BlackKey);
                auto kbc = skin->getColor(clr::WhiteKey);
                g.setColour(kbc);
                g.fillRect(-1, 0, txtOff, height + 2);

                // OK so now check neighbors
                if (rowNumber > 0 && notesOn[rowNumber - 1])
                {
                    g.setColour(pressedColour);
                    g.fillRect(0, 0, txtOff, height / 2);
                }
                if (rowNumber < 127 && notesOn[rowNumber + 1])
                {
                    g.setColour(pressedColour);
                    g.fillRect(0, height / 2, txtOff, height / 2 + 1);
                }
                g.setColour(skin->getColor(clr::BlackKey));
                g.fillRect(0, height / 2, txtOff, 1);

                if (no)
                {
                    g.fillRect(txtOff, 0, width - 1 - txtOff, 1);
                    g.fillRect(txtOff, height - 1, width - 1 - txtOff, 1);
                    g.fillRect(txtOff, 0, 1, height - 1);
                }
            }
        }

        auto mn = rowNumber;
        double fr = tuning.frequencyForMidiNote(mn);

        std::string notenum, notename, display;

        g.setColour(skin->getColor(clr::Separator));
        g.fillRect(width - 1, 0, 1, height);
        if (noblack)
            g.fillRect(0, height - 1, width, 1);

        g.setColour(skin->getColor(clr::Text));
        if (no)
            g.setColour(skin->getColor(clr::PressedKeyText));
        auto just = juce::Justification::centredLeft;
        switch (columnID)
        {
        case 1:
        {
            notenum = std::to_string(mn);
            notename = noteInScale % 12 == 0 ? fmt::format("C{:d}", rowNumber / 12 - mcoff) : "";
            static std::vector<std::string> nn = {
                {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}};

            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            g.drawText(notename, 2 + txtOff, 0, width - 4, height, juce::Justification::centredLeft,
                       false);
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(7));
            g.drawText(notenum, 2 + txtOff, 0, width - 2 - txtOff - 2, height,
                       juce::Justification::centredRight, false);

            break;
        }
        case 2:
        {
            just = juce::Justification::centredRight;
            display = fmt::format("{:.2f}", fr);
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            g.drawText(display, 2 + txtOff, 0, width - 4, height, juce::Justification::centredRight,
                       false);
            break;
        }
        }
    }

    virtual void cellClicked(int rowNumber, int columnId, const juce::MouseEvent &e) override {}

    virtual void tuningUpdated(const Tunings::Tuning &newTuning)
    {
        tuning = newTuning;
        if (table)
            table->repaint();
    }

    Tunings::Tuning tuning;
    std::bitset<128> notesOn;
    std::unique_ptr<juce::PopupMenu> rmbMenu;
    juce::TableListBox *table{nullptr};
};

class RadialScaleGraph;

class RadialScaleGraph : public juce::Component,
                         public juce::TextEditor::Listener,
                         public Surge::GUI::SkinConsumingComponent
{
  public:
    RadialScaleGraph()
    {
        toneList = std::make_unique<juce::Viewport>();
        toneInterior = std::make_unique<juce::Component>();
        toneList->setViewedComponent(toneInterior.get(), false);
        addAndMakeVisible(*toneList);

        setTuning(Tunings::Tuning(Tunings::evenTemperament12NoteScale(), Tunings::tuneA69To(440)));
    }

    void setTuning(const Tunings::Tuning &t)
    {
        int priorLen = tuning.scale.count;
        tuning = t;
        scale = t.scale;

        if (toneEditors.empty() || priorLen != scale.count || toneEditors.size() != scale.count + 1)
        {
            toneInterior->removeAllChildren();
            auto w = usedForSidebar - 15;
            auto m = 2;
            auto h = 24;
            toneInterior->setSize(w, (scale.count + 1) * (h + m));
            toneEditors.clear();
            for (int i = 0; i < scale.count + 1; ++i)
            {
                auto te = std::make_unique<juce::TextEditor>("tone");
                te->setText(std::to_string(i), juce::NotificationType::dontSendNotification);
                te->setBounds(m, i * (h + m) + m, w - 2 * m, h);
                te->setEnabled(i != 0);
                te->addListener(this);
                toneInterior->addAndMakeVisible(*te);
                toneEditors.push_back(std::move(te));
            }
        }

        toneEditors[0]->setText("-", juce::NotificationType::dontSendNotification);
        for (int i = 0; i < scale.count; ++i)
        {
            auto td = fmt::format("{:.5f}", scale.tones[i].cents);
            if (scale.tones[i].type == Tunings::Tone::kToneRatio)
            {
                td = fmt::format("{:d}/{:d}", scale.tones[i].ratio_n, scale.tones[i].ratio_d);
            }
            toneEditors[i + 1]->setText(td, juce::NotificationType::dontSendNotification);
        }

        notesOn.clear();
        notesOn.resize(scale.count);
        for (int i = 0; i < scale.count; ++i)
            notesOn[i] = false;
        setNotesOn(bitset);
    }

  private:
    void textEditorReturnKeyPressed(juce::TextEditor &editor) override;

  public:
    virtual void paint(juce::Graphics &g) override;
    Tunings::Tuning tuning;
    Tunings::Scale scale;
    std::vector<juce::Rectangle<float>> screenHotSpots;
    int hotSpotIndex = -1;
    double dInterval, centsAtMouseDown, dIntervalAtMouseDown;

    juce::AffineTransform screenTransform, screenTransformInverted;
    std::function<void(int index, double)> onToneChanged = [](int, double) {};
    std::function<void(int index, const std::string &s)> onToneStringChanged =
        [](int, const std::string &) {};
    static constexpr int usedForSidebar = 140;

    std::unique_ptr<juce::Viewport> toneList;
    std::unique_ptr<juce::Component> toneInterior;
    std::vector<std::unique_ptr<juce::TextEditor>> toneEditors;

    void resized() override { toneList->setBounds(0, 0, usedForSidebar, getHeight()); }

    std::vector<bool> notesOn;
    std::bitset<128> bitset{0};
    void setNotesOn(const std::bitset<128> &bs)
    {
        bitset = bs;
        for (int i = 0; i < scale.count; ++i)
            notesOn[i] = false;

        for (int i = 0; i < 128; ++i)
        {
            if (bitset[i])
            {
                notesOn[tuning.scalePositionForMidiNote(i)] = true;
            }
        }

        for (int i = 0; i < scale.count + 1; ++i)
        {
            auto ni = i % (scale.count);
            if (notesOn[ni])
            {
                toneEditors[i]->setColour(juce::TextEditor::ColourIds::backgroundColourId,
                                          juce::Colour(0xFFaaaa50));
            }
            else
            {
                toneEditors[i]->setColour(juce::TextEditor::ColourIds::backgroundColourId,
                                          juce::Colours::black);
            }
            toneEditors[i]->repaint();
        }
        toneInterior->repaint();
        toneList->repaint();
        repaint();
    }
    virtual void mouseMove(const juce::MouseEvent &e) override;
    virtual void mouseDown(const juce::MouseEvent &e) override;
    virtual void mouseDrag(const juce::MouseEvent &e) override;
};

void RadialScaleGraph::paint(juce::Graphics &g)
{
    if (notesOn.size() != scale.count)
    {
        notesOn.clear();
        notesOn.resize(scale.count);
        for (int i = 0; i < scale.count; ++i)
            notesOn[i] = 0;
    }
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    int w = getWidth() - usedForSidebar;
    int h = getHeight();
    float r = std::min(w, h) / 2.1;
    float xo = (w - 2 * r) / 2.0;
    float yo = (h - 2 * r) / 2.0;
    double outerRadiusExtension = 0.4;

    g.saveState();

    screenTransform = juce::AffineTransform()
                          .scaled(1.0 / (1.0 + outerRadiusExtension * 1.1))
                          .scaled(r, -r)
                          .translated(r, r)
                          .translated(xo, yo)
                          .translated(usedForSidebar, 0);
    screenTransformInverted = screenTransform.inverted();

    g.addTransform(screenTransform);

    // We are now in a normal x y 0 1 coordinate system with 0,0 at the center. Cool

    // So first things first - scan for range.
    double ETInterval = scale.tones.back().cents / scale.count;
    double dIntMin = 0, dIntMax = 0;
    for (int i = 0; i < scale.count; ++i)
    {
        auto t = scale.tones[i];
        auto c = t.cents;

        auto intervalDistance = (c - ETInterval * (i + 1)) / ETInterval;
        dIntMax = std::max(intervalDistance, dIntMax);
        dIntMin = std::min(intervalDistance, dIntMin);
    }
    double range = std::max(0.01, std::max(dIntMax, -dIntMin / 2.0)); // twice as many inside rings
    int iRange = std::ceil(range);

    dInterval = outerRadiusExtension / iRange;
    double nup = iRange;
    double ndn = (int)(iRange * 1.6);

    // Now draw the interval circles
    for (int i = -ndn; i <= nup; ++i)
    {
        if (i == 0)
        {
        }
        else
        {
            float pos = 1.0 * std::abs(i) / ndn;
            float cpos = std::max(0.f, pos);

            g.setColour(juce::Colour(110, 110, 120)
                            .interpolatedWith(getLookAndFeel().findColour(
                                                  juce::ResizableWindow::backgroundColourId),
                                              cpos * 0.8));

            float rad = 1.0 + dInterval * i;
            g.drawEllipse(-rad, -rad, 2 * rad, 2 * rad, 0.01);
        }
    }

    for (int i = 0; i < scale.count; ++i)
    {
        double frac = 1.0 * i / (scale.count);
        double sx = std::sin(frac * 2.0 * juce::MathConstants<double>::pi);
        double cx = std::cos(frac * 2.0 * juce::MathConstants<double>::pi);

        if (notesOn[i])
            g.setColour(juce::Colour(255, 255, 255));
        else
            g.setColour(juce::Colour(110, 110, 120));
        g.drawLine(0, 0, (1.0 + outerRadiusExtension) * sx, (1.0 + outerRadiusExtension) * cx,
                   0.01);

        g.saveState();
        g.addTransform(juce::AffineTransform::rotation((-frac + 0.25) * 2.0 *
                                                       juce::MathConstants<double>::pi));
        g.addTransform(juce::AffineTransform::translation(1.0 + outerRadiusExtension, 0.0));
        g.addTransform(juce::AffineTransform::rotation(juce::MathConstants<double>::pi * 0.5));
        g.addTransform(juce::AffineTransform::scale(-1.0, 1.0));

        if (notesOn[i])
            g.setColour(juce::Colour(255, 255, 255));
        else
            g.setColour(juce::Colour(200, 200, 240));
        juce::Rectangle<float> textPos(0, -0.1, 0.1, 0.1);
        g.setFont(0.1);
        g.drawText(juce::String(i), textPos, juce::Justification::centred, 1);
        g.restoreState();
    }

    // Draw the ring at 1.0
    g.setColour(juce::Colour(255, 255, 255));
    g.drawEllipse(-1, -1, 2, 2, 0.01);

    // Then draw ellipses for each note
    screenHotSpots.clear();

    for (int i = 1; i <= scale.count; ++i)
    {
        double frac = 1.0 * i / (scale.count);
        double sx = std::sin(frac * 2.0 * juce::MathConstants<double>::pi);
        double cx = std::cos(frac * 2.0 * juce::MathConstants<double>::pi);

        auto t = scale.tones[i - 1];
        auto c = t.cents;
        auto expectedC = scale.tones.back().cents / scale.count;

        auto rx = 1.0 + dInterval * (c - expectedC * i) / expectedC;

        float x0 = rx * sx - 0.05, y0 = rx * cx - 0.05, dx = 0.1, dy = 0.1;

        if (notesOn[i])
        {
            g.setColour(juce::Colour(255, 255, 255));
            g.drawLine(sx, cx, rx * sx, rx * cx, 0.03);
        }

        juce::Colour drawColour(200, 200, 200);

        // FIXME - this colormap is bad
        if (rx < 0.99)
        {
            // use a blue here
            drawColour = juce::Colour(200 * (1.0 - 0.6 * rx), 200 * (1.0 - 0.6 * rx), 200);
        }
        else if (rx > 1.01)
        {
            // Use a yellow here
            drawColour = juce::Colour(200, 200, 200 * (rx - 1.0));
        }

        if (hotSpotIndex == i - 1)
            drawColour = drawColour.brighter(0.6);

        g.setColour(drawColour);

        g.drawLine(sx, cx, rx * sx, rx * cx, 0.01);
        g.fillEllipse(x0, y0, dx, dy);

        if (hotSpotIndex != i - 1)
        {
            g.setColour(drawColour.brighter(0.6));
            g.drawEllipse(x0, y0, dx, dy, 0.01);
        }

        if (notesOn[i % scale.count])
        {
            g.setColour(juce::Colour(255, 255, 255));
            g.drawEllipse(x0, y0, dx, dy, 0.02);
        }

        dx += x0;
        dy += y0;
        screenTransform.transformPoint(x0, y0);
        screenTransform.transformPoint(dx, dy);
        screenHotSpots.push_back(juce::Rectangle<float>(x0, dy, dx - x0, y0 - dy));
    }

    g.restoreState();
}

struct IntervalMatrix : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    IntervalMatrix(TuningOverlay *o) : overlay(o)
    {
        viewport = std::make_unique<juce::Viewport>();
        intervalPainter = std::make_unique<IntervalPainter>(this);
        viewport->setViewedComponent(intervalPainter.get(), false);

        addAndMakeVisible(*viewport);
    };
    virtual ~IntervalMatrix() = default;

    void setTuning(const Tunings::Tuning &t)
    {
        tuning = t;
        setNotesOn(bitset);
        intervalPainter->setSizeFromTuning();
        intervalPainter->repaint();
    }

    struct IntervalPainter : public juce::Component
    {
        enum Mode
        {
            INTERV,
            DIST
        } mode{INTERV};
        IntervalPainter(IntervalMatrix *m) : matrix(m) {}

        static constexpr int cellH{14}, cellW{35};
        void setSizeFromTuning()
        {
            auto ic = matrix->tuning.scale.count + 2;
            auto nh = ic * cellH;
            auto nw = ic * cellW;

            setSize(nw, nh);
        }

        // ToDo: Skin Colors Here
        void paint(juce::Graphics &g) override
        {
            g.fillAll(juce::Colour(0xFF979797));
            auto ic = matrix->tuning.scale.count;
            int mt = ic + 2;
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            for (int i = 0; i < mt; ++i)
            {
                for (int j = 0; j < mt; ++j)
                {
                    bool isHovered = false;
                    if ((i == hoverI && j == hoverJ) || (i == 0 && j == hoverJ) ||
                        (i == hoverI && j == 0))
                    {
                        isHovered = true;
                    }
                    auto bx = juce::Rectangle<int>(i * cellW, j * cellH, cellW - 1, cellH - 1);
                    if ((i == 0 || j == 0) && (i + j))
                    {
                        auto no = false;
                        if (i > 0)
                            no = no || matrix->notesOn[i - 1];
                        if (j > 0)
                            no = no || matrix->notesOn[j - 1];

                        if (no)
                            g.setColour(juce::Colours::darkgreen);
                        else
                            g.setColour(juce::Colours::darkblue);
                        g.fillRect(bx);
                        if (no)
                        {
                            g.setColour(juce::Colours::white);
                            g.drawRect(bx);
                        }

                        auto lb = std::to_string(i + j - 1);

                        if (isHovered)
                            g.setColour(juce::Colours::white);
                        else
                            g.setColour(juce::Colours::lightblue);
                        g.drawText(lb, bx, juce::Justification::centred);
                    }
                    else if (i == j)
                    {
                        g.setColour(juce::Colours::darkgrey);
                        g.fillRect(bx);
                    }
                    else if (i > j)
                    {
                        auto centsi = 0.0;
                        auto centsj = 0.0;
                        if (i > 1)
                            centsi = matrix->tuning.scale.tones[i - 2].cents;
                        if (j > 1)
                            centsj = matrix->tuning.scale.tones[j - 2].cents;

                        auto cdiff = centsi - centsj;
                        auto disNote = i - j;
                        auto lastTone =
                            matrix->tuning.scale.tones[matrix->tuning.scale.count - 1].cents;
                        auto evenStep = lastTone / matrix->tuning.scale.count;
                        auto desCents = disNote * evenStep;

                        if (cdiff < desCents)
                        {
                            // we are flat of even
                            auto dist = std::min((desCents - cdiff) / evenStep, 1.0);
                            auto r = (int)((1.0 - dist) * 200);
                            g.setColour(juce::Colour(255, 255, r));
                        }
                        else if (fabs(cdiff - desCents) < 0.1)
                        {
                            g.setColour(juce::Colours::white);
                        }
                        else
                        {
                            auto dist = std::min(-(desCents - cdiff) / evenStep, 1.0);
                            auto b = (int)((1.0 - dist) * 100) + 130;
                            g.setColour(juce::Colour(b, b, 255));
                        }
                        g.fillRect(bx);

                        auto displayCents = cdiff;
                        if (mode == DIST)
                            displayCents = cdiff - desCents;
                        auto lb = fmt::format("{:.1f}", displayCents);
                        if (isHovered)
                            g.setColour(juce::Colours::darkgreen);
                        else
                            g.setColour(juce::Colours::black);
                        g.drawText(lb, bx, juce::Justification::centred);

                        if (isHovered)
                        {
                            g.setColour(juce::Colour(255, 255, 255));
                            g.drawRect(bx);
                        }
                    }
                }
            }
        }

        int hoverI{-1}, hoverJ{-1};

        juce::Point<float> lastMousePos;
        void mouseDown(const juce::MouseEvent &e) override
        {
            if (!Surge::GUI::showCursor(matrix->overlay->storage))
            {
                juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(
                    true);
            }
            lastMousePos = e.position;
        }
        void mouseUp(const juce::MouseEvent &e) override
        {
            if (!Surge::GUI::showCursor(matrix->overlay->storage))
            {
                juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(
                    false);
                auto p = localPointToGlobal(e.mouseDownPosition);
                juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(p);
            }
            // show
        }
        void mouseDrag(const juce::MouseEvent &e) override
        {
            auto dPos = e.position.getY() - lastMousePos.getY();
            dPos = -dPos;
            auto speed = 0.5;
            if (e.mods.isShiftDown())
                speed = 0.1;
            dPos = dPos * speed;
            lastMousePos = e.position;

            auto i = hoverI;
            if (i > 1)
            {
                auto centsi = matrix->tuning.scale.tones[i - 2].cents + dPos;
                matrix->overlay->onToneChanged(i - 2, centsi);
            }
        }
        void mouseEnter(const juce::MouseEvent &e) override { repaint(); }

        void mouseExit(const juce::MouseEvent &e) override
        {
            hoverI = -1;
            hoverJ = -1;
            repaint();

            setMouseCursor(juce::MouseCursor::NormalCursor);
        }

        void mouseMove(const juce::MouseEvent &e) override
        {
            if (setupHoverFrom(e.position))
                repaint();
            if (hoverI >= 1 && hoverI <= matrix->tuning.scale.count && hoverJ >= 1 &&
                hoverJ <= matrix->tuning.scale.count && hoverI > hoverJ)
            {
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            }
            else
            {
                setMouseCursor(juce::MouseCursor::NormalCursor);
            }
        }

        bool setupHoverFrom(const juce::Point<float> &here)
        {
            int ohi = hoverI, ohj = hoverJ;

            //  auto bx = juce::Rectangle<int>(i * cellW, j * cellH, cellW - 1, cellH - 1);
            // box x is i*cellW to i*cellW + cellW
            // box x / cellW is i to i + 1
            // floor(box x / cellW ) is i
            hoverI = floor(here.x / cellW);
            hoverJ = floor(here.y / cellH);
            if (ohi != hoverI || ohj != hoverJ)
            {
                return true;
            }
            return false;
        }

        IntervalMatrix *matrix;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IntervalPainter);
    };

    void resized() override
    {
        viewport->setBounds(getLocalBounds().reduced(2));
        intervalPainter->setSizeFromTuning();
    }

    std::vector<bool> notesOn;
    std::bitset<128> bitset{0};
    void setNotesOn(const std::bitset<128> &bs)
    {
        bitset = bs;
        notesOn.resize(tuning.scale.count + 1);
        for (int i = 0; i < tuning.scale.count; ++i)
            notesOn[i] = false;
        notesOn[tuning.scale.count] = notesOn[0];

        for (int i = 0; i < 128; ++i)
        {
            if (bitset[i])
            {
                notesOn[tuning.scalePositionForMidiNote(i)] = true;
            }
        }
        intervalPainter->repaint();
        repaint();
    }

    std::unique_ptr<IntervalPainter> intervalPainter;
    std::unique_ptr<juce::Viewport> viewport;

    Tunings::Tuning tuning;
    TuningOverlay *overlay{nullptr};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IntervalMatrix);
};

void RadialScaleGraph::mouseMove(const juce::MouseEvent &e)
{
    int ohsi = hotSpotIndex;
    hotSpotIndex = -1;
    int h = 0;
    for (auto r : screenHotSpots)
    {
        if (r.contains(e.getPosition().toFloat()))
            hotSpotIndex = h;
        h++;
    }
    if (ohsi != hotSpotIndex)
        repaint();
}
void RadialScaleGraph::mouseDown(const juce::MouseEvent &e)
{
    if (hotSpotIndex == -1)
        centsAtMouseDown = 0;
    else
    {
        centsAtMouseDown = scale.tones[hotSpotIndex].cents;
        dIntervalAtMouseDown = dInterval;
    }
}

void RadialScaleGraph::mouseDrag(const juce::MouseEvent &e)
{
    if (hotSpotIndex != -1)
    {
        auto mdp = e.getMouseDownPosition().toFloat();
        auto xd = mdp.getX();
        auto yd = mdp.getY();
        screenTransformInverted.transformPoint(xd, yd);

        auto mp = e.getPosition().toFloat();
        auto x = mp.getX();
        auto y = mp.getY();
        screenTransformInverted.transformPoint(x, y);

        auto dr = -sqrt(xd * xd + yd * yd) + sqrt(x * x + y * y);
        dr = dr * 0.7; // FIXME - make this a variable
        onToneChanged(hotSpotIndex, centsAtMouseDown + 100 * dr / dIntervalAtMouseDown);
    }
}

void RadialScaleGraph::textEditorReturnKeyPressed(juce::TextEditor &editor)
{
    for (int i = 1; i <= scale.count; ++i)
    {
        if (&editor == toneEditors[i].get())
        {
            onToneStringChanged(i - 1, editor.getText().toStdString());
        }
    }
}

struct SCLKBMDisplay : public juce::Component,
                       Surge::GUI::SkinConsumingComponent,
                       juce::TextEditor::Listener,
                       juce::CodeDocument::Listener
{
    SCLKBMDisplay(TuningOverlay *o) : overlay(o)
    {
        sclDocument = std::make_unique<juce::CodeDocument>();
        sclDocument->addListener(this);
        sclTokeniser = std::make_unique<SCLKBMTokeniser>();
        scl = std::make_unique<juce::CodeEditorComponent>(*sclDocument, sclTokeniser.get());
        scl->setFont(Surge::GUI::getFontManager()->getFiraMonoAtSize(10));
        scl->setLineNumbersShown(false);
        scl->setScrollbarThickness(5);
        scl->setColour(juce::CodeEditorComponent::ColourIds::backgroundColourId,
                       juce::Colour(0xFFE3E3E3));
        addAndMakeVisible(*scl);

        kbmDocument = std::make_unique<juce::CodeDocument>();
        kbmDocument->addListener(this);
        kbmTokeniser = std::make_unique<SCLKBMTokeniser>(false);

        kbm = std::make_unique<juce::CodeEditorComponent>(*kbmDocument, kbmTokeniser.get());
        kbm->setFont(Surge::GUI::getFontManager()->getFiraMonoAtSize(10));
        kbm->setLineNumbersShown(false);
        kbm->setScrollbarThickness(5);
        kbm->setColour(juce::CodeEditorComponent::ColourIds::backgroundColourId,
                       juce::Colour(0xFFE3E3E3));
        addAndMakeVisible(*kbm);
    }

    struct SCLKBMTokeniser : public juce::CodeTokeniser
    {
        enum
        {
            token_Error,
            token_Comment,
            token_Text,
            token_Cents,
            token_Ratio
        };

        bool isSCL{false};
        SCLKBMTokeniser(bool s = true) : isSCL(s) {}

        int readNextToken(juce::CodeDocument::Iterator &source) override
        {
            auto firstChar = source.peekNextChar();
            if (firstChar == '!')
            {
                source.skipToEndOfLine();
                return token_Comment;
            }
            if (!isSCL)
            {
                source.skipToEndOfLine();
                return token_Text;
            }
            source.skipWhitespace();
            auto nc = source.nextChar();
            while (nc >= '0' && nc <= '9' && nc)
            {
                nc = source.nextChar();
            }
            source.previousChar(); // in case we are just numbers
            source.skipToEndOfLine();
            if (nc == '/')
                return token_Ratio;
            if (nc == '.')
                return token_Cents;
            return token_Text;
        }

        juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override
        {
            struct Type
            {
                const char *name;
                uint32_t colour;
            };

            // clang-format off
            const Type types[] = {
                {"Error", 0xffcc0000},
                {"Comment", 0xFF703000},
                {"Text", 0xFF242424},
                {"Cents", 0xFF1212A0},
                {"Ratio", 0xFF12A012},
            };
            // clang-format on

            juce::CodeEditorComponent::ColourScheme cs;

            for (auto &t : types)
                cs.set(t.name, juce::Colour(t.colour));

            return cs;
        }
    };

    std::unique_ptr<juce::CodeDocument> sclDocument, kbmDocument;
    std::unique_ptr<SCLKBMTokeniser> sclTokeniser, kbmTokeniser;

    void setTuning(const Tunings::Tuning &t)
    {
        sclDocument->replaceAllContent(t.scale.rawText);
        kbmDocument->replaceAllContent(t.keyboardMapping.rawText);
        setApplyEnabled(false);
    }

    void resized() override
    {
        auto w = getWidth();
        auto h = getHeight();
        auto b = juce::Rectangle<int>(0, 0, w / 2, h).reduced(3, 3);

        scl->setBounds(b);
        kbm->setBounds(b.translated(w / 2, 0));
    }

    void setApplyEnabled(bool b);

    void codeDocumentTextInserted(const juce::String &newText, int insertIndex) override
    {
        setApplyEnabled(true);
    }
    void codeDocumentTextDeleted(int startIndex, int endIndex) override { setApplyEnabled(true); }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colour(0xFF979797));
        g.setColour(juce::Colour(0xFF242424));
        g.drawRect(scl->getBounds().expanded(1), 2);
        g.drawRect(kbm->getBounds().expanded(1), 2);
    }

    void textEditorTextChanged(juce::TextEditor &editor) override { setApplyEnabled(true); }

    std::function<void(const std::string &scl, const std::string &kbl)> onTextChanged =
        [](auto a, auto b) {};

    std::unique_ptr<juce::CodeEditorComponent> scl;
    std::unique_ptr<juce::CodeEditorComponent> kbm;
    TuningOverlay *overlay{nullptr};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SCLKBMDisplay);
};

struct TuningControlArea : public juce::Component,
                           public Surge::GUI::SkinConsumingComponent,
                           public Surge::GUI::IComponentTagValue::Listener
{
    enum tags
    {
        tag_select_tab = 0x475200,
        tag_select_interval,
        tag_export_html,
        tag_apply_sclkbm,
        tag_open_library
    };
    TuningOverlay *overlay{nullptr};
    TuningControlArea(TuningOverlay *ol) : overlay(ol) {}

    void resized() override
    {
        if (skin)
            rebuild();
    }

    void rebuild()
    {
        int labelHeight = 12;
        int buttonHeight = 14;
        int numfieldHeight = 12;
        int margin = 2;
        int xpos = 10;

        removeAllChildren();
        auto h = getHeight();

        {
            int marginPos = xpos + margin;
            int btnWidth = 130;
            int ypos = 1 + labelHeight + margin;

            selectL = newL("Editor Mode");
            selectL->setBounds(xpos, 1, 100, labelHeight);
            addAndMakeVisible(*selectL);

            selectS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);

            selectS->setBounds(btnrect);
            selectS->setStorage(overlay->storage);
            selectS->setLabels({"SCL/KBM", "Radial", "Interval"});
            selectS->addListener(this);
            selectS->setTag(tag_select_tab);
            selectS->setHeightOfOneImage(buttonHeight);
            selectS->setRows(1);
            selectS->setColumns(3);
            selectS->setDraggable(true);
            selectS->setSkin(skin, associatedBitmapStore);
            addAndMakeVisible(*selectS);
            xpos += btnWidth + 10;
        }

        {
            int marginPos = xpos + margin;
            int btnWidth = 130;
            int ypos = 1 + labelHeight + margin;

            intervalL = newL("Interval Display");
            intervalL->setBounds(xpos, 1, 100, labelHeight);
            addAndMakeVisible(*intervalL);

            intervalS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);

            intervalS->setBounds(btnrect);
            intervalS->setStorage(overlay->storage);
            intervalS->setLabels({"Absolute", "To Even"});
            intervalS->addListener(this);
            intervalS->setTag(tag_select_interval);
            intervalS->setHeightOfOneImage(buttonHeight);
            intervalS->setRows(1);
            intervalS->setColumns(2);
            intervalS->setDraggable(true);
            intervalS->setSkin(skin, associatedBitmapStore);
            addAndMakeVisible(*intervalS);
            xpos += btnWidth + 10;
        }

        {
            int marginPos = xpos + margin;
            int btnWidth = 65;
            int ypos = 1 + labelHeight + margin;

            actionL = newL("Actions");
            actionL->setBounds(xpos, 1, 100, labelHeight);
            addAndMakeVisible(*actionL);

            auto ma = [&](const std::string &l, tags t) {
                auto res = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
                auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);

                res->setBounds(btnrect);
                res->setStorage(overlay->storage);
                res->setLabels({l});
                res->addListener(this);
                res->setTag(t);
                res->setHeightOfOneImage(buttonHeight);
                res->setRows(1);
                res->setColumns(1);
                res->setDraggable(false);
                res->setSkin(skin, associatedBitmapStore);
                res->setValue(0);
                return res;
            };

            exportS = ma("Export HTML", tag_export_html);
            addAndMakeVisible(*exportS);
            marginPos += btnWidth + 5;

            libraryS = ma("Tuning Library", tag_open_library);
            addAndMakeVisible(*libraryS);
            marginPos += btnWidth + 5;

            applyS = ma("Apply SCL/KBM", tag_apply_sclkbm);
            addAndMakeVisible(*applyS);
            applyS->setEnabled(false);
            xpos += btnWidth + 5;
        }
    }

    std::unique_ptr<juce::Label> newL(const std::string &s)
    {
        auto res = std::make_unique<juce::Label>(s, s);
        res->setText(s, juce::dontSendNotification);
        res->setFont(Surge::GUI::getFontManager()->getLatoAtSize(9, juce::Font::bold));
        res->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
        return res;
    }

    void valueChanged(GUI::IComponentTagValue *c) override
    {
        auto tag = (tags)(c->getTag());
        switch (tag)
        {
        case tag_select_tab:
        {
            int m = c->getValue() * 2;
            overlay->showEditor(m);
        }
        break;
        case tag_select_interval:
        {
            auto v = c->getValue();
            if (v < 0.5)
            {
                overlay->intervalMatrix->intervalPainter->mode =
                    IntervalMatrix::IntervalPainter::INTERV;
            }
            else
            {
                overlay->intervalMatrix->intervalPainter->mode =
                    IntervalMatrix::IntervalPainter::DIST;
            }
            overlay->intervalMatrix->intervalPainter->repaint();
        }
        break;
        case tag_open_library:
        {
            auto path = overlay->storage->datapath / "tuning_library";
            Surge::GUI::openFileOrFolder(path);
        }
        break;
        case tag_export_html:
        {
            if (overlay && overlay->editor)
            {
                overlay->editor->showHTML(overlay->editor->tuningToHtml());
            }
        }
        break;
        case tag_apply_sclkbm:
        {
            if (applyS->isEnabled())
            {
                auto *sck = overlay->sclKbmDisplay.get();
                sck->onTextChanged(sck->sclDocument->getAllContent().toStdString(),
                                   sck->kbmDocument->getAllContent().toStdString());
                applyS->setEnabled(false);
                applyS->repaint();
            }
        }
        break;
        }
    }

    std::unique_ptr<juce::Label> selectL, intervalL, actionL;
    std::unique_ptr<Surge::Widgets::MultiSwitchSelfDraw> selectS, intervalS, exportS, libraryS,
        applyS;

    void paint(juce::Graphics &g) override { g.fillAll(skin->getColor(Colors::MSEGEditor::Panel)); }

    void onSkinChanged() override { rebuild(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningControlArea);
};

void SCLKBMDisplay::setApplyEnabled(bool b)
{
    overlay->controlArea->applyS->setEnabled(b);
    overlay->controlArea->applyS->repaint();
}

TuningOverlay::TuningOverlay()
{
    tuning = Tunings::Tuning(Tunings::evenTemperament12NoteScale(),
                             Tunings::startScaleOnAndTuneNoteTo(60, 60, Tunings::MIDI_0_FREQ * 32));
    tuningKeyboardTableModel = std::make_unique<TuningTableListBoxModel>();
    tuningKeyboardTableModel->tuningUpdated(tuning);
    tuningKeyboardTable =
        std::make_unique<juce::TableListBox>("Tuning", tuningKeyboardTableModel.get());
    tuningKeyboardTableModel->setTableListBox(tuningKeyboardTable.get());
    tuningKeyboardTableModel->setupDefaultHeaders(tuningKeyboardTable.get());
    addAndMakeVisible(*tuningKeyboardTable);

    tuningKeyboardTable->getViewport()->setScrollBarsShown(true, false);
    tuningKeyboardTable->getViewport()->setViewPositionProportionately(0.0, 48.0 / 127.0);

    sclKbmDisplay = std::make_unique<SCLKBMDisplay>(this);
    sclKbmDisplay->onTextChanged = [this](const std::string &s, const std::string &k) {
        this->onNewSCLKBM(s, k);
    };

    radialScaleGraph = std::make_unique<RadialScaleGraph>();
    radialScaleGraph->onToneChanged = [this](int note, double d) { this->onToneChanged(note, d); };
    radialScaleGraph->onToneStringChanged = [this](int note, const std::string &d) {
        this->onToneStringChanged(note, d);
    };

    intervalMatrix = std::make_unique<IntervalMatrix>(this);

    controlArea = std::make_unique<TuningControlArea>(this);
    addAndMakeVisible(*controlArea);

    addChildComponent(*sclKbmDisplay);
    sclKbmDisplay->setVisible(true);

    addChildComponent(*radialScaleGraph);
    radialScaleGraph->setVisible(false);

    addChildComponent(*intervalMatrix);
    intervalMatrix->setVisible(false);
}

TuningOverlay::~TuningOverlay() = default;

void TuningOverlay::resized()
{
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();

    int kbWidth = 120;
    int ctrlHeight = 35;

    t.transformPoint(w, h);

    tuningKeyboardTable->setBounds(0, 0, kbWidth, h);

    auto contentArea = juce::Rectangle<int>(kbWidth, 0, w - kbWidth, h - ctrlHeight);

    sclKbmDisplay->setBounds(contentArea);
    radialScaleGraph->setBounds(contentArea);
    intervalMatrix->setBounds(contentArea);
    controlArea->setBounds(kbWidth, h - ctrlHeight, w - kbWidth, ctrlHeight);

    // it's a bit of a hack to put this here but by this [oint i'm all set up
    if (storage)
    {
        auto mcoff = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::MiddleC, 1);
        tuningKeyboardTableModel->setMiddleCOff(mcoff);
    }
}

void TuningOverlay::showEditor(int which)
{
    jassert(which >= 0 && which <= 2);
    sclKbmDisplay->setVisible(which == 0);
    radialScaleGraph->setVisible(which == 1);
    intervalMatrix->setVisible(which == 2);
}

void TuningOverlay::onToneChanged(int tone, double newCentsValue)
{
    if (storage)
    {
        storage->currentScale.tones[tone].type = Tunings::Tone::kToneCents;
        storage->currentScale.tones[tone].cents = newCentsValue;
        recalculateScaleText();
    }
}

void TuningOverlay::onToneStringChanged(int tone, const std::string &newStringValue)
{
    if (storage)
    {
        try
        {
            auto parsed = Tunings::toneFromString(newStringValue);
            storage->currentScale.tones[tone] = parsed;
            recalculateScaleText();
        }
        catch (Tunings::TuningError &e)
        {
            storage->reportError(e.what(), "Tuning Tone Conversion");
        }
    }
}

void TuningOverlay::onNewSCLKBM(const std::string &scl, const std::string &kbm)
{
    if (!storage)
        return;

    try
    {
        auto s = Tunings::parseSCLData(scl);
        auto k = Tunings::parseKBMData(kbm);
        storage->retuneAndRemapToScaleAndMapping(s, k);
        setTuning(storage->currentTuning);
    }
    catch (const Tunings::TuningError &e)
    {
        storage->reportError(e.what(), "Error Applying Tuning");
    }
}

void TuningOverlay::setMidiOnKeys(const std::bitset<128> &keys)
{
    tuningKeyboardTableModel->notesOn = keys;
    tuningKeyboardTable->repaint();
    radialScaleGraph->setNotesOn(keys);
    intervalMatrix->setNotesOn(keys);
}

void TuningOverlay::recalculateScaleText()
{
    std::ostringstream oss;
    oss << "! Scale generated by tuning editor\n"
        << storage->currentScale.description << "\n"
        << storage->currentScale.count << "\n"
        << "! \n";
    for (int i = 0; i < storage->currentScale.count; ++i)
    {
        auto tn = storage->currentScale.tones[i];
        if (tn.type == Tunings::Tone::kToneRatio)
        {
            oss << tn.ratio_n << "/" << tn.ratio_d << "\n";
        }
        else
        {
            oss << std::fixed << std::setprecision(5) << tn.cents << "\n";
        }
    }

    try
    {
        storage->retuneToScale(Tunings::parseSCLData(oss.str()));
        setTuning(storage->currentTuning);
    }
    catch (const Tunings::TuningError &e)
    {
    }
}

void TuningOverlay::setTuning(const Tunings::Tuning &t)
{
    tuning = t;
    tuningKeyboardTableModel->tuningUpdated(tuning);
    sclKbmDisplay->setTuning(t);
    radialScaleGraph->setTuning(t);
    intervalMatrix->setTuning(t);
    repaint();
}

void TuningOverlay::onSkinChanged()
{
    tuningKeyboardTableModel->setSkin(skin, associatedBitmapStore);
    tuningKeyboardTable->repaint();

    sclKbmDisplay->setSkin(skin, associatedBitmapStore);
    radialScaleGraph->setSkin(skin, associatedBitmapStore);
    intervalMatrix->setSkin(skin, associatedBitmapStore);

    controlArea->setSkin(skin, associatedBitmapStore);
}

void TuningOverlay::onTearOutChanged(bool isTornOut) { doDnD = isTornOut; }
bool TuningOverlay::isInterestedInFileDrag(const juce::StringArray &files)
{
    if (!doDnD)
        return false;
    if (editor)
        return editor->juceEditor->isInterestedInFileDrag(files);
    return false;
}
void TuningOverlay::filesDropped(const juce::StringArray &files, int x, int y)
{
    if (!doDnD)
        return;
    if (editor)
        editor->juceEditor->filesDropped(files, x, y);
}

} // namespace Overlays
} // namespace Surge