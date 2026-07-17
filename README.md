# ⚡ QuantumPrice

**A High-Frequency Multi-Threaded Algorithmic Pricing & Arbitrage Engine**

QuantumPrice is a highly decoupled, distributed microservices architecture designed to simulate a high-frequency algorithmic pricing engine. Similar to how e-commerce giants or financial institutions dynamically adjust prices and execute trades millions of times a day, this engine processes a relentless firehose of simulated market fluctuations. 

It recalculates platform prices in **microseconds** to undercut competitors, while strictly ensuring prices never drop below a mathematically protected profit boundary.

---

## 🏗️ System Architecture

To achieve microsecond-level updates while keeping the application web-accessible, the system operates on a 3-tier decoupled infrastructure:

1. **The Fast Path (C++ Core Engine):** A highly concurrent, in-memory daemon utilizing a Producer-Consumer thread pool. It holds the product registry, evaluates competitor strategies via polymorphism (Strategy Pattern), and computes profit boundaries.
2. **The State & Sync Layer (Python + Redis):** An API Gateway (FastAPI) and an ephemeral database (Redis). It uses UDP sockets to catch C++ calculations in microseconds, saves them to Redis, and broadcasts them via Redis Pub/Sub.
3. **The Presentation Layer (React):** A high-density "Level 2 Market Data" dashboard. It connects via WebSockets to ingest live data streams, visually rendering competitive price movements (flashing green/red) in real-time.

---

## 🛠️ Tech Stack

*   **Core Engine:** C++17 (Multithreading, Mutex/Condition Variables, UDP Networking, OOP Polymorphism)
*   **API Gateway:** Python 3 (FastAPI, Subprocess OS Management)
*   **Cache & Pub/Sub:** Redis (In-Memory Database)
*   **Frontend Dashboard:** React.js, Tailwind CSS, WebSockets (HTML5)
*   **Data Generation:** Python

---

## ⚙️ Prerequisites

Before you begin, ensure you have the following installed:
*   **Git:** To clone the repository.
*   **C++ Compiler:** `g++` (MinGW-w64 for Windows) with C++17 support.
*   **Python 3.8+:** For the API Gateway and Data Generator.
*   **Docker:** To easily run the Redis caching server.

---

## 🚀 Installation & Setup

**1. Clone the Repository**
```bash
git clone https://github.com/yourusername/QuantumPrice.git
cd QuantumPrice
```

**2. Start the Redis Server (via Docker)**
You need a running Redis instance for the system's global whiteboard.
```bash
docker run -p 6379:6379 -d redis
```

**3. Install Python Dependencies**
Install the required libraries for the FastAPI Gateway.
```bash
pip install fastapi uvicorn redis
```

---

## 🎮 How to Run and Test the Code

**Step 1: Generate High-Frequency Mock Data**
We need to create the massive data pool for the engine to process (50 SKUs and 2,000 market fluctuations).
```bash
python script.py
```
*(This generates `inventory.csv` and `events.csv` in your project folder).*

**Step 2: Compile the C++ Core Engine**
Turn the C++ code into an executable file. 
*Note for Windows users: The `-lws2_32` flag is required to link the Windows Socket API.*
```bash
g++ -std=c++17 -pthread PricingEngine_Phase11.cpp -o engine.exe -lws2_32
```
*(If you are on Linux/macOS, omit the `-lws2_32` flag and output to `-o engine`).*

**Step 3: Boot the Python API Gateway**
Turn on the "Nervous System" that bridges C++ to the web browser.
```bash
python Bridge_Phase5.py
```
*You should see a message indicating the Gateway has started and is listening on UDP 5000.*

**Step 4: Launch the Presentation Dashboard**
1. Open your File Explorer and double-click `index.html` to open it in Chrome, Edge, or Safari.
2. Look at the top right of the dashboard: The status indicator should pulse **CONNECTED - LIVE**.
3. All 50 generated products will instantly populate the screen.

**Step 5: Ignite the Engine (The Grand Finale)**
1. Click the green **▶ RUN SYSTEM** button inside your web dashboard.
2. **Watch the screen:** Python silently boots `engine.exe` in the background. You will see rows flashing green (undercuts) and red (profit failsafes hit) as your C++ algorithms battle 2,000 competitor price changes in real-time.
3. The UI will dynamically calculate precise Net Margins and Customer Savings on the fly.
4. Click **■ HALT** to safely terminate the C++ process when finished.

---

## 📁 Project Structure & File Summary

| File | Role | Description |
| :--- | :--- | :--- |
| `PricingEngine_Phase11.cpp` | **The Brain** | High-performance C++ backend. Uses an O(1) Hash Map (`SKURegistry`) and the Strategy Pattern (`Aggressive` vs `Safe`) to calculate prices. Utilizes a Producer-Consumer thread pool with strict `std::mutex` locks to prevent race conditions. |
| `Bridge_Phase5.py` | **The Nervous System** | FastAPI Gateway. Runs a background UDP listener to catch C++ updates, pushes them to Redis, and broadcasts via WebSockets. Manages OS-level `subprocess` execution to start/stop the C++ engine. |
| `index.html` | **The Face** | React/Tailwind frontend. A Level 2 Market Data UI. Connects to the WebSocket to render real-time DOM updates and visual flash indicators without page reloads. |
| `script.py` | **The Simulator** | Generates the foundational state (50 unique items) and the heavy-load firehose (2,000 simulated competitor price drops) to stress-test concurrency. |
| `inventory.csv` | **State Data** | Read by C++ on boot-up to populate its in-memory Hash Map. |
| `events.csv` | **The Firehose** | Read by the C++ main thread as fast as possible to feed worker threads. |

---

## 🧠 Key Engineering Concepts Demonstrated

*   **Concurrency & Thread Safety:** Fine-grained mutex locking (`std::mutex`) inside individual product classes rather than locking the entire registry, preventing thread bottlenecking.
*   **Time Complexity Optimization:** O(1) query times using custom Hash Maps for instant product location during a market event.
*   **Design Patterns:** Implementation of the **Strategy Pattern** to hot-swap pricing logic at runtime, ensuring Open-Closed Principle compliance.
*   **Inter-Process Communication (IPC):** Microsecond data handoffs between C++ and Python using lock-free UDP sockets.
*   **Real-Time Data Pipelines:** Implementation of WebSockets paired with Redis Pub/Sub to bypass HTTP polling limitations.
