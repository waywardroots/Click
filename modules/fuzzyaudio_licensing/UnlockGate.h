#pragma once

namespace fuzzyaudio
{
class UnlockGate : public juce::Component
{
public:
    explicit UnlockGate (LicenseManager& licenseToUse);
    std::function<void()> onUnlockSucceeded;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void tryUnlock();
    LicenseManager& license;
    juce::Label    titleLabel, infoLabel, keyCaption, statusLabel;
    juce::TextEditor keyEntry;
    juce::TextButton unlockButton { "Unlock" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UnlockGate)
};
} // namespace fuzzyaudio
