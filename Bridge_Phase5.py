import socket
import threading
import redis
import asyncio
import subprocess
from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

# Network Config
UDP_LISTEN_PORT = 5000  
UDP_SEND_PORT = 5001    
UDP_IP = "127.0.0.1"

# Connect to Redis Database
r = redis.Redis(host='localhost', port=6379, decode_responses=True)

# Global variable to track the C++ operating system process
engine_process = None

# --- BACKGROUND LISTENER ---
def udp_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_LISTEN_PORT))
    print(f"[*] Background Listener active on UDP {UDP_LISTEN_PORT}...")
    
    while True:
        data, _ = sock.recvfrom(1024)
        message = data.decode('utf-8')
        try:
            parts = message.split(',')
            if len(parts) >= 5:
                # Catch the new 5-part data format
                sku = parts[0]
                r.set(sku, message)
                r.publish("price_updates", message)
        except Exception:
            pass

# --- LIFESPAN MANAGER (Starts the background thread safely) ---
@asynccontextmanager
async def lifespan(app: FastAPI):
    print("[*] Wiping old Redis cache to prevent data format collisions...")
    r.flushdb()
    # Turn on the listener!
    threading.Thread(target=udp_listener, daemon=True).start()
    yield

# Initialize FastAPI
app = FastAPI(title="QuantumPrice API Gateway", lifespan=lifespan)

# Add CORS so our React Dashboard can send POST commands safely
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# --- DATA MODELS ---
class StrategyUpdate(BaseModel):
    strategy: str

class ProductCreate(BaseModel):
    sku: str
    cost: float
    price: float
    stock: int
    strategy: str

# --- ROUTES ---
@app.get("/catalog")
def get_catalog():
    keys = r.keys()
    if not keys:
        return {"message": "Cache empty."}
    prices = r.mget(keys)
    return dict(zip(keys, prices))

@app.post("/sku")
def add_product(payload: ProductCreate):
    if payload.strategy not in ["Safe", "Aggressive"]:
        raise HTTPException(status_code=400, detail="Invalid strategy.")
    
    command = f"ADD,{payload.sku},{payload.cost},{payload.price},{payload.stock},{payload.strategy}"
    
    out_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    out_sock.sendto(command.encode('utf-8'), (UDP_IP, UDP_SEND_PORT))
    out_sock.close()
    
    initial_data = f"{payload.sku},{payload.price},{payload.price},{payload.price},{payload.cost}"
    r.set(payload.sku, initial_data)
    
    return {"status": "success", "command": command}

@app.put("/sku/{sku}/strategy")
def update_strategy(sku: str, payload: StrategyUpdate):
    if payload.strategy not in ["Safe", "Aggressive"]:
        raise HTTPException(status_code=400, detail="Invalid strategy.")
    
    command = f"STRATEGY,{sku},{payload.strategy}"
    out_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    out_sock.sendto(command.encode('utf-8'), (UDP_IP, UDP_SEND_PORT))
    out_sock.close()
    
    return {"status": "success", "command": command}

@app.delete("/sku/{sku}")
def delete_product(sku: str):
    command = f"REMOVE,{sku}"
    out_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    out_sock.sendto(command.encode('utf-8'), (UDP_IP, UDP_SEND_PORT))
    out_sock.close()
    
    r.delete(sku)
    
    return {"status": "success", "command": command}

@app.post("/engine/start")
def start_engine():
    global engine_process
    if engine_process is None or engine_process.poll() is not None:
        print("[*] Dashboard requested Engine START. Booting C++ executable...")
        engine_process = subprocess.Popen(["engine.exe"])
        return {"status": "Engine Started"}
    return {"status": "Engine is already running"}

@app.post("/engine/stop")
def stop_engine():
    global engine_process
    if engine_process and engine_process.poll() is None:
        print("[*] Dashboard requested Engine STOP. Terminating C++ executable...")
        engine_process.terminate()
        engine_process = None
        return {"status": "Engine Stopped"}
    return {"status": "Engine is not running"}

@app.websocket("/ws/stream")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    pubsub = r.pubsub()
    pubsub.subscribe("price_updates")
    print("[*] Client connected to live stream!")
    
    try:
        while True:
            message = pubsub.get_message(ignore_subscribe_messages=True)
            if message:
                await websocket.send_text(message['data'])
            await asyncio.sleep(0.01)
    except WebSocketDisconnect:
        print("[*] Client disconnected.")
    except asyncio.CancelledError:
        print("[*] Server shutting down. Closing WebSocket cleanly.")
        # REMOVED the 'raise' keyword from here!
    finally:
        pubsub.unsubscribe("price_updates")

if __name__ == "__main__":
    import uvicorn
    import sys
    try:
        print("[*] Starting QuantumPrice Gateway...")
        uvicorn.run(app, host="127.0.0.1", port=8000)
    except KeyboardInterrupt:
        print("\n[*] Server safely powered down. Goodbye!")
        sys.exit(0)