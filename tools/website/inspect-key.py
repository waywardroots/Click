#!/usr/bin/env python3
"""Decode a license key the way the plug-in does, to see why one was rejected.

    python3 tools/website/inspect-key.py <publicKey> <keyFile>

<publicKey> is the "public:" line from `keygen makepair` - "part1,part2" in
hex. It only verifies, so it is safe to hand around; never pass the private
half to this or anything else off your server.

Prints the payload recovered from the key and applies the four tests
juce::OnlineUnlockStatus::applyKeyFile applies, so a rejected key can be told
apart from a wrongly configured plug-in without a debugger.
"""

import re
import sys

PRODUCT_ID = "Click"              # LicenseManager::Config::productId
ACTIVATION_TOKEN = "click-license-v1"   # LicenseManager::Config::activationToken


def apply_to_value(value, part1, part2):
    """juce::RSAKey::applyToValue - block-wise in base part2."""
    result = 0
    while value != 0:
        result *= part2
        value, rem = divmod(value, part2)
        result += pow(rem, part1, part2)
    return result


def main(public_key, key_file):
    if "," not in public_key:
        sys.exit('public key must look like "part1,part2" in hex')

    part1, part2 = (int(h.strip(), 16) for h in public_key.split(",", 1))

    with open(key_file, encoding="utf-8") as f:
        text = f.read()

    if "#" not in text:
        sys.exit("that file has no '#': the plug-in reads only what follows the last one")

    # BigInteger::parseString ignores every character that is not a hex digit,
    # which is why the key file can wrap the hex across lines.
    hex_digits = re.sub(r"[^0-9a-fA-F]", "", text[text.rindex("#") + 1:])
    value = apply_to_value(int(hex_digits, 16), part1, part2)
    raw = value.to_bytes((value.bit_length() + 7) // 8, "little")

    print(f"hex digits : {len(hex_digits)}")

    try:
        payload = raw.decode("utf-8")
    except UnicodeDecodeError:
        print("payload    : not valid UTF-8\n")
        sys.exit("The key does not decode with this public key. Either the key was signed\n"
                 "with a different pair's private half, or this is not its public half.")

    print(f"payload    : {payload}\n")

    attr = lambda n: (re.search(rf'{n}="([^"]*)"', payload) or [None, ""])[1]
    machines = [m.strip() for m in re.split(r"[,; ]", attr("mach")) if m.strip()]

    checks = [
        ("user and email are non-empty",
         attr("user") != "" and attr("email") != "",
         'rejected as "Credentials are invalid"'),
        (f'app is "{PRODUCT_ID}"',
         attr("app") == PRODUCT_ID,
         f'rejected as "Wrong product ID" - the key says app="{attr("app")}"'),
        (f'mach carries "{ACTIVATION_TOKEN}"',
         any(m.lower() == ACTIVATION_TOKEN.lower() for m in machines),
         f'the unlock fails - the key says mach="{attr("mach")}"'),
    ]

    failed = False

    for label, ok, consequence in checks:
        print(f"  {'PASS' if ok else 'FAIL'}  {label}")
        if not ok:
            print(f"        -> {consequence}")
            failed = True

    print("\n" + ("This key would be rejected." if failed else
                  "This key unlocks a plug-in built with that public key."))
    return 1 if failed else 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1], sys.argv[2]))
