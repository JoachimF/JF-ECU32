import matplotlib.pyplot as plt
import numpy as np

class AircraftEngine:
    def __init__(self):
        self.egt = 0  # Exhaust Gas Temperature in degrees Celsius
        self.rpm = 0  # Revolutions Per Minute
        self.fuel_pump = 0  # Fuel pump setting (0 to 100)
        self.egt_rate = 0  # Rate of change of EGT
        self.rpm_rate = 0  # Rate of change of RPM

    def update_sensors(self):
        # Simulate EGT and RPM based on fuel pump setting with time response
        self.egt_rate = 0.2 * (800 * self.fuel_pump / 100 - self.egt)  # Faster response for EGT
        self.rpm_rate = 0.05 * (140000 * self.fuel_pump / 100 - self.rpm)  # Slower response for RPM

        self.egt += self.egt_rate
        self.rpm += self.rpm_rate

        # Simulate EGT peak
        if self.fuel_pump > 80:
            self.egt += 50 * np.sin(self.fuel_pump / 10.0)  # Adding peak effect

        # EGT decreases as RPM approaches final value
        if abs(140000 * self.fuel_pump / 100 - self.rpm) < 5000:
            self.egt -= 0.1 * self.egt  # Decrease EGT

    def simulate(self, steps, fuel_pump_settings):
        egt_values = []
        rpm_values = []
        fuel_pump_values = []

        for i in range(steps):
            self.fuel_pump = fuel_pump_settings[i]

            # Ensure fuel pump setting is within bounds
            if self.fuel_pump < 0:
                self.fuel_pump = 0
            if self.fuel_pump > 100:
                self.fuel_pump = 100

            self.update_sensors()

            egt_values.append(self.egt)
            rpm_values.append(self.rpm)
            fuel_pump_values.append(self.fuel_pump)

        return egt_values, rpm_values, fuel_pump_values

# Create an instance of the AircraftEngine class
engine = AircraftEngine()

# Define the number of simulation steps and fuel pump settings for each step
steps = 100
fuel_pump_settings = [i for i in range(steps // 2)] + [i for i in range(steps // 2, 0, -1)]

# Simulate the engine operation
egt_values, rpm_values, fuel_pump_values = engine.simulate(steps, fuel_pump_settings)

# Plot the results
plt.figure(figsize=(12, 8))

plt.subplot(3, 1, 1)
plt.plot(egt_values, label='EGT (°C)')
plt.ylabel('EGT (°C)')
plt.legend()

plt.subplot(3, 1, 2)
plt.plot(rpm_values, label='RPM')
plt.ylabel('RPM')
plt.legend()

plt.subplot(3, 1, 3)
plt.plot(fuel_pump_values, label='Fuel Pump (%)')
plt.xlabel('Time Step')
plt.ylabel('Fuel Pump (%)')
plt.legend()

plt.tight_layout()
plt.show()
