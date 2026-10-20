"""Pre-build: data/cert/x509_crt_bundle.bin for WiFiClientSecure::setCACertBundle.

Format (ESP-IDF 4.4 esp_crt_bundle): u16 BE count, then per certificate
u16 BE subject length, u16 BE key length, subject DER, SubjectPublicKeyInfo DER,
sorted by subject DER. Source: Mozilla CA list as published by curl
(override with HUPROBE_CACERT_PEM=<path>). Pure Python, no cryptography package.
"""

import base64
import os
import re
import struct
import urllib.request

Import("env")  # noqa: F821  (SCons)

CACERT_URL = "https://curl.se/ca/cacert.pem"
OUT = os.path.join(env.subst("$PROJECT_DIR"), "data", "cert", "x509_crt_bundle.bin")  # noqa: F821


def _tlv(b: bytes, i: int):
    """(tag, header_start, content_start, end) of the DER element at i."""
    start = i
    tag = b[i]
    i += 1
    n = b[i]
    i += 1
    if n & 0x80:
        k = n & 0x7F
        n = int.from_bytes(b[i:i + k], "big")
        i += k
    return tag, start, i, i + n


def _subject_and_key(der: bytes):
    _, _, c, _ = _tlv(der, 0)          # Certificate
    _, _, t, t_end = _tlv(der, c)      # tbsCertificate
    parts = []
    i = t
    while i < t_end:
        tag, s, _, e = _tlv(der, i)
        if not (tag == 0xA0 and not parts):  # skip explicit [0] version
            parts.append(der[s:e])
        i = e
    # serial, signature, issuer, validity, subject, subjectPublicKeyInfo
    return parts[4], parts[5]


def _pem_certs(text: str):
    for m in re.finditer(r"-----BEGIN CERTIFICATE-----(.+?)-----END CERTIFICATE-----", text, re.S):
        yield base64.b64decode("".join(m.group(1).split()))


def _load_pem() -> str:
    path = os.environ.get("HUPROBE_CACERT_PEM")
    if path:
        with open(path, encoding="ascii", errors="ignore") as f:
            return f.read()
    with urllib.request.urlopen(CACERT_URL, timeout=30) as r:
        return r.read().decode("ascii", errors="ignore")


def build():
    if os.path.isfile(OUT) and os.path.getsize(OUT) > 2:
        return
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    entries = []
    try:
        for der in _pem_certs(_load_pem()):
            try:
                entries.append(_subject_and_key(der))
            except (IndexError, ValueError):
                continue
    except Exception as e:  # offline build: empty bundle, GitHub OTA then fails TLS cleanly
        print(f"[gen_ca_bundle] WARNING: no CA list ({e}); writing empty bundle")
    entries.sort(key=lambda x: x[0])
    blob = struct.pack(">H", len(entries))
    for name, key in entries:
        blob += struct.pack(">HH", len(name), len(key)) + name + key
    with open(OUT, "wb") as f:
        f.write(blob)
    print(f"[gen_ca_bundle] {len(entries)} certificates -> {OUT} ({len(blob)} B)")


build()
