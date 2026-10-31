import base64
import os
import shutil
import sys

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from pymilvus import CollectionSchema, DataType, FieldSchema, MilvusClient

# Force UTF-8 output so non-ASCII text is not garbled on Windows consoles
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

DB_PATH = "milvus_encrypt.db"

# Remove any leftover database from a previous run so the demo starts clean.
# Milvus Lite stores the database as a directory in newer versions and as a
# plain file in older ones, so handle both.
if os.path.isdir(DB_PATH):
    shutil.rmtree(DB_PATH)
elif os.path.exists(DB_PATH):
    os.remove(DB_PATH)

# ==================== AES-256-CBC encrypt/decrypt ====================
# 32-byte key. Hard-coded only for demo purposes; load it from a secure
# source in production.
AES_KEY = b"0123456789abcdef0123456789abcdef"
BLOCK_SIZE = 16  # AES block size, also the IV length


def _pkcs7_pad(data: bytes) -> bytes:
    """PKCS7 padding, matching the manual padding in 04-crypto/03-AES"""
    pad_len = BLOCK_SIZE - len(data) % BLOCK_SIZE
    return data + bytes([pad_len]) * pad_len


def _pkcs7_unpad(data: bytes) -> bytes:
    pad_len = data[-1]
    if not 1 <= pad_len <= BLOCK_SIZE:
        raise ValueError(f"Invalid PKCS7 padding length: {pad_len}")
    return data[:-pad_len]


def encrypt(plaintext: str) -> str:
    """plaintext -> base64(IV + ciphertext), convenient for a VARCHAR field"""
    iv = os.urandom(BLOCK_SIZE)  # random per call, so the same plaintext encrypts differently
    data = _pkcs7_pad(plaintext.encode("utf-8"))

    encryptor = Cipher(algorithms.AES(AES_KEY), modes.CBC(iv)).encryptor()
    ciphertext = encryptor.update(data) + encryptor.finalize()

    return base64.b64encode(iv + ciphertext).decode("ascii")


def decrypt(token: str) -> str:
    """base64(IV + ciphertext) -> plaintext"""
    raw = base64.b64decode(token)
    iv, ciphertext = raw[:BLOCK_SIZE], raw[BLOCK_SIZE:]

    decryptor = Cipher(algorithms.AES(AES_KEY), modes.CBC(iv)).decryptor()
    data = decryptor.update(ciphertext) + decryptor.finalize()

    return _pkcs7_unpad(data).decode("utf-8")


# ==================== Milvus workflow ====================
# 1. Connect to Milvus Lite (local file, zero config, no Docker required)
client = MilvusClient(DB_PATH)

COLLECTION_NAME = "collection_demo"

# Drop the collection first if it exists, so the script can be re-run
if client.has_collection(COLLECTION_NAME):
    client.drop_collection(COLLECTION_NAME)

# 2. Define the schema: primary key + vector + scalar
# (text stores ciphertext, so max_length is increased to 4096)
schema = CollectionSchema(
    fields=[
        FieldSchema(name="id", dtype=DataType.INT64, is_primary=True),
        FieldSchema(name="vector", dtype=DataType.FLOAT_VECTOR, dim=3),
        FieldSchema(name="text", dtype=DataType.VARCHAR, max_length=4096),
    ]
)

# 3. Create the collection (index is created automatically)
client.create_collection(
    collection_name=COLLECTION_NAME,
    schema=schema,
)

# 4. Prepare data: encrypt the scalar text before handing it to Milvus
records = [
    (1, [0.1, 0.2, 0.3], "Milvus is a vector database"),
    (2, [0.4, 0.5, 0.6], "Vector search enables semantic search"),
    (3, [0.7, 0.8, 0.9], "Embeddings capture the semantic meaning of data"),
]

data = [
    {"id": rid, "vector": vec, "text": encrypt(text)}
    for rid, vec, text in records
]

# Effect of the random IV: encrypting the same plaintext twice gives different results
print("Encryption demo (same plaintext encrypted twice):")
print(f"  First : {encrypt(records[0][2])}")
print(f"  Second: {encrypt(records[0][2])}")
print()

# 5. Insert data (ciphertext goes in)
client.insert(collection_name=COLLECTION_NAME, data=data)

# 6. Load the collection (required before searching)
client.load_collection(COLLECTION_NAME)

# 7. Inspect what is actually stored -- ciphertext; without the key it cannot be read
stored = client.query(
    collection_name=COLLECTION_NAME,
    filter="id in [1, 2, 3]",
    output_fields=["id", "text"],
)
print("Scalar data actually stored in the collection:")
for row in sorted(stored, key=lambda r: r["id"]):
    print(f"  ID: {row['id']}, text: {row['text']}")
print()

# 8. Vector search (the search still uses plaintext vectors; ciphertext comes back with results)
search_result = client.search(
    collection_name=COLLECTION_NAME,
    data=[[0.1, 0.2, 0.3]],   # query vector
    limit=2,                   # return the 2 most similar results
    output_fields=["text"],    # also return the scalar field (ciphertext)
)

# 9. Print results: ciphertext + plaintext recovered by decryption
print("Search results:")
for hits in search_result:
    for hit in hits:
        cipher_text = hit["entity"]["text"]
        print(f"  ID: {hit['id']}, distance: {hit['distance']:.4f}")
        print(f"    ciphertext: {cipher_text}")
        print(f"    plaintext : {decrypt(cipher_text)}")

client.close()
