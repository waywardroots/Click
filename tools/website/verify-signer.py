#!/usr/bin/env python3
"""Check the website signers against JUCE's verification maths.

    python3 tools/website/verify-signer.py

Makes a throwaway RSA pair in JUCE's "part1,part2" hex form, signs a key with
each signer that can run here, then undoes the transform with the public half
and applies the same four tests juce::OnlineUnlockStatus::applyKeyFile does.
Proves a signer end to end without building the plug-in.

Needs node; the TypeScript signer additionally needs tsc, and the PHP one needs
php with the gmp extension. Signers whose runtime is missing are skipped.
"""

import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

E = 5                      # juce::RSAKey::createKeyPair's public exponent
APP = "Click"              # must equal the plug-in's productId
TOKEN = "click-license-v1" # must equal the plug-in's activationToken
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))


def is_prime(n, rounds=40):
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


def recover_payload(key_file_text, public_exponent, modulus):
    """What the plug-in does: everything after the last '#', transformed back.

    BigInteger::parseString skips any character that is not a hex digit, which
    is why the 70-column wrapping in the key file is harmless.
    """
    hex_digits = re.sub(r"[^0-9a-fA-F]", "", key_file_text[key_file_text.rindex("#") + 1:])
    value = apply_to_value(int(hex_digits, 16), public_exponent, modulus)
    return hex_digits, value.to_bytes((value.bit_length() + 7) // 8, "little").decode("utf-8")


def juce_checks(xml, email, name):
    """The tests applyKeyFile actually applies, in its order."""
    attr = lambda n: (re.search(rf'{n}="([^"]*)"', xml) or [None, ""])[1]

    # machineNumberAllowed splits the attribute on ",; " and compares
    # case-insensitively after trimming.
    machines = [m.strip() for m in re.split(r"[,; ]", attr("mach")) if m.strip()]

    return {
        "user and email non-empty": attr("user") != "" and attr("email") != "",
        "user survived the round trip": attr("user") == name,
        "email survived the round trip": attr("email") == email,
        f'app matches productId "{APP}"': attr("app") == APP,
        f'mach matches token "{TOKEN}"': any(m.lower() == TOKEN.lower() for m in machines),
        "token is one token, not several": machines == [TOKEN],
        "date is lowercase hex": re.fullmatch(r"[0-9a-f]+", attr("date")) is not None,
    }


def node_env():
    env = dict(os.environ)
    env["LICENSE_SIGNING_KEY"] = PRIVATE
    return env


def sign_with_mjs(email, name):
    return subprocess.run(["node", os.path.join(HERE, "generate-licence-keys.mjs"),
                           APP, email, TOKEN, name],
                          capture_output=True, check=True, env=node_env()).stdout


def sign_with_ts(email, name):
    """Compile the TypeScript signer and drive it through the same interface."""
    out = tempfile.mkdtemp()
    subprocess.run(["tsc", "--target", "es2020", "--module", "es2022",
                    "--moduleResolution", "bundler", "--outDir", out,
                    os.path.join(HERE, "generate-licence-keys.ts")], check=True)

    driver = os.path.join(out, "driver.mjs")
    with open(driver, "w") as f:
        f.write(
            'import { generateKeyFile } from "./generate-licence-keys.js";\n'
            'const [,, app, email, machine, name] = process.argv;\n'
            'process.stdout.write(generateKeyFile({ app, email, machine, name },'
            ' process.env.LICENSE_SIGNING_KEY));\n')

    return subprocess.run(["node", driver, APP, email, TOKEN, name],
                          capture_output=True, check=True, env=node_env()).stdout


def php_has_gmp():
    if shutil.which("php") is None:
        return False
    return subprocess.run(["php", "-r", 'exit(extension_loaded("gmp") ? 0 : 1);']).returncode == 0


def sign_with_php(email, name):
    return subprocess.run(["php", os.path.join(HERE, "sign_license.php"),
                           APP, email, name, TOKEN, PRIVATE],
                          capture_output=True, check=True).stdout


# A throwaway pair: this verifies the signers, it is not a product key.
p, q = prime(512), prime(512)
MODULUS = p * q
PRIVATE = f"{pow(E, -1, (p - 1) * (q - 1)):x},{MODULUS:x}"

# Short details fit one modulus-sized block, long details take two. Both are
# correct, so both are exercised.
CASES = [("short details", "b@e.co", "Jo"),
         ("long details", "a-rather-longer-buyer@somewhere-in-particular.example",
          "Alexandra Featherstonehaugh")]

SIGNERS = [("generate-licence-keys.mjs", sign_with_mjs)]

if shutil.which("tsc") is not None:
    SIGNERS.append(("generate-licence-keys.ts", sign_with_ts))
else:
    print("skipping generate-licence-keys.ts: tsc not installed")

if php_has_gmp():
    SIGNERS.append(("sign_license.php", sign_with_php))
else:
    print("skipping sign_license.php: needs php with the gmp extension")

failures = 0

for signer, sign in SIGNERS:
    for label, email, name in CASES:
        # Bytes, not text: universal newlines would hide the CRLF endings.
        raw = sign(email, name)
        text = raw.decode()
        hex_digits, xml = recover_payload(text, E, MODULUS)

        results = {
            "no cleartext XML in the file": "<key" not in text,
            "CRLF line endings": b"\r\n" in raw,
            "hex wraps at 70 columns": all(len(l) <= 70 for l in text.splitlines()),
            "payload is one flat <key/> element": re.fullmatch(r"<key [^>]*/>", xml) is not None,
            **juce_checks(xml, email, name),
        }

        print(f"\n{signer}  [{label}]  payload={len(xml.encode())}B  hex={len(hex_digits)}")
        print(f"  {xml}")

        for name_, ok in results.items():
            print(f"  {'PASS' if ok else 'FAIL'}  {name_}")
            failures += not ok

print(f"\n{'all checks passed' if failures == 0 else str(failures) + ' CHECKS FAILED'}")
sys.exit(1 if failures else 0)
