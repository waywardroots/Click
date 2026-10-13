#pragma once

namespace fuzzyaudio
{
class LicenseManager : public juce::OnlineUnlockStatus
{
public:
    struct Config
    {
        juce::String productId;                 // must match the key file's "app"
        juce::String publicKey;                 // RSA public key "part1,part2"
        juce::String vendorName  { "Fuzzy Audio" };
        juce::String appName;                   // settings file name (defaults to productId)
        juce::String websiteName { "fuzzyaudio.example" };
        juce::URL    websiteURL  { juce::URL ("https://fuzzyaudio.example") };
        // Paste-only: every key is signed with this fixed token, so a key unlocks
        // any machine. Set empty for real per-machine locking.
        juce::String activationToken { "license" };
    };

    explicit LicenseManager (Config configToUse);

    bool isActivated();
    juce::String getThisMachineID();
    juce::Result applyKey (const juce::String& keyFileText);
    const Config& getConfig() const noexcept { return config; }

    juce::String getProductID() override                            { return config.productId; }
    bool doesProductIDMatch (const juce::String& returned) override { return returned == config.productId; }
    juce::RSAKey getPublicKey() override                            { return juce::RSAKey (config.publicKey); }
    juce::StringArray getLocalMachineIDs() override;
    void saveState (const juce::String& state) override;
    juce::String getState() override;
    juce::String getWebsiteName() override                          { return config.websiteName; }
    juce::URL getServerAuthenticationURL() override                 { return config.websiteURL; }
    juce::String readReplyFromWebserver (const juce::String&, const juce::String&) override { return {}; }

private:
    juce::PropertiesFile* settings();
    Config config;
    juce::ApplicationProperties appProperties;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseManager)
};
} // namespace fuzzyaudio
