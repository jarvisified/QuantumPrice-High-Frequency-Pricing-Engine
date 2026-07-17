#include <iostream>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace std;

// Locks and Queue variables
mutex consoleLock;
mutex qLock;
condition_variable cv;
queue<pair<string, double>> workQueue;
bool isDone = false;

// IPC Network Variables
#ifdef _WIN32
SOCKET udpSocket;
#else
int udpSocket;
#endif
sockaddr_in pythonServer;

void setupIPC()
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    pythonServer.sin_family = AF_INET;
    pythonServer.sin_port = htons(5000);
    pythonServer.sin_addr.s_addr = inet_addr("127.0.0.1");
}

void sendToPython(string sku, double price, double oldPrice, double compPrice, double cost)
{
    // New dense payload: SKU, NewPrice, OldPrice, CompPrice, Cost
    string msg = sku + "," + to_string(price) + "," + to_string(oldPrice) + "," + to_string(compPrice) + "," + to_string(cost);
    sendto(udpSocket, msg.c_str(), msg.length(), 0, (struct sockaddr *)&pythonServer, sizeof(pythonServer));
}

class Product;

class PricingStrategy
{
public:
    virtual ~PricingStrategy() = default;
    virtual double calcPrice(const Product &prod, double compPrice) = 0;
};

class Product
{
private:
    string sku;
    double cost;
    double price;
    int stock;
    PricingStrategy *strategy;
    mutex pLock;

public:
    Product(string s, double c, double p, int inv, PricingStrategy *strat = nullptr)
    {
        sku = s;
        cost = c;
        price = p;
        stock = inv;
        strategy = strat;
    }

    void setPrice(double newPrice, double compPrice)
    {
        lock_guard<mutex> lock(pLock);
        double oldPrice = price; // Capture the old price before changing it

        if (newPrice < cost)
        {
            price = cost;
        }
        else
        {
            price = newPrice;
        }

        // Broadcast the full context to the UI
        sendToPython(sku, price, oldPrice, compPrice, cost);
    }

    void setStrategy(PricingStrategy *newStrat)
    {
        lock_guard<mutex> lock(pLock);
        strategy = newStrat;
    }

    PricingStrategy *getStrategy() const { return strategy; }
    string getSku() const { return sku; }
    double getPrice() const { return price; }
    double getCost() const { return cost; }
};

class AggressiveStrategy : public PricingStrategy
{
public:
    double calcPrice(const Product &prod, double compPrice) override
    {
        return compPrice * 0.99;
    }
};

class SafeStrategy : public PricingStrategy
{
public:
    double calcPrice(const Product &prod, double compPrice) override
    {
        double minPrice = prod.getCost() * 1.20;
        if (compPrice < minPrice)
            return minPrice;
        return compPrice - 0.01;
    }
};

class SKURegistry
{
private:
    unordered_map<string, Product *> db;
    mutex mapLock; // Protects the Hash Map from crashing during ADD/REMOVE

public:
    ~SKURegistry()
    {
        for (auto const &[sku, prod] : db)
        {
            delete prod;
        }
    }

    void addProduct(Product *p)
    {
        lock_guard<mutex> lock(mapLock); // Lock the parking lot
        db[p->getSku()] = p;
    }

    void removeProduct(string sku)
    {
        lock_guard<mutex> lock(mapLock); // Lock the parking lot
        if (db.find(sku) != db.end())
        {
            delete db[sku]; // Safely free the physical RAM
            db.erase(sku);  // Erase the pointer from the Map

            lock_guard<mutex> cLock(consoleLock);
            cout << "\n[API COMMAND] Successfully REMOVED " << sku << " from engine memory!\n\n";
        }
    }

    void updateStrategy(string sku, PricingStrategy *newStrat, string stratName)
    {
        lock_guard<mutex> lock(mapLock);
        if (db.find(sku) != db.end())
        {
            db[sku]->setStrategy(newStrat);
            lock_guard<mutex> cLock(consoleLock);
            cout << "\n[API COMMAND] Successfully hot-swapped " << sku << " to " << stratName << " Strategy!\n\n";
        }
    }

    void processEvent(string sku, double compPrice)
    {
        Product *p = nullptr;

        {
            lock_guard<mutex> lock(mapLock);
            if (db.find(sku) != db.end())
            {
                p = db[sku];
            }
        }

        if (p != nullptr)
        {
            PricingStrategy *strat = p->getStrategy();
            if (strat != nullptr)
            {
                double newPrice = strat->calcPrice(*p, compPrice);
                p->setPrice(newPrice, compPrice); // Pass competitor price down
            }
        }
    }
};

