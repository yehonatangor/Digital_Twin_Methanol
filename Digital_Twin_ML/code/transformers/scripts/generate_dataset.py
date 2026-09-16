import csv
import random 

from digital_twin import evaluate_design_point_full

rng = random.Random(42)
prices = [30.0] * 24
rows = []

for _ in range(50):
    inputs = {
        "co2_feed_kg_s": rng.uniform(22.0, 27.0),
        "activity": rng.uniform(0.8, 1.0),
        "recycle_fraction": rng.uniform(0.65, 0.75),
        "reactor_inlet_T_K": rng.uniform(478.0, 488.0),
        "reactor_inlet_P_bar": rng.uniform(76.0, 80.0),
        "n_tubes": rng.randint(5900, 6500),
        "storage_volume_m3": rng.uniform(4500.0, 5500.0),
        "fresh_h2_to_co2_ratio": rng.uniform(2.90, 3.00),
        "tube_inner_diameter_m": rng.uniform(0.033, 0.037),
        "bed_length_m": rng.uniform(6.8, 7.2),
        "n_trays": rng.randint(55, 59),
    }

    result = evaluate_design_point_full(
        prices_USD_per_MWh = prices,
        **inputs,
    )

    rows.append({**inputs, **result})

columns = sorted({key for row in rows for key in row})
with open("design_points.csv", "w", newline = "", encoding = "utf-8") as file:
    writer = csv.DictWriter(file, fieldnames=columns)
    writer.writeheader()
    writer.writerows(rows)

print(f"Saved {len(rows)} designs; {sum(r['ok'] for r in rows)} suceeded")

