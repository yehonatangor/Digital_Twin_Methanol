import pandas as pd

from sklearn.ensemble import RandomForestRegressor

from sklearn.model_selection import train_test_split

from sklearn.metrics import mean_absolute_error

data = pd.read_csv("design_points.csv")

features= [
    "co2_feed_kg_s", "activity", "recycle_fraction",
    "reactor_inlet_T_K", "reactor_inlet_P_bar",
    "n_tubes", "storage_volume_m3", "fresh_h2_to_co2_ratio",
    "tube_inner_diameter_m", "bed_length_m", "n_trays",
]

good = data[data["ok"]].dropna(subset=["unit_cost_full_USD_per_t"])
X_train, X_test, y_train, y_test = train_test_split(
    good[features],
    good["unit_cost_full_USD_per_t"],
    test_size = 0.2,
    random_state=42,
)

model = RandomForestRegressor(n_estimators=200, random_state=42)
model.fit(X_train, y_train)
predictions = model.predict(X_test)

print("Cost MAE ($/tonne):", mean_absolute_error(y_test,predictions))