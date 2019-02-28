
#pragma once

#include "JuceHeader.h"

namespace Element {

struct Artist
{
    static void drawVerticalText (Graphics& g, const String& text,
                                  const Rectangle<int> area,
                                  Justification justification = Justification::centredLeft)
    {
        auto r = area; 
        Graphics::ScopedSaveState savestate (g);
        g.setOrigin (r.getX(), r.getY());
        if (justification == Justification::left ||
            justification == Justification::centredLeft ||
            justification == Justification::topLeft ||
            justification == Justification::bottomLeft)
        {
            g.addTransform (AffineTransform().rotated (
                float_Pi / 2.0f, 0.0f, 0.0f));
            g.drawText (text,
                0, -r.getWidth(), r.getHeight(), r.getWidth(),
                justification, false);
        }
        else if (justification == Justification::right ||
            justification == Justification::centredRight ||
            justification == Justification::topRight ||
            justification == Justification::bottomRight)
        {
            g.addTransform (AffineTransform().rotated (
                -float_Pi / 2.0f, 0.0f, (float)r.getHeight()));
            g.drawText (text,
                0, r.getHeight(), r.getHeight(), r.getWidth(),
                justification, false);
        }
        else
        {
            jassertfalse; // mode not supported
        }
    }
};

}