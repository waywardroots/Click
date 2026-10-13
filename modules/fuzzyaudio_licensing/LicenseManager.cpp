namespace fuzzyaudio
{
LicenseManager::LicenseManager (Config configToUse)
    : config (std::move (configToUse))
{
    juce::PropertiesFile::Options options;
    options.applicationName     = config.appName.isNotEmpty() ? config.appName : config.productId;
    options.filenameSuffix      = "settings";
    options.folderName          = config.vendorName;
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (options);

    load();   // restore saved activation so we don't re-ask after a restart
}

juce::PropertiesFile* LicenseManager::settings() { return appProperties.getUserSettings(); }

void LicenseManager::saveState (const juce::String& state)
{
    if (auto* s = settings()) { s->setValue ("licenseKey", state); s->saveIfNeeded(); }
}

juce::String LicenseManager::getState()
{
    if (auto* s = settings()) return s->getValue ("licenseKey", {});
    return {};
}

bool LicenseManager::isActivated() { return isUnlocked(); }

juce::String LicenseManager::getThisMachineID()
{
    auto ids = getLocalMachineIDs();
    return ids.isEmpty() ? juce::String() : ids[0];
}

juce::StringArray LicenseManager::getLocalMachineIDs()
{
    if (config.activationToken.isNotEmpty())
        return juce::StringArray (config.activationToken);   // paste-only token
    return juce::OnlineUnlockStatus::getLocalMachineIDs();   // real machine locking
}

juce::Result LicenseManager::applyKey (const juce::String& keyFileText)
{
    auto result = applyKeyFile (keyFileText.trim());
    if (result.wasOk())
        save();   // persist activation
    return result;
}
} // namespace fuzzyaudio
