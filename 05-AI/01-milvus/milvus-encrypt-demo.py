import base64
import os
import sys

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from pymilvus import CollectionSchema, DataType, FieldSchema, MilvusClient

# Windows 控制台默认 GBK，中文输出会乱码，这里显式改为 UTF-8
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

# ==================== AES-256-CBC 加解密 ====================
# 32 字节密钥。仅供示例固化在源码中，生产环境请改为从安全渠道获取
AES_KEY = b"0123456789abcdef0123456789abcdef"
BLOCK_SIZE = 16  # AES 分组长度，同时也是 IV 长度


def _pkcs7_pad(data: bytes) -> bytes:
    """PKCS7 填充，与 04-crypto/03-AES 的手工填充保持一致"""
    pad_len = BLOCK_SIZE - len(data) % BLOCK_SIZE
    return data + bytes([pad_len]) * pad_len


def _pkcs7_unpad(data: bytes) -> bytes:
    pad_len = data[-1]
    if not 1 <= pad_len <= BLOCK_SIZE:
        raise ValueError(f"非法的 PKCS7 填充长度: {pad_len}")
    return data[:-pad_len]


def encrypt(plaintext: str) -> str:
    """明文 -> base64(IV + 密文)，便于存进 VARCHAR 字段"""
    iv = os.urandom(BLOCK_SIZE)  # 每次随机，故同一明文每次密文都不同
    data = _pkcs7_pad(plaintext.encode("utf-8"))

    encryptor = Cipher(algorithms.AES(AES_KEY), modes.CBC(iv)).encryptor()
    ciphertext = encryptor.update(data) + encryptor.finalize()

    return base64.b64encode(iv + ciphertext).decode("ascii")


def decrypt(token: str) -> str:
    """base64(IV + 密文) -> 明文"""
    raw = base64.b64decode(token)
    iv, ciphertext = raw[:BLOCK_SIZE], raw[BLOCK_SIZE:]

    decryptor = Cipher(algorithms.AES(AES_KEY), modes.CBC(iv)).decryptor()
    data = decryptor.update(ciphertext) + decryptor.finalize()

    return _pkcs7_unpad(data).decode("utf-8")


# ==================== Milvus 流程 ====================
# 1. 连接 Milvus Lite（本地文件，零配置，无需 Docker）
client = MilvusClient("milvus_basic.db")

COLLECTION_NAME = "encrypt_demo"

# 如果集合已存在，先删除，保证代码可重复运行
if client.has_collection(COLLECTION_NAME):
    client.drop_collection(COLLECTION_NAME)

# 2. 定义 Schema：主键 + 向量 + 标量（text 存密文，故 max_length 放大到 4096）
schema = CollectionSchema(
    fields=[
        FieldSchema(name="id", dtype=DataType.INT64, is_primary=True),
        FieldSchema(name="vector", dtype=DataType.FLOAT_VECTOR, dim=3),
        FieldSchema(name="text", dtype=DataType.VARCHAR, max_length=4096),
    ]
)

# 3. 创建集合（自动创建索引）
client.create_collection(
    collection_name=COLLECTION_NAME,
    schema=schema,
)

# 4. 准备数据：标量 text 先加密，再交给 Milvus
records = [
    (1, [0.1, 0.2, 0.3], "Milvus 是一个向量数据库"),
    (2, [0.4, 0.5, 0.6], "向量检索用于语义搜索"),
    (3, [0.7, 0.8, 0.9], "Embedding 是数据的语义特征"),
]

data = [
    {"id": rid, "vector": vec, "text": encrypt(text)}
    for rid, vec, text in records
]

# 加密随机 IV 的效果：同一明文两次加密的结果并不相同
print("加密效果演示（同一明文两次加密）：")
print(f"  第一次: {encrypt(records[0][2])}")
print(f"  第二次: {encrypt(records[0][2])}")
print()

# 5. 插入数据（入的是密文）
client.insert(collection_name=COLLECTION_NAME, data=data)

# 6. 加载集合（搜索前必须加载）
client.load_collection(COLLECTION_NAME)

# 7. 看看库里实际落盘的内容——是密文，拿不到密钥就读不出原文
stored = client.query(
    collection_name=COLLECTION_NAME,
    filter="id in [1, 2, 3]",
    output_fields=["id", "text"],
)
print("库中实际存储的标量数据：")
for row in sorted(stored, key=lambda r: r["id"]):
    print(f"  ID: {row['id']}, text: {row['text']}")
print()

# 8. 向量搜索（搜索仍走明文向量，密文标量随结果一并返回）
search_result = client.search(
    collection_name=COLLECTION_NAME,
    data=[[0.1, 0.2, 0.3]],   # 查询向量
    limit=2,                   # 返回最相似的 2 条
    output_fields=["text"],    # 同时返回标量字段（密文）
)

# 9. 打印结果：密文 + 解密还原的原文
print("搜索结果：")
for hits in search_result:
    for hit in hits:
        cipher_text = hit["entity"]["text"]
        print(f"  ID: {hit['id']}, 距离: {hit['distance']:.4f}")
        print(f"    密文: {cipher_text}")
        print(f"    原文: {decrypt(cipher_text)}")

client.close()
