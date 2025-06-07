import sys, datetime, struct
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

import hashlib, binascii

def sign_license(user: str, expires: str, shared_key_hex: str, priv_pem: str, asset_pack: str = "") -> bytes:
    priv_key = serialization.load_pem_private_key(open(priv_pem, 'rb').read(), password=None)

    user_b = user.encode('utf-8')
    asset_b = asset_pack.encode('utf-8')
    key_b = bytes.fromhex(shared_key_hex)
    exp_ts = int(datetime.datetime.fromisoformat(expires).timestamp())

    data = bytearray()
    data += b"BSL1"
    data += struct.pack('<I', len(user_b)) + user_b
    data += struct.pack('<I', len(asset_b)) + asset_b
    data += struct.pack('<q', exp_ts)
    data += struct.pack('<I', len(key_b)) + key_b

    sig = priv_key.sign(bytes(data), padding.PKCS1v15(), hashes.SHA256())
    data += struct.pack('<I', len(sig)) + sig
    return bytes(data)

if __name__ == "__main__":
    if len(sys.argv) not in (5, 6):
        print("usage: ntag_sign.py <user> <YYYY-MM-DD> <shared_key_hex> <priv.pem> [asset_pack]")
        sys.exit(1)
    asset_pack = sys.argv[5] if len(sys.argv) == 6 else ""
    blob = sign_license(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], asset_pack)
    sys.stdout.buffer.write(blob)
