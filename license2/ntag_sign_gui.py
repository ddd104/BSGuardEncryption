import os
import struct
import datetime
import tkinter as tk
from tkinter import filedialog, messagebox

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.backends import default_backend


def generate_keypair(priv_path: str, pub_path: str, key_bits: int = 2048):
    """生成 RSA 密钥对并保存到 PEM 文件"""
    private_key = rsa.generate_private_key(
        public_exponent=65537,
        key_size=key_bits,
        backend=default_backend()
    )
    # 私钥（PKCS#8，无密码）
    priv_pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption()
    )
    # 公钥（SubjectPublicKeyInfo）
    public_key = private_key.public_key()
    pub_pem = public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )

    with open(priv_path, "wb") as f:
        f.write(priv_pem)
    with open(pub_path, "wb") as f:
        f.write(pub_pem)


def encrypt_shared_key(pub_pem_path: str, out_path: str, key_len: int = 32) -> int:
    """随机生成 sharedKey（默认32字节），使用 RSA 公钥 OAEP+SHA256 加密，写入 out_path（二进制）"""
    from os import urandom
    shared_key = urandom(key_len)

    with open(pub_pem_path, "rb") as f:
        public_key = serialization.load_pem_public_key(f.read(), backend=default_backend())

    ciphertext = public_key.encrypt(
        shared_key,
        padding.OAEP(mgf=padding.MGF1(algorithm=hashes.SHA256()), algorithm=hashes.SHA256(), label=None)
    )

    with open(out_path, "wb") as f:
        f.write(ciphertext)

    return len(ciphertext)


def decrypt_shared_key(priv_pem_path: str, enc_path: str) -> bytes:
    """读取加密的 sharedKey 文件并用私钥解密"""
    with open(priv_pem_path, "rb") as f:
        private_key = serialization.load_pem_private_key(f.read(), password=None, backend=default_backend())

    with open(enc_path, "rb") as f:
        ciphertext = f.read()

    shared_key = private_key.decrypt(
        ciphertext,
        padding.OAEP(mgf=padding.MGF1(algorithm=hashes.SHA256()), algorithm=hashes.SHA256(), label=None)
    )
    return shared_key


