# Milvus 3.x Examples & Tools

A small collection of scripts demonstrating **Milvus 3.x** vector database usage through the
`MilvusClient` API, running locally with **Milvus Lite** (a plain `.db` file, no Docker or
server required).

## Contents

| File | Description |
| --- | --- |
| `milvus-demo.py` | Basic workflow: create a collection, insert vectors, load it, and run a similarity search. |
| `milvus-encrypt-demo.py` | Same workflow, but scalar text is encrypted with AES-256-CBC (via `cryptography`) before being stored, and decrypted after retrieval. |
| `milvus-tool.py` | CLI tool to inspect a local Milvus Lite `.db`: list collections, show schemas and row counts, or dump records. |
| `requirements.txt` | Pinned dependency ranges for the Milvus 3.x SDK. |

## Requirements

- Python >= 3.10
- Dependencies (Milvus 3.x SDK):

```bash
pip install -r requirements.txt
```

Key packages: `pymilvus>=3.0,<4.0`, `milvus-lite>=3.0,<4.0`, `cryptography>=50.0.2`.

## Usage

### Basic demo

```bash
python milvus-demo.py
```

Creates `milvus_basic.db`, inserts three 3-dimensional vectors, and prints the top-2 matches:

```text
搜索结果：
  ID: 1, 距离: 1.0000, 文本: Milvus is a vector database
  ID: 2, 距离: 0.9746, 文本: Vector search enables semantic search
```

### Encryption demo

```bash
python milvus-encrypt-demo.py
```

Encrypts the scalar `text` field before insertion, then queries the stored ciphertext and
decrypts it in the search results. The AES key is hard-coded for demonstration only — use a
secure key source in production.

### Inspecting the local database

```bash
# Overview: collection names, row counts, and schemas
python milvus-tool.py show

# Dump records from a collection (default: 10 rows, first 3 vector dims)
python milvus-tool.py show -c basic_demo

# Filter, limit, and print full vectors
python milvus-tool.py show -c basic_demo -n 5 -f "id in [1, 2]" --full

# Specify a database file explicitly
python milvus-tool.py show -p ./milvus_basic.db -c encrypt_demo
```

## Notes

- Milvus Lite stores data in a local `.db` directory; `*.db` is ignored by `.gitignore`.
- The `milvus-tool.py` output requires a UTF-8 capable terminal. On Windows, run with
  `PYTHONIOENCODING=utf-8` if Chinese characters appear garbled.
- This project targets the **Milvus 3.x Python SDK**; the older 2.x API is not supported.
