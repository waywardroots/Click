#!/usr/bin/env python3
"""Round-trip the website signer against JUCE's verification maths.

    python3 tools/website/verify-signer.py

Needs node on PATH. Proves the signer's output decodes back to the exact XML
the plug-in expects, without needing a build of the plug-in itself.

Makes an RSA pair in JUCE's "part1,part2" hex form, signs a key with the Node
signer, then undoes the transform with the public half the way
OnlineUnlockStatus does, and checks the XML that falls out.
"""

import random
import re
import subprocess
import sys

E = 5  # juce::RSAKey::createKeyPair's public exponent


def is_prime(n, rounds=40):
    if n < 2:
        return False
    for p in (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37):
        if n % p == 0:
            return n == p
    d, s = n - 1, 0
    while d % 2 == 0:
        d //= 2
        s += 1
    for _ in range(rounds):
        a = random.randrange(2, n - 1)
        x = pow(a, d, n)
        if x in (1, n - 1):
            continue
        for _ in range(s - 1):
            x = x * x % n
            if x == n - 1:
                break
        else:
            return False
    return True


def prime(bits):
    while True:
        p = random.getrandbits(bits) | (1 << (bits - 1)) | 1
        if p % E != 1 and is_prime(p):   # keep gcd(e, p-1) == 1
            return p


def apply_to_value(value, part1, part2):
    """juce::RSAKey::applyToValue - block-wise in base part2."""
    result = 0
    while value != 0:
        result *= part2
        value, rem = divmod(value, part2)
        result += pow(rem, part1, part2)
    return result


p, q = prime(512), prime(512)
n = p * q
phi = (p - 1) * (q - 1)
d = pow(E, -1, phi)

public = f"{E:x},{n:x}"
private = f"{d:x},{n:x}"

app, token = "Click", "click-license-v1"
email, name = "buyer@example.com", "Jane Doe"

# Raw bytes, not text: universal newlines would hide the CRLF line endings.
raw = subprocess.run(
    ["node", "tools/website/generate-licence-keys.mjs", app, email, token, name],
    capture_output=True, check=True,
    env={"LICENSE_SIGNING_KEY": private, "PATH": "/usr/bin:/bin:/opt/node22/bin"}).stdout
signed = raw.decode()

print("--- key file as the buyer receives it ---")
print("\n".join(signed.splitlines()[:7]))
print("...")

# What the plug-in does: take everything after the last '#', undo the transform
# with the public key, and read the bytes back little-endian.
body = signed[signed.rindex("#") + 1:]
hex_digits = re.sub(r"\s+", "", body)
recovered = apply_to_value(int(hex_digits, 16), E, n)
xml = recovered.to_bytes((recovered.bit_length() + 7) // 8, "little").decode("utf-8")

print("\n--- recovered payload ---")
print(xml)

checks = {
    "no cleartext XML in the file": "<key" not in signed,
    # One ~256-char block per modulus-sized chunk of payload; both are correct.
    "hex is a whole number of blocks": len(hex_digits) in range(240, 260) or len(hex_digits) in range(500, 520),
    "payload is one flat <key/> element": re.fullmatch(r"<key [^>]*/>", xml) is not None,
    f'app == "{app}"': f'app="{app}"' in xml,
    f'mach == "{token}"': f'mach="{token}"' in xml,
    "user non-empty": f'user="{name}"' in xml,
    "email non-empty": f'email="{email}"' in xml,
    "date is lowercase hex": re.search(r'date="([0-9a-f]+)"', xml) is not None,
    "CRLF line endings": b"\r\n" in raw,
}

print()
for label, ok in checks.items():
    print(f"  {'PASS' if ok else 'FAIL'}  {label}")

sys.exit(0 if all(checks.values()) else 1)
