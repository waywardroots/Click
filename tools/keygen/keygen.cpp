#include <juce_product_unlocking/juce_product_unlocking.h>
#include <iostream>
using namespace juce;

static int doMakePair (int numBits)
{
    RSAKey publicKey, privateKey;
    Random rng (Time::getHighResolutionTicks());
    Array<int> seeds;
    for (int i = 0; i < 64; ++i) seeds.add (rng.nextInt());
    RSAKey::createKeyPair (publicKey, privateKey, numBits,
                           seeds.getRawDataPointer(), seeds.size());
    std::cout << "public:  " << publicKey.toString()  << "\n"
              << "private: " << privateKey.toString() << "\n";
    return 0;
}

static int doSign (const StringArray& a)
{
    // a = { productName, email, machineID, privateKey, [userName] }
    const auto productName = a[0];
    const auto email       = a[1];
    const auto machineID   = a[2];
    const RSAKey privateKey (a[3]);
    const auto userName    = a.size() > 4 && a[4].isNotEmpty() ? a[4] : email;
    if (! a[3].containsChar (','))
    { std::cerr << "error: not a valid RSA private key (expected \"part1,part2\")\n"; return 1; }
    std::cout << KeyGeneration::generateKeyFile (productName, email, userName, machineID, privateKey)
              << std::endl;
    return 0;
}

struct VerifierStatus final : public OnlineUnlockStatus
{
    String productID, publicKeyString, stateString; StringArray machineIDs;
    String getProductID() override                     { return productID; }
    bool doesProductIDMatch (const String& r) override { return r == productID; }
    RSAKey getPublicKey() override                     { return RSAKey (publicKeyString); }
    void saveState (const String& s) override          { stateString = s; }
    String getState() override                         { return stateString; }
    String getWebsiteName() override                   { return {}; }
    URL getServerAuthenticationURL() override          { return URL(); }
    String readReplyFromWebserver (const String&, const String&) override { return {}; }
    StringArray getLocalMachineIDs() override          { return machineIDs; }
};

static int doVerify (const StringArray& a)
{
    VerifierStatus status;
    status.productID = a[0]; status.publicKeyString = a[1]; status.machineIDs.add (a[2]);
    const auto result = status.applyKeyFile (File (a[3]).loadFileAsString());
    std::cout << "applyKeyFile: " << (result.wasOk() ? String ("ok") : result.getErrorMessage()) << "\n"
              << "isUnlocked:   " << (status.isUnlocked() ? "YES" : "no") << "\n";
    return status.isUnlocked() ? 0 : 2;
}

int main (int argc, char* argv[])
{
    StringArray args;
    for (int i = 1; i < argc; ++i) args.add (String::fromUTF8 (argv[i]));
    const auto usage = [] {
        std::cerr << "usage:\n  keygen makepair [numBits=1024]\n"
                  << "  keygen sign   <product> <email> <machineOrToken> <privateKey> [name]\n"
                  << "  keygen verify <product> <publicKey> <machineOrToken> <keyFilePath>\n";
        return 1; };
    if (args.isEmpty()) return usage();
    const auto cmd = args[0]; args.remove (0);
    if (cmd == "makepair") return doMakePair (args.isEmpty() ? 1024 : args[0].getIntValue());
    if (cmd == "sign")     return args.size() >= 4 ? doSign (args)   : usage();
    if (cmd == "verify")   return args.size() == 4 ? doVerify (args) : usage();
    return usage();
}
