from fastapi import FastAPI
from controller import router

app = FastAPI(title="API de Sensores")

app.include_router(router)

if __name__ == "__main__":
    import uvicorn
    print("Servidor corriendo en http://localhost:3080")
    uvicorn.run(app, host="localhost", port=3080, log_level="info")    
