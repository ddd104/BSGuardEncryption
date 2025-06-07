import json, base64, sys, datetime
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

import hashlib, binascii

def sign_license(user:str, expires:str, shared_key_hex:str, priv_pem:str):
    priv_key = serialization.load_pem_private_key(
        open(priv_pem, 'rb').read(), password=None)
    # 构造 JSON（不含 Sig 字段）
    lic = {
        "User":      user,
        "Expires":   expires,                   # ISO-8601, e.g. 2026-01-01
        "SharedKey": base64.b64encode(bytes.fromhex(shared_key_hex)).decode()
    }
    canonical = json.dumps(lic, separators=(',',':'), sort_keys=True)
    sig = priv_key.sign(
        canonical.encode(),
        padding.PKCS1v15(),
        hashes.SHA256())
    
    h = hashlib.sha256(canonical.encode()).digest()
    print("Digest:", binascii.hexlify(h).decode())
    lic["Sig"] = base64.b64encode(sig).decode()
    return lic

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("usage: ntag_sign.py <user> <YYYY-MM-DD> <shared_key_hex> <priv.pem>")
        sys.exit(1)
    lic = sign_license(*sys.argv[1:])
    print(json.dumps(lic, indent=2))
