#!/usr/bin/env python3
"""Generate debug data for local testing.

Creates, only when missing:
  1. data/updata.zip  - zip containing a.txt and b.txt with random content
  2. keys/aes256.key  - raw 32-byte AES-256 key
  3. keys/ecdsa_private.der / keys/ecdsa_public.der - ECDSA P-256 key pair (DER)
"""
import secrets
import string
import sys
import zipfile
from pathlib import Path

ZIP_NAME = "updata.zip"
AES_KEY_NAME = "aes256.key"
ECDSA_PRIVATE_NAME = "ecdsa_private.der"
ECDSA_PUBLIC_NAME = "ecdsa_public.der"

TEXT_LENGTH = 10


def random_text(length=TEXT_LENGTH):
    """Return a random string of letters and digits."""
    alphabet = string.ascii_letters + string.digits
    return "".join(secrets.choice(alphabet) for _ in range(length))


def prepare_zip(data_dir):
    zip_path = data_dir / ZIP_NAME
    if zip_path.exists():
        print(f"  [=] {zip_path} already exists, skip")
        return

    data_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for name in ("a.txt", "b.txt"):
            zf.writestr(name, random_text())
    print(f"  [+] generated {zip_path}")


def prepare_aes_key(keys_dir):
    key_path = keys_dir / AES_KEY_NAME
    if key_path.exists():
        print(f"  [=] {key_path} already exists, skip")
        return

    keys_dir.mkdir(parents=True, exist_ok=True)
    # AES-256 = 32 bytes raw key
    key_path.write_bytes(secrets.token_bytes(32))
    print(f"  [+] generated {key_path}")


def prepare_ecdsa_keys(keys_dir):
    priv_path = keys_dir / ECDSA_PRIVATE_NAME
    pub_path = keys_dir / ECDSA_PUBLIC_NAME

    if priv_path.exists() and pub_path.exists():
        print(f"  [=] ECDSA key pair already exists, skip")
        return

    try:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric import ec
    except ImportError as exc:
        raise RuntimeError(
            "Python package 'cryptography' is required to generate ECDSA keys "
            "(pip install cryptography)"
        ) from exc

    if priv_path.exists() != pub_path.exists():
        print("  [!] incomplete ECDSA key pair found, regenerating both")

    keys_dir.mkdir(parents=True, exist_ok=True)
    private_key = ec.generate_private_key(ec.SECP256R1())

    priv_path.write_bytes(
        private_key.private_bytes(
            encoding=serialization.Encoding.DER,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.NoEncryption(),
        )
    )
    pub_path.write_bytes(
        private_key.public_key().public_bytes(
            encoding=serialization.Encoding.DER,
            format=serialization.PublicFormat.SubjectPublicKeyInfo,
        )
    )
    print(f"  [+] generated {priv_path}")
    print(f"  [+] generated {pub_path}")


def main():
    root = Path(__file__).resolve().parent
    print(f"Root location: {root}")
    print("=" * 60)
    print("Preparing debug data...")

    prepare_zip(root / "data")
    prepare_aes_key(root / "keys")
    prepare_ecdsa_keys(root / "keys")

    print("Debug data ready")
    print("=" * 60)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)