vector<string> parseCSV(const string &line)
{
    vector<string> cols;
    stringstream ss(line);
    string item;
    while (getline(ss, item, ','))
    {
        cols.push_back(item);
    }
    return cols;
}

void apiListenerThread(SKURegistry *registry, PricingStrategy *safeStrat, PricingStrategy *aggStrat)
{
#ifdef _WIN32
    SOCKET recvSocket = socket(AF_INET, SOCK_DGRAM, 0);
#else
    int recvSocket = socket(AF_INET, SOCK_DGRAM, 0);
#endif
    sockaddr_in recvAddr;
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(5001);
    recvAddr.sin_addr.s_addr = INADDR_ANY;

    bind(recvSocket, (sockaddr *)&recvAddr, sizeof(recvAddr));

    char buffer[1024];
    while (true)
    {
        int bytes = recvfrom(recvSocket, buffer, 1024, 0, NULL, NULL);
        if (bytes > 0)
        {
            buffer[bytes] = '\0';
            string msg(buffer);
            vector<string> parts = parseCSV(msg);

            if (parts[0] == "STRATEGY" && parts.size() == 3)
            {
                PricingStrategy *newStrat = (parts[2] == "Safe") ? safeStrat : aggStrat;
                registry->updateStrategy(parts[1], newStrat, parts[2]);
            }
            else if (parts[0] == "ADD" && parts.size() == 6)
            {
                PricingStrategy *strat = (parts[5] == "Safe") ? safeStrat : aggStrat;
                Product *p = new Product(parts[1], stod(parts[2]), stod(parts[3]), stoi(parts[4]), strat);
                registry->addProduct(p);

                lock_guard<mutex> cLock(consoleLock);
                cout << "\n[API COMMAND] Successfully ADDED " << parts[1] << " to live memory!\n\n";
            }
            else if (parts[0] == "REMOVE" && parts.size() == 2)
            {
                registry->removeProduct(parts[1]);
            }
        }
    }
}

int main()
{
    setupIPC();

    AggressiveStrategy aggressive;
    SafeStrategy safe;
    SKURegistry registry;

    thread apiThread(apiListenerThread, &registry, &safe, &aggressive);
    apiThread.detach();

    ifstream dbReader("inventory.csv");
    if (!dbReader.is_open())
        return 1;

    string line;
    getline(dbReader, line);

    while (getline(dbReader, line))
    {
        vector<string> data = parseCSV(line);
        if (data.size() == 5)
        {
            PricingStrategy *strat = (data[4] == "Safe") ? (PricingStrategy *)&safe : (PricingStrategy *)&aggressive;
            Product *p = new Product(data[0], stod(data[1]), stod(data[2]), stoi(data[3]), strat);
            registry.addProduct(p);
        }
    }
    dbReader.close();

    vector<thread> workers;
    int numWorkers = 4;

    for (int i = 0; i < numWorkers; i++)
    {
        workers.push_back(thread([&registry]()
                                 {
            while (true) {
                pair<string, double> task;
                {
                    unique_lock<mutex> lock(qLock); 
                    cv.wait(lock, [] { return !workQueue.empty() || isDone; });
                    
                    if (workQueue.empty() && isDone) return;
                    
                    task = workQueue.front();
                    workQueue.pop();
                } 
                registry.processEvent(task.first, task.second);
            } }));
    }

    int eventCount = 0;
    auto start = chrono::high_resolution_clock::now();

    ifstream eventReader("events.csv");
    if (!eventReader.is_open())
        return 1;

    while (getline(eventReader, line))
    {
        vector<string> data = parseCSV(line);
        if (data.size() == 2)
        {
            string targetSku = data[0];
            double compPrice = stod(data[1]);

            eventCount++;

            {
                lock_guard<mutex> lock(qLock);
                workQueue.push({targetSku, compPrice});
            }
            cv.notify_one();

            this_thread::sleep_for(chrono::milliseconds(200));
        }
    }
    eventReader.close();

    {
        lock_guard<mutex> lock(qLock);
        isDone = true;
    }
    cv.notify_all();

    for (auto &w : workers)
    {
        if (w.joinable())
            w.join();
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end - start).count();

    {
        lock_guard<mutex> cLock(consoleLock);
        cout << "\n[BENCHMARK] Processed " << eventCount << " events in " << duration << " microseconds.\n";
    }

#ifdef _WIN32
    closesocket(udpSocket);
    WSACleanup();
#endif

    return 0;
}