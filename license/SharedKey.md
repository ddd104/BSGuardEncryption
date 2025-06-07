2a307a821dcd790fd9fc7cd11000427420b36e9cc85b01a989a93a70bf63124a



# 生成私钥（PEM）
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out root_private.pem

# 导出公钥（PEM）——拷贝到插件源码 GRootPubKeyPem 处
openssl rsa -in root_private.pem -pubout -out  root_public.pem

# 生成 License
python ntag_sign.py TeamA 2026-01-01 2a307a821dcd790fd9fc7cd11000427420b36e9cc85b01a989a93a70bf63124a root_private.pem  > License.license



# 随机生成SharedKey
openssl rand -hex 32


# 验证SharedKey是否一致
python -c "import base64; print(base64.b64encode(bytes.fromhex('2a307a821dcd790fd9fc7cd11000427420b36e9cc85b01a989a93a70bf63124a')).decode())"


Digest: 760423af81eb716b0d3811ec503e4059e2c418c8174bcc6262fb91087f96cae8