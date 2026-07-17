import csv
import random

# Setup product categories
brands = ["APPLE", "SAMSUNG", "SONY", "DELL", "ASUS", "LG", "NVIDIA", "AMD", "HP", "LENOVO"]
categories = ["LAPTOP", "PHONE", "MONITOR", "GPU", "CONSOLE", "TABLET", "WATCH"]
strategies = ["Aggressive", "Safe"]

products = []

print("Generating 50 unique products for inventory.csv...")

# Generate 50 random products
with open('inventory.csv', mode='w', newline='') as inv_file:
    writer = csv.writer(inv_file)
    writer.writerow(["SKU", "Cost", "Price", "Stock", "Strategy"]) # Header
    
    for _ in range(50):
        brand = random.choice(brands)
        category = random.choice(categories)
        model = random.randint(100, 9999)
        sku = f"{brand}-{category}-{model}"
        
        cost = round(random.uniform(200.0, 1500.0), 2)
        # Price is initially 20% to 50% higher than cost
        price = round(cost * random.uniform(1.2, 1.5), 2) 
        stock = random.randint(10, 500)
        strategy = random.choice(strategies)
        
        products.append({"sku": sku, "cost": cost, "price": price})
        writer.writerow([sku, cost, price, stock, strategy])

print("Generating 2,000 high-frequency price events for events.csv...")

# Generate 2,000 random competitor price changes
with open('events.csv', mode='w', newline='') as event_file:
    writer = csv.writer(event_file)
    
    for _ in range(2000):
        # Pick a random product from our generated list
        target_product = random.choice(products)
        
        # Simulate a competitor changing their price (anywhere from 10% below cost to 40% above)
        # This will heavily test our "Safe" strategy failsafes!
        comp_price = round(target_product["cost"] * random.uniform(0.9, 1.4), 2)
        
        writer.writerow([target_product["sku"], comp_price])

print("\n[SUCCESS] Generated inventory.csv (50 items) and events.csv (2000 events).")
print("Ready for High-Frequency Load Testing!")