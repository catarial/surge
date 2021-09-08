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

#ifndef SURGE_XT_SURGEJUCELOOKANDFEEL_H
#define SURGE_XT_SURGEJUCELOOKANDFEEL_H

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeJUCELookAndFeel : public juce::LookAndFeel_V4
{
  public:
    void drawLabel(juce::Graphics &graphics, juce::Label &label) override;
    void drawTextEditorOutline(juce::Graphics &graphics, int width, int height,
                               juce::TextEditor &editor) override;

    enum SurgeColourIds
    {
        componentBgStart = 0x3700001,
        componentBgEnd,

        vkbTextLabelId,
        vkbShadowId,
        vkbBlackKeyId,
        vkbWhiteKeyId,
        vkbKeySeparatorId,
        vkbMouseOverKeyOverlayId,
        vkbKeyDownOverlayId,
        vkbOctaveJogBackgroundId,
        vkbOctaveJogArrowId,

        tempoBackgroundId,
        tempoLabelId,

        tempoTypeinBackgroundId,
        tempoTypeinBorderId,
        tempoTypeinHighlightId,
        tempoTypeinTextId,
    };
};
#endif // SURGE_XT_SURGEJUCELOOKANDFEEL_H
