from motor.motor_asyncio import AsyncIOMotorClient
from model import MeditionData

MONGO_URI = "mongodb://localhost:27017"
DATABASE_NAME = "sensores"
COLLECTION_NAME = "mediciones"

client = AsyncIOMotorClient(MONGO_URI)
db = client[DATABASE_NAME]
collection = db[COLLECTION_NAME]

async def save_medition(data: MeditionData):
    result = await collection.insert_one(data.dict())
    return str(result.inserted_id)
