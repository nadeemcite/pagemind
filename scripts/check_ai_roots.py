"""Check src/ai/AiTrustedRoots.h against real servers (Phase 4 check A6).

    python3 scripts/check_ai_roots.py [https://your-proxy.run.app]

Extracts the PEM bundle from the header, fetches the server's certificate
chain with openssl, and verifies it against the bundle only (no system
roots). Also checks that a chain the bundle should NOT trust is rejected.
Exit code 0 when every expectation holds.
"""

import re
import subprocess
import sys
import tempfile
from pathlib import Path
from urllib.parse import urlparse

HEADER = Path(__file__).resolve().parent.parent / "src" / "ai" / "AiTrustedRoots.h"
DEFAULT_TRUSTED = "https://ai-proxy-cyshxss5ta-uc.a.run.app"
# A public site whose chain does not end at a bundled root (DigiCert).
UNTRUSTED = "https://www.digicert.com"


def bundle_pem() -> str:
    text = HEADER.read_text(encoding="utf-8")
    lines = re.findall(r'^\s*"([^"]*)\\n"', text, re.M)
    pem = "\n".join(lines) + "\n"
    count = pem.count("BEGIN CERTIFICATE")
    if count == 0:
        sys.exit(f"no certificates found in {HEADER}")
    print(f"Bundle: {count} roots from {HEADER.name}")
    return pem


def server_chain(url: str) -> list[str]:
    host = urlparse(url).hostname
    out = subprocess.run(
        ["openssl", "s_client", "-connect", f"{host}:443", "-servername", host, "-showcerts"],
        input="", capture_output=True, text=True, timeout=30,
    ).stdout
    certs = re.findall(r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", out, re.S)
    if not certs:
        sys.exit(f"could not fetch the certificate chain of {host}")
    return certs


def verifies(url: str, bundle: Path) -> bool:
    chain = server_chain(url)
    with tempfile.TemporaryDirectory() as tmp:
        leaf = Path(tmp) / "leaf.pem"
        inter = Path(tmp) / "inter.pem"
        leaf.write_text(chain[0] + "\n")
        inter.write_text("\n".join(chain[1:]) + "\n")
        args = ["openssl", "verify", "-no-CApath", "-no-CAstore", "-CAfile", str(bundle)]
        if len(chain) > 1:
            args += ["-untrusted", str(inter)]
        result = subprocess.run(args + [str(leaf)], capture_output=True, text=True)
    return result.returncode == 0


def main() -> int:
    trusted_url = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_TRUSTED
    with tempfile.TemporaryDirectory() as tmp:
        bundle = Path(tmp) / "bundle.pem"
        bundle.write_text(bundle_pem())
        ok_trusted = verifies(trusted_url, bundle)
        ok_untrusted = not verifies(UNTRUSTED, bundle)
    print(f"{'PASS' if ok_trusted else 'FAIL'}  {trusted_url} verifies against the bundle")
    print(f"{'PASS' if ok_untrusted else 'FAIL'}  {UNTRUSTED} is rejected by the bundle")
    return 0 if ok_trusted and ok_untrusted else 1


if __name__ == "__main__":
    sys.exit(main())
