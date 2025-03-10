from pydantic import BaseModel, Field
from typing import Literal

class MeditionData(BaseModel):
    time: int = Field(..., gt=0, description="Tiempo en microsegundos")
    microName: str = Field(..., min_length=3, description="Nombre del microcontrolador")
    medition: str = Field(..., description="Tipo de medición")
    UnitMessure: str = Field(..., description="Unidad de medida")
    typeMessage: Literal["ANALYTICS", "ALERT", "STATUS"] = Field(..., description="Tipo de mensaje permitido")
