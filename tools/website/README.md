# License-key generation spec (for the website / AI builder)

This describes **exactly** how to generate Click license keys so the plug-in
accepts them. The plug-in uses JUCE's `OnlineUnlockStatus`, which is **not** normal
RSA signing. Follow this literally.

Ready-to-use reference implementations live next to this file:
- `generate-licence-keys.mjs` (plain Node, run directly) — **verified** to unlock.
- `generate-licence-keys.ts` (TypeScript) — same algorithm.
- `sign_license.php` (PHP + gmp) — same algorithm.

`verify-signer.py` drives each of them through a throwaway key pair and applies
the same tests `applyKeyFile` does, so a signer can be proved without building
the plug-in:

```
$ python3 tools/website/verify-signer.py
generate-licence-keys.mjs  [short details]  payload=86B   hex=256
generate-licence-keys.mjs  [long details]   payload=158B  hex=512
generate-licence-keys.ts   [short details]  payload=86B   hex=256
generate-licence-keys.ts   [long details]   payload=158B  hex=511
all checks passed
```

(It skips any signer whose runtime is missing — the PHP one needs `php-gmp`.)

`inspect-key.py` goes the other way, for when a buyer reports a key that will
not unlock. Give it the product's public key and the key file and it prints the
payload the plug-in would recover, then says which of JUCE's checks the key
fails:

```
$ python3 tools/website/inspect-key.py "5,<modulus>" key.txt
payload : <key user="Jane Doe" email="buyer@example.com" mach="click-license-v1" app="Click" date="1a1373fddd5"/>
  PASS  user and email are non-empty
  PASS  app is "Click"
  PASS  mach carries "click-license-v1"
```

It distinguishes the three ways a key goes wrong: wrong product name, wrong
activation token, and signed with a pair whose public half is not the one in
the plug-in — the last shows up as a payload that is not valid UTF-8 at all.

## DO NOT (these are the mistakes that make keys fail)
1. **Do NOT hash** the payload (no SHA-1/SHA-256). JUCE signs the *entire XML*.
2. **Do NOT** emit standard RSA / PEM signatures. The key is JUCE's `"<d_hex>,<n_hex>"`.
3. **Do NOT** put the XML in the file as clear text. The file is `comment + "#" + hex`.
4. **Do NOT** use nested `<user>`/`<product>` elements. Use one flat `<key .../>`.
5. **Do NOT** leave `user` or `email` empty — the plug-in rejects those.

## Inputs
- `LICENSE_SIGNING_KEY` = `"<d_hex>,<n_hex>"` — the private key (server-side secret).
- `app` = `"Click"` (must match the plug-in's product id).
- `email` = buyer's email (non-empty, varies per purchase).
- `name` = buyer's name or email (non-empty).
- `machine` = `"click-license-v1"` — the fixed activation token (paste-only).

> **The token may not contain a comma, semicolon or space.** JUCE splits the
> `mach` attribute on `,;` and space before comparing, so a token containing any
> of them would be read as several tokens and the comparison would not be the one
> you intended. `click-license-v1` is safe.

## What the plug-in actually checks
From `juce::OnlineUnlockStatus::applyKeyFile`, in order — these are the only
things that can reject a well-formed key:

1. `user` **and** `email` non-empty, else *"Credentials are invalid"*.
2. `app` equals the plug-in's product id, else *"Wrong product ID"*.
3. the machine's unique ID is readable (always true on a normal desktop).
4. `mach` matches one of the plug-in's local machine IDs — which, in paste-only
   mode, is the single activation token. The comparison is case-insensitive and
   trimmed.

`date` is only used by *expiring* keys, which these are not; it is otherwise
ignored. The hex may be wrapped across lines: JUCE skips every non-hex character
while parsing, which is why the 70-character wrapping is safe.

## Algorithm (must match byte-for-byte)
1. Build a **single-line** XML string, attributes on the root `<key>` element:
   ```
   <key user="NAME" email="EMAIL" mach="TOKEN" app="APP" date="HEXMILLIS"/>
   ```
   - `date` = current time in ms as lowercase hex (`Date.now().toString(16)`).
   - XML-escape `& < > "` in the values.
2. Encode that string as **UTF-8 bytes**.
3. Convert the bytes to a BigInteger **little-endian** (first byte = least significant).
4. Apply JUCE's RSA transform with the private key (`d`, `n`) — **block-wise in base n**:
   ```
   result = 0
   while value != 0:
       result  = result * n
       rem     = value mod n
       value   = value div n          # integer division
       result += (rem ** d) mod n     # modular exponentiation
   ```
   (Because the XML is usually larger than the 1024-bit modulus, this loop runs
   ~2 times and the hex output is ~**512** characters — not 256.)
5. `hex = result` in lowercase base-16.
6. Build the final key file text (CRLF line endings):
   ```
   Keyfile for APP
   User: NAME
   Email: EMAIL
   Machine numbers: TOKEN
   Created: <any date string>
   <blank line>
   #<hex, wrapped at 70 chars per line>
   <blank line>
   ```
   Only the part after the last `#` matters to the plug-in; the comment lines are
   cosmetic. Email the whole block to the buyer.

## Minimal reference (Node, no dependencies)
```js
function modPow(b, e, m){let r=1n;b%=m;while(e>0n){if(e&1n)r=r*b%m;e>>=1n;b=b*b%m;}return r;}
function applyToValue(v, d, n){let r=0n;while(v!==0n){r*=n;const q=v%n;v=v/n;r+=modPow(q,d,n);}return r;}
function bytesToBigIntLE(a){let v=0n;for(let i=a.length-1;i>=0;i--)v=(v<<8n)|BigInt(a[i]);return v;}
const esc=s=>String(s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");

export function generateKeyFile({app,email,name,machine}, privateKey){
  const i=privateKey.indexOf(","); const d=BigInt("0x"+privateKey.slice(0,i).trim()), n=BigInt("0x"+privateKey.slice(i+1).trim());
  const user=name||email; if(!user||!email) throw new Error("user/email required");
  const xml=`<key user="${esc(user)}" email="${esc(email)}" mach="${esc(machine)}" app="${esc(app)}" date="${Date.now().toString(16)}"/>`;
  const hex=applyToValue(bytesToBigIntLE(new TextEncoder().encode(xml)), d, n).toString(16);
  const lines=[`Keyfile for ${app}`,`User: ${user}`,`Email: ${email}`,`Machine numbers: ${machine}`,`Created: ${new Date().toUTCString()}`,""];
  const body="#"+hex; for(let k=0;k<body.length;k+=70) lines.push(body.slice(k,k+70)); lines.push("");
  return lines.join("\r\n")+"\r\n";
}
```

## Self-test (prove it before shipping)
A correct key for `app="Click"`, `machine="click-license-v1"`, a
non-empty email, signed with your private key:
- contains **no** cleartext `<key>` XML,
- has a `#` followed by hex that decodes back to the flat `<key .../>` payload,
- unlocks the plug-in when pasted into its Unlock screen.

**On the hex length.** With a 1024-bit key each block is ~256 hex characters, and
the transform runs once per modulus-sized block. `Click` is a short product name,
so a buyer with short details produces a payload under 128 bytes and a key of
~256 hex characters; a longer name or email pushes it into a second block and
~512. Both are correct — verified here by signing with the Node signer and
undoing the transform with the public half:

```
short name/email  payload= 86B  hex=255  CRLF=yes  round-trips=yes
long  name/email  payload=158B  hex=512  CRLF=yes  round-trips=yes
```

So judge a key by the payload it decodes to, not by its length. What the length
*does* tell you: a key that is exactly 256 hex characters **and** decodes to
nothing readable is the old hashed code path, which never unlocks.

**Comparing against `keygen sign`.** The C++ generator serialises the XML with
JUCE's own writer, which prepends `<?xml version="1.0" encoding="UTF-8"?>` to the
payload; the signers here emit the bare `<key .../>` element. Both verify, because
the plug-in parses whatever it decrypts. So a byte-for-byte diff between a keygen
key and a website key will differ in length and content even when both are
correct — compare the *decoded* attributes instead.

If your output still looks like `<key><user .../><product .../></key>` + 256 hex
chars, the old code path is still running — replace it with the reference above.
