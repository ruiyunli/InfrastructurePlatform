# Milvus 2.x Examples & Tools

A small collection of scripts demonstrating **Milvus 2.x** vector database usage through the
`MilvusClient` API, running locally with **Milvus Lite** (a single `.db` file, no Docker or
server required).

## Contents

| File | Description |
| --- | --- |
| `milvus-demo.py` | Basic workflow: create a collection, insert vectors, load it, and run a similarity search. |
| `milvus-encrypt-demo.py` | Same workflow, but scalar text is encrypted with AES-256-CBC (via `cryptography`) before being stored, and decrypted after retrieval. |
| `milvus-tool.py` | CLI tool to inspect a local Milvus Lite `.db`: list collections, show schemas and row counts, or dump records. |
| `requirements.txt` | Pinned dependency versions for the Milvus 2.x SDK. |

## Requirements

- Python >= 3.10
- **Linux or macOS.** `milvus-lite` 2.x publishes wheels only for
  `manylinux2014_x86_64/aarch64` and macOS; Windows is not supported natively.
  On Windows, run everything inside WSL (this project was verified on WSL Ubuntu 24.04
  with Python 3.12.3).

Create and activate a virtual environment in this directory:

```bash
# Debian/Ubuntu only: install the venv module if it is missing
sudo apt install -y python3-venv

python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Key packages: `pymilvus==2.6.17`, `milvus-lite==2.5.1`, `cryptography>=50.0.2`.
`milvus-lite 2.5.1` still imports `pkg_resources`, so the requirements pin
`setuptools>=68,<81` (setuptools 81+ removed that module).

### Running under WSL from Windows

The repository is usually mounted at `/mnt/d/...` inside WSL. For example:

```bash
cd /mnt/d/01-liruiyun/github/InfrastructurePlatform/05-AI/02-milvus-v2
sudo apt install -y python3-venv   # only if the venv module is missing
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python milvus-demo.py
```

If creating the venv on the mounted Windows drive fails because of symlink permissions,
use `python3 -m venv --copies .venv` instead. If `ensurepip` is unavailable, either install
`python3-venv` or create the venv with `--without-pip --copies` and bootstrap pip by running
[`get-pip.py`](https://bootstrap.pypa.io/get-pip.py) with the venv's Python.

## Usage

### Basic demo

```bash
python milvus-demo.py
```

Creates `milvus_basic.db`, inserts three 3-dimensional vectors, and prints the top-2 matches:

```text
Search results:
  ID: 1, distance: 1.0000, text: Milvus is a vector database
  ID: 2, distance: 0.9746, text: Vector search enables semantic search
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
python milvus-tool.py show -p milvus_basic.db

# Dump records from a collection (default: 10 rows, first 3 vector dims)
python milvus-tool.py show -c collection_demo -p milvus_basic.db

# Filter, limit, and print full vectors
python milvus-tool.py show -c collection_demo -n 5 -f "id in [1, 2]" --full -p milvus_basic.db
```

## Notes

- Milvus Lite 2.x stores the whole database in a **single `.db` file**; `*.db` and `.venv/`
  are ignored by `.gitignore`.
- Milvus Lite 2.x limitations: supports only `FLAT` and `IVF_FLAT` indexes; no partitions,
  collection aliases, or users/roles/RBAC. It is intended for prototyping and small datasets
  (roughly under a million vectors).
- The `milvus-tool.py` output requires a UTF-8 capable terminal. On WSL this is usually the
  default; set `PYTHONIOENCODING=utf-8` if Chinese characters appear garbled.
- This project targets the **Milvus 2.x Python SDK**; see `../01-milvus-v3` for the 3.x API.
