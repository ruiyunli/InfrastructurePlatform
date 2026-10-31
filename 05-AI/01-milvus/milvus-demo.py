import os
import shutil

from pymilvus import MilvusClient, DataType, FieldSchema, CollectionSchema

DB_PATH = "milvus_basic.db"

# Remove any leftover database from a previous run so the demo starts clean.
# Milvus Lite stores the database as a directory in newer versions and as a
# plain file in older ones, so handle both.
if os.path.isdir(DB_PATH):
    shutil.rmtree(DB_PATH)
elif os.path.exists(DB_PATH):
    os.remove(DB_PATH)

# 1. Connect to Milvus Lite (local file, zero config, no Docker required)
client = MilvusClient(DB_PATH)

COLLECTION_NAME = "collection_demo"

# Drop the collection first if it exists, so the script can be re-run
if client.has_collection(COLLECTION_NAME):
    client.drop_collection(COLLECTION_NAME)

# 2. Define the schema: primary key + vector + scalar
schema = CollectionSchema(
    fields=[
        FieldSchema(name="id", dtype=DataType.INT64, is_primary=True),
        FieldSchema(name="vector", dtype=DataType.FLOAT_VECTOR, dim=3),
        FieldSchema(name="text", dtype=DataType.VARCHAR, max_length=512),
    ]
)

# 3. Create the collection (index is created automatically)
client.create_collection(
    collection_name=COLLECTION_NAME,
    schema=schema,
)

# 4. Insert data
data = [
    {"id": 1, "vector": [0.1, 0.2, 0.3], "text": "Milvus is a vector database"},
    {"id": 2, "vector": [0.4, 0.5, 0.6], "text": "Vector search enables semantic search"},
    {"id": 3, "vector": [0.7, 0.8, 0.9], "text": "Embeddings capture the semantic meaning of data"},
]
client.insert(collection_name=COLLECTION_NAME, data=data)

# 5. Load the collection (required before searching)
client.load_collection(COLLECTION_NAME)

# 6. Vector search
search_result = client.search(
    collection_name=COLLECTION_NAME,
    data=[[0.1, 0.2, 0.3]],   # query vector
    limit=2,                   # return the 2 most similar results
    output_fields=["text"],    # also return the scalar field
)

# 7. Print the results
print("Search results:")
for hits in search_result:
    for hit in hits:
        print(f"  ID: {hit['id']}, distance: {hit['distance']:.4f}, text: {hit['entity']['text']}")

client.close()
