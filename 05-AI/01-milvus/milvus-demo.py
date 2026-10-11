from pymilvus import MilvusClient, DataType, FieldSchema, CollectionSchema

# 1. 连接 Milvus Lite（本地文件，零配置，无需 Docker）
client = MilvusClient("milvus_basic.db")

COLLECTION_NAME = "basic_demo"

# 如果集合已存在，先删除，保证代码可重复运行
if client.has_collection(COLLECTION_NAME):
    client.drop_collection(COLLECTION_NAME)

# 2. 定义 Schema：主键 + 向量 + 标量
schema = CollectionSchema(
    fields=[
        FieldSchema(name="id", dtype=DataType.INT64, is_primary=True),
        FieldSchema(name="vector", dtype=DataType.FLOAT_VECTOR, dim=3),
        FieldSchema(name="text", dtype=DataType.VARCHAR, max_length=512),
    ]
)

# 3. 创建集合（自动创建索引）
client.create_collection(
    collection_name=COLLECTION_NAME,
    schema=schema,
)

# 4. 插入数据
data = [
    {"id": 1, "vector": [0.1, 0.2, 0.3], "text": "Milvus 是一个向量数据库"},
    {"id": 2, "vector": [0.4, 0.5, 0.6], "text": "向量检索用于语义搜索"},
    {"id": 3, "vector": [0.7, 0.8, 0.9], "text": "Embedding 是数据的语义特征"},
]
client.insert(collection_name=COLLECTION_NAME, data=data)

# 5. 加载集合（搜索前必须加载）
client.load_collection(COLLECTION_NAME)

# 6. 向量搜索
search_result = client.search(
    collection_name=COLLECTION_NAME,
    data=[[0.1, 0.2, 0.3]],   # 查询向量
    limit=2,                   # 返回最相似的 2 条
    output_fields=["text"],    # 同时返回标量字段
)

# 7. 打印结果
print("搜索结果：")
for hits in search_result:
    for hit in hits:
        print(f"  ID: {hit['id']}, 距离: {hit['distance']:.4f}, 文本: {hit['entity']['text']}")

client.close()