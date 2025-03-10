from fastapi import APIRouter, HTTPException
from model import MeditionData
from repository import save_medition

router = APIRouter()

@router.post("/medition/")
async def receive_medition(data: MeditionData):
    try:
        inserted_id = await save_medition(data)
        return {"message": "Medición almacenada correctamente", "id": inserted_id}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
