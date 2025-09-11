#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ntag_sign GUI 工具（PUBKDF 专用版，SharedKey.enc 无需私钥即可解封）

功能：
1) 生成根密钥对（root_private.pem / root_public.pem）
2) 生成 SharedKey.enc（BSK1 格式，PUBKDF：用公钥 SPKI + salt 通过 SHA256 -> AES-256-GCM）
3) 输入团队与过期日期，读取 SharedKey.enc（仅需公钥）并生成 License（二进制，"BSL1" 结构）
   License 使用 root_private.pem 进行 RSA PKCS#1 v1.5 + SHA256 签名（仅在发行端需要私钥，客户端不需要）

BSK1 文件格式（小端序）：
  magic[4]    : "BSK1"
  mode[1]     : 固定 1（PUBKDF）
  salt_len[1] : s
  salt[s]
  nonce[12]
  ct_len[2]
  ciphertext[ct_len]
  tag[16]
AES-GCM 的 AAD = "BSK1" || mode(=1) || salt
"""
import os, struct, datetime, tkinter as tk
from tkinter import filedialog, messagebox
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.backends import default_backend

def _sha256(data: bytes) -> bytes:
    d = hashes.Hash(hashes.SHA256(), backend=default_backend()); d.update(data); return d.finalize()

def _pub_spki_der(pub_pem_path: str) -> bytes:
    pub = serialization.load_pem_public_key(open(pub_pem_path, "rb").read(), backend=default_backend())
    return pub.public_bytes(encoding=serialization.Encoding.DER, format=serialization.PublicFormat.SubjectPublicKeyInfo)

def generate_keypair(priv_path: str, pub_path: str, key_bits: int = 2048):
    private_key = rsa.generate_private_key(public_exponent=65537, key_size=key_bits, backend=default_backend())
    priv_pem = private_key.private_bytes(encoding=serialization.Encoding.PEM, format=serialization.PrivateFormat.PKCS8, encryption_algorithm=serialization.NoEncryption())
    public_key = private_key.public_key()
    pub_pem = public_key.public_bytes(encoding=serialization.Encoding.PEM, format=serialization.PublicFormat.SubjectPublicKeyInfo)
    open(priv_path, "wb").write(priv_pem); open(pub_path, "wb").write(pub_pem)

def build_sharedkey_enc_pubkdf(pub_pem_path: str, out_path: str, key_len: int = 32) -> bytes:
    from os import urandom
    shared_key = urandom(key_len); salt = urandom(16); spki = _pub_spki_der(pub_pem_path)
    aes_key = _sha256(spki + salt); aesgcm = AESGCM(aes_key); nonce = urandom(12); aad = b"BSK1"+bytes([1])+salt
    ct = aesgcm.encrypt(nonce, shared_key, aad); ciphertext, tag = ct[:-16], ct[-16:]
    blob = bytearray(); blob += b"BSK1"; blob += bytes([1]); blob += bytes([len(salt)]); blob += salt
    blob += nonce; blob += struct.pack("<H", len(ciphertext)); blob += ciphertext; blob += tag
    open(out_path, "wb").write(blob); return shared_key

def read_sharedkey_enc_pubkdf(enc_path: str, pub_pem_path: str) -> bytes:
    b = open(enc_path, "rb").read()
    if len(b) < 4+1+1+12+2+16: raise ValueError("SharedKey.enc 文件过短")
    off=0; magic=b[off:off+4]; off+=4
    if magic != b"BSK1": raise ValueError("非法的 BSK1 头")
    mode=b[off]; off+=1
    if mode != 1: raise ValueError(f"不支持的 BSK1 模式：{mode}（此版本仅支持 PUBKDF=1）")
    salt_len=b[off]; off+=1; salt=b[off:off+salt_len]; off+=salt_len
    nonce=b[off:off+12]; off+=12; ct_len=struct.unpack("<H", b[off:off+2])[0]; off+=2
    ciphertext=b[off:off+ct_len]; off+=ct_len; tag=b[off:off+16]; off+=16
    spki=_pub_spki_der(pub_pem_path); aes_key=_sha256(spki+salt); aad=b"BSK1"+bytes([1])+salt
    aesgcm=AESGCM(aes_key); shared_key=aesgcm.decrypt(nonce, ciphertext+tag, aad); return shared_key

def sign_license(team: str, expires: str, shared_key: bytes, priv_pem_path: str, asset_pack: str = "") -> bytes:
    private_key = serialization.load_pem_private_key(open(priv_pem_path,"rb").read(), password=None, backend=default_backend())
    team_b=team.encode("utf-8"); asset_b=asset_pack.encode("utf-8"); key_b=shared_key
    try: dt=datetime.datetime.fromisoformat(expires)
    except ValueError: dt=datetime.datetime.fromisoformat(expires+"T00:00:00")
    exp_ts=int(dt.timestamp())
    data=bytearray(); data+=b"BSL1"
    data+=struct.pack("<I", len(team_b))+team_b
    data+=struct.pack("<I", len(asset_b))+asset_b
    data+=struct.pack("<q", exp_ts)
    data+=struct.pack("<I", len(key_b))+key_b
    from cryptography.hazmat.primitives.asymmetric import padding as asy_padding
    signature = private_key.sign(bytes(data), asy_padding.PKCS1v15(), hashes.SHA256())
    data+=struct.pack("<I", len(signature))+signature; return bytes(data)

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ntag_sign GUI (BSK1 / PUBKDF)"); self.geometry("760x400")
        tk.Label(self, text="团队 (Team)：").grid(row=0,column=0,sticky="e",padx=8,pady=8)
        self.team_var=tk.StringVar(); tk.Entry(self,textvariable=self.team_var,width=50).grid(row=0,column=1,columnspan=3,sticky="we",padx=8)
        tk.Label(self, text="过期时间：").grid(row=1,column=0,sticky="e",padx=8,pady=8)
        self.expires_var=tk.StringVar(value=datetime.date.today().isoformat())
        tk.Entry(self,textvariable=self.expires_var,width=20).grid(row=1,column=1,sticky="w",padx=8)
        tk.Label(self, text="格式: YYYY-MM-DD 或 YYYY-MM-DDTHH:MM:SS").grid(row=1,column=2,columnspan=2,sticky="w")
        tk.Label(self, text="私钥 (root_private.pem)：").grid(row=2,column=0,sticky="e",padx=8,pady=8)
        self.priv_path=tk.StringVar(value="root_private.pem")
        tk.Entry(self,textvariable=self.priv_path,width=50).grid(row=2,column=1,columnspan=2,sticky="we",padx=8)
        tk.Button(self,text="浏览",command=self.browse_priv).grid(row=2,column=3,padx=8)
        tk.Label(self, text="公钥 (root_public.pem)：").grid(row=3,column=0,sticky="e",padx=8,pady=8)
        self.pub_path=tk.StringVar(value="root_public.pem")
        tk.Entry(self,textvariable=self.pub_path,width=50).grid(row=3,column=1,columnspan=2,sticky="we",padx=8)
        tk.Button(self,text="浏览",command=self.browse_pub).grid(row=3,column=3,padx=8)
        tk.Label(self, text="SharedKey.enc：").grid(row=4,column=0,sticky="e",padx=8,pady=8)
        self.sk_enc_path=tk.StringVar(value="SharedKey.enc")
        tk.Entry(self,textvariable=self.sk_enc_path,width=50).grid(row=4,column=1,columnspan=2,sticky="we",padx=8)
        tk.Button(self,text="浏览",command=self.browse_sk).grid(row=4,column=3,padx=8)
        tk.Label(self, text="License 输出文件：").grid(row=5,column=0,sticky="e",padx=8,pady=8)
        self.lic_out_path=tk.StringVar(value="license.bin")
        tk.Entry(self,textvariable=self.lic_out_path,width=50).grid(row=5,column=1,columnspan=2,sticky="we",padx=8)
        tk.Button(self,text="选择",command=self.choose_lic_out).grid(row=5,column=3,padx=8)
        tk.Button(self,text="生成根密钥对",command=self.on_gen_keys,width=18).grid(row=6,column=0,padx=8,pady=16)
        tk.Button(self,text="生成 SharedKey.enc",command=self.on_gen_shared_key_bsk1,width=18).grid(row=6,column=1,padx=8,pady=16)
        tk.Button(self,text="生成 License",command=self.on_gen_license,width=18).grid(row=6,column=2,padx=8,pady=16)
        self.grid_columnconfigure(1,weight=1); self.grid_columnconfigure(2,weight=1)

    def browse_priv(self):
        p=filedialog.askopenfilename(title="选择私钥 PEM 文件",filetypes=[("PEM files","*.pem"),("All files","*.*")])
        if p: self.priv_path.set(p)
    def browse_pub(self):
        p=filedialog.askopenfilename(title="选择公钥 PEM 文件",filetypes=[("PEM files","*.pem"),("All files","*.*")])
        if p: self.pub_path.set(p)
    def browse_sk(self):
        p=filedialog.askopenfilename(title="选择 SharedKey.enc (BSK1/PUBKDF)",filetypes=[("All files","*.*")])
        if p: self.sk_enc_path.set(p)
    def choose_lic_out(self):
        p=filedialog.asksaveasfilename(title="选择 License 输出文件",defaultextension=".bin",filetypes=[("Binary files","*.bin"),("All files","*.*")])
        if p: self.lic_out_path.set(p)

    def on_gen_keys(self):
        try:
            priv=self.priv_path.get() or "root_private.pem"; pub=self.pub_path.get() or "root_public.pem"
            generate_keypair(priv,pub,2048); messagebox.showinfo("完成", f"已生成密钥对：\n{priv}\n{pub}")
        except Exception as e:
            messagebox.showerror("错误", f"生成密钥对失败：{e}")

    def on_gen_shared_key_bsk1(self):
        try:
            pub=self.pub_path.get()
            if not os.path.isfile(pub): messagebox.showwarning("提示","请先选择或生成公钥（root_public.pem）。"); return
            out_path=self.sk_enc_path.get() or "SharedKey.enc"
            sk=build_sharedkey_enc_pubkdf(pub,out_path,32); messagebox.showinfo("完成", f"已生成 BSK1 SharedKey.enc（PUBKDF）：\n{out_path}\nsharedKey长度：{len(sk)} 字节")
        except Exception as e:
            messagebox.showerror("错误", f"生成 SharedKey.enc 失败：{e}")

    def on_gen_license(self):
        try:
            team=self.team_var.get().strip(); expires=self.expires_var.get().strip()
            priv=self.priv_path.get().strip(); sk_enc=self.sk_enc_path.get().strip(); pub=self.pub_path.get().strip()
            out_path=self.lic_out_path.get().strip() or "license.bin"
            if not team: messagebox.showwarning("提示","请输入团队（Team）。"); return
            if not os.path.isfile(priv): messagebox.showwarning("提示","请先选择或生成私钥（root_private.pem），用于签名 License。"); return
            if not os.path.isfile(sk_enc): messagebox.showwarning("提示","请先生成或选择 SharedKey.enc (BSK1/PUBKDF)。"); return
            if not os.path.isfile(pub): messagebox.showwarning("提示","需要公钥（root_public.pem）以解封 SharedKey.enc。"); return
            shared_key=read_sharedkey_enc_pubkdf(sk_enc,pub)
            blob=sign_license(team,expires,shared_key,priv,asset_pack="")
            open(out_path,"wb").write(blob); messagebox.showinfo("完成", f"License 已生成：\n{out_path}\n大小：{len(blob)} 字节")
        except Exception as e:
            messagebox.showerror("错误", f"生成 License 失败：{e}")

def main():
    app=App(); app.mainloop()
if __name__=="__main__": main()