def sign_license(team: str, expires: str, shared_key: bytes, priv_pem_path: str, asset_pack: str = "") -> bytes:
    """构造并签名 License 二进制数据"""
    private_key = serialization.load_pem_private_key(open(priv_pem_path, "rb").read(), password=None, backend=default_backend())

    team_b = team.encode("utf-8")
    asset_b = asset_pack.encode("utf-8")
    key_b = shared_key
    # 允许仅日期字符串（YYYY-MM-DD），视为当天 00:00:00
    # 如果用户提供完整 ISO 字符串（YYYY-MM-DDTHH:MM:SS），也可解析
    try:
        dt = datetime.datetime.fromisoformat(expires)
    except ValueError:
        # 容错：若用户仅输入YYYY-MM-DD且 fromisoformat 失败，尝试追加 00:00:00
        dt = datetime.datetime.fromisoformat(expires + "T00:00:00")
    exp_ts = int(dt.timestamp())

    data = bytearray()
    data += b"BSL1"
    data += struct.pack("<I", len(team_b)) + team_b
    data += struct.pack("<I", len(asset_b)) + asset_b
    data += struct.pack("<q", exp_ts)
    data += struct.pack("<I", len(key_b)) + key_b

    signature = private_key.sign(bytes(data), padding.PKCS1v15(), hashes.SHA256())
    data += struct.pack("<I", len(signature)) + signature

    return bytes(data)


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ntag_sign GUI")
        self.geometry("700x360")

        # 行 0：团队
        tk.Label(self, text="团队 (Team)：").grid(row=0, column=0, sticky="e", padx=8, pady=8)
        self.team_var = tk.StringVar()
        tk.Entry(self, textvariable=self.team_var, width=50).grid(row=0, column=1, columnspan=3, sticky="we", padx=8)

        # 行 1：过期时间
        tk.Label(self, text="过期时间：").grid(row=1, column=0, sticky="e", padx=8, pady=8)
        self.expires_var = tk.StringVar(value=datetime.date.today().isoformat())
        tk.Entry(self, textvariable=self.expires_var, width=20).grid(row=1, column=1, sticky="w", padx=8)
        tk.Label(self, text="格式: YYYY-MM-DD 或 YYYY-MM-DDTHH:MM:SS").grid(row=1, column=2, columnspan=2, sticky="w")

        # 行 2：私钥路径
        tk.Label(self, text="私钥 (root_private.pem)：").grid(row=2, column=0, sticky="e", padx=8, pady=8)
        self.priv_path = tk.StringVar(value="root_private.pem")
        tk.Entry(self, textvariable=self.priv_path, width=50).grid(row=2, column=1, columnspan=2, sticky="we", padx=8)
        tk.Button(self, text="浏览", command=self.browse_priv).grid(row=2, column=3, padx=8)

        # 行 3：公钥路径
        tk.Label(self, text="公钥 (root_public.pem)：").grid(row=3, column=0, sticky="e", padx=8, pady=8)
        self.pub_path = tk.StringVar(value="root_public.pem")
        tk.Entry(self, textvariable=self.pub_path, width=50).grid(row=3, column=1, columnspan=2, sticky="we", padx=8)
        tk.Button(self, text="浏览", command=self.browse_pub).grid(row=3, column=3, padx=8)

        # 行 4：SharedKey 加密文件
        tk.Label(self, text="SharedKey 加密文件：").grid(row=4, column=0, sticky="e", padx=8, pady=8)
        self.sk_enc_path = tk.StringVar(value="SharedKey.enc")
        tk.Entry(self, textvariable=self.sk_enc_path, width=50).grid(row=4, column=1, columnspan=2, sticky="we", padx=8)
        tk.Button(self, text="浏览", command=self.browse_sk).grid(row=4, column=3, padx=8)

        # 行 5：License 输出路径
        tk.Label(self, text="License 输出文件：").grid(row=5, column=0, sticky="e", padx=8, pady=8)
        self.lic_out_path = tk.StringVar(value="license.bin")
        tk.Entry(self, textvariable=self.lic_out_path, width=50).grid(row=5, column=1, columnspan=2, sticky="we", padx=8)
        tk.Button(self, text="选择", command=self.choose_lic_out).grid(row=5, column=3, padx=8)

        # 行 6：操作按钮
        tk.Button(self, text="生成根密钥对", command=self.on_gen_keys, width=18).grid(row=6, column=0, padx=8, pady=16)
        tk.Button(self, text="生成并加密 SharedKey", command=self.on_gen_shared_key, width=18).grid(row=6, column=1, padx=8, pady=16)
        tk.Button(self, text="生成 License", command=self.on_gen_license, width=18).grid(row=6, column=2, padx=8, pady=16)

        # 自适应列宽
        self.grid_columnconfigure(1, weight=1)
        self.grid_columnconfigure(2, weight=1)

    # 文件对话框函数
    def browse_priv(self):
        path = filedialog.askopenfilename(title="选择私钥 PEM 文件", filetypes=[("PEM files", "*.pem"), ("All files", "*.*")])
        if path:
            self.priv_path.set(path)

    def browse_pub(self):
        path = filedialog.askopenfilename(title="选择公钥 PEM 文件", filetypes=[("PEM files", "*.pem"), ("All files", "*.*")])
        if path:
            self.pub_path.set(path)

    def browse_sk(self):
        path = filedialog.askopenfilename(title="选择加密的 SharedKey 文件", filetypes=[("All files", "*.*")])
        if path:
            self.sk_enc_path.set(path)

    def choose_lic_out(self):
        path = filedialog.asksaveasfilename(title="选择 License 输出文件", defaultextension=".bin",
                                            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")])
        if path:
            self.lic_out_path.set(path)

    # 业务动作
    def on_gen_keys(self):
        try:
            priv = self.priv_path.get() or "root_private.pem"
            pub = self.pub_path.get() or "root_public.pem"
            generate_keypair(priv, pub, key_bits=2048)
            messagebox.showinfo("完成", f"已生成密钥对：\n{priv}\n{pub}")
        except Exception as e:
            messagebox.showerror("错误", f"生成密钥对失败：{e}")

    def on_gen_shared_key(self):
        try:
            pub = self.pub_path.get()
            if not os.path.isfile(pub):
                messagebox.showwarning("提示", "请先选择或生成公钥（root_public.pem）。")
                return
            out_path = self.sk_enc_path.get() or "SharedKey.enc"
            ct_len = encrypt_shared_key(pub, out_path, key_len=32)
            messagebox.showinfo("完成", f"已生成并加密 SharedKey：\n{out_path}\n密文长度：{ct_len} 字节")
        except Exception as e:
            messagebox.showerror("错误", f"生成/加密 SharedKey 失败：{e}")

    def on_gen_license(self):
        try:
            team = self.team_var.get().strip()
            expires = self.expires_var.get().strip()
            priv = self.priv_path.get().strip()
            sk_enc = self.sk_enc_path.get().strip()
            out_path = self.lic_out_path.get().strip() or "license.bin"

            if not team:
                messagebox.showwarning("提示", "请输入团队（Team）。")
                return
            if not os.path.isfile(priv):
                messagebox.showwarning("提示", "请先选择或生成私钥（root_private.pem）。")
                return
            if not os.path.isfile(sk_enc):
                messagebox.showwarning("提示", "请先生成或选择加密的 SharedKey 文件。")
                return

            # 解密 sharedKey
            shared_key = decrypt_shared_key(priv, sk_enc)
            # 生成 License
            blob = sign_license(team, expires, shared_key, priv, asset_pack="")
            with open(out_path, "wb") as f:
                f.write(blob)

            messagebox.showinfo("完成", f"License 已生成：\n{out_path}\n大小：{len(blob)} 字节")
        except Exception as e:
            messagebox.showerror("错误", f"生成 License 失败：{e}")


def main():
    app = App()
    app.mainloop()


if __name__ == "__main__":
    main()