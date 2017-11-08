
#pragma once

#include "ElementApp.h"
#include "gui/LookAndFeel.h"

namespace Element {

class SettingButton : public Button
{
public:
    SettingButton (const String& name = String()) : Button (name) { }
    ~SettingButton() { }
    
    enum ColourIds {
        textColourId  =  0x90000000,
    };
    
protected:
    virtual Colour getTextColour()
    {
        findColour (textColourId);
        return Colours::black;
    }
    
    /** @internal */
    void paintButton (Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
};

class PanicButton : public SettingButton {
public:
    PanicButton() { setButtonText ("!"); }
};
}