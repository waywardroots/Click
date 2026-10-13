export interface LicenceInfo { app: string; email: string; name?: string; machine: string; }

function modPow(base: bigint, exp: bigint, m: bigint): bigint {
  let r = 1n; base %= m;
  while (exp > 0n) { if (exp & 1n) r = (r * base) % m; exp >>= 1n; base = (base * base) % m; }
  return r;
}
function applyToValue(value: bigint, part1: bigint, part2: bigint): bigint {
  let result = 0n;
  while (value !== 0n) { result *= part2; const rem = value % part2; value = value / part2; result += modPow(rem, part1, part2); }
  return result;
}
function bytesToBigIntLE(bytes: Uint8Array): bigint {
  let v = 0n; for (let i = bytes.length - 1; i >= 0; i--) v = (v << 8n) | BigInt(bytes[i]); return v;
}
function xmlAttr(s: string): string {
  return s.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");
}
export function generateKeyFile(info: LicenceInfo, privateKey: string): string {
  const comma = privateKey.indexOf(",");
  if (comma < 0) throw new Error('Invalid JUCE private key (expected "<d_hex>,<n_hex>").');
  const d = BigInt("0x" + privateKey.slice(0, comma).trim());
  const n = BigInt("0x" + privateKey.slice(comma + 1).trim());
  const name = (info.name && info.name.length > 0) ? info.name : info.email;
  // The plug-in rejects a key whose user or email is empty ("Credentials are
  // invalid"), and the comment lines still look plausible, so refuse to sign
  // one rather than hand the buyer a key that cannot work.
  if (!name || !info.email) throw new Error("user/name and email must be non-empty.");
  const xml =
    `<key user="${xmlAttr(name)}" email="${xmlAttr(info.email)}" mach="${xmlAttr(info.machine)}" app="${xmlAttr(info.app)}" date="${Date.now().toString(16)}"/>`;
  const hex = applyToValue(bytesToBigIntLE(new TextEncoder().encode(xml)), d, n).toString(16);
  const lines = [`Keyfile for ${info.app}`, `User: ${name}`, `Email: ${info.email}`,
                 `Machine numbers: ${info.machine}`, `Created: ${new Date().toUTCString()}`, ""];
  const body = "#" + hex;
  for (let i = 0; i < body.length; i += 70) lines.push(body.slice(i, i + 70));
  lines.push("");
  return lines.join("\r\n") + "\r\n";
}
