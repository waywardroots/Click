/*
  BEGIN_JUCE_MODULE_DECLARATION
    ID:                 fuzzyaudio_licensing
    vendor:             FuzzyAudio
    version:            1.0.0
    name:               Fuzzy Audio licensing
    description:        Offline license-key unlocking for JUCE plug-ins.
    license:            Proprietary
    minimumCppStandard: 17
    dependencies:       juce_product_unlocking juce_gui_extra
  END_JUCE_MODULE_DECLARATION
*/
#pragma once
#define FUZZYAUDIO_LICENSING_H_INCLUDED 1

#include <juce_product_unlocking/juce_product_unlocking.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "LicenseManager.h"
#include "UnlockGate.h"
