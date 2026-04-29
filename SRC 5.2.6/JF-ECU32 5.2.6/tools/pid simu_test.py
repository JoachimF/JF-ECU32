import matplotlib.pyplot as plt
import numpy as np

class AircraftEngine:
    def __init__(self):
        self.egt = 500  # Exhaust Gas Temperature in degrees Celsius (minimum 500)
        self.rpm = 0  # Revolutions Per Minute (starting at 40000 when fuel pump is at 8%)
        self.fuel_pump = 8  # Fuel pump setting (minimum 8 to keep the engine running)
        self.egt_rate = 0  # Rate of change of EGT
        self.rpm_rate = 0  # Rate of change of RPM

    def update_sensors(self):
        # Simulate EGT and RPM based on fuel pump setting with time response
        self.egt_rate = 0.2 * (400 * self.fuel_pump / 100 - self.egt+400)  # Faster response for EGT
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
            # Ensure fuel pump setting is within bounds (minimum 8 to keep the engine running)
            if self.fuel_pump < 8:
                print(f"Engine stopped at step {i+1} due to fuel pump setting below minimum threshold.")
                break
            if self.fuel_pump > 100:
                self.fuel_pump = 100

            self.update_sensors()
            egt_values.append(self.egt)
            rpm_values.append(self.rpm)
            fuel_pump_values.append(self.fuel_pump)

        return egt_values, rpm_values, fuel_pump_values

# PID tuning using Ziegler-Nichols method with ramp input signal
def ziegler_nichols_tuning(engine, steps, ramp_rate):
    # Generate ramp input signal for fuel pump settings starting at 8%
    fuel_pump_settings = [min(8 + i * ramp_rate, 100) for i in range(steps)]

    # Simulate the engine operation with ramp input signal
    egt_values, rpm_values, _ = engine.simulate(steps, fuel_pump_settings)

    # Find the ultimate gain Ku and period Tu from the response
    if len(rpm_values) > 0:
        Ku = max(rpm_values) / (np.pi / 2)
        Tu = steps / (2 * np.pi)

        # Calculate PID coefficients using Ziegler-Nichols method
        Kp = 0.6 * Ku
        Ki = 1.2 * Ku / Tu
        Kd = 3 * Ku * Tu / 40

        return Kp, Ki, Kd
    else:
        return None, None, None

# Create an instance of the AircraftEngine class
engine = AircraftEngine()

# Define the number of simulation steps and ramp rate for fuel pump settings
steps = 200
ramp_rate = 1

# Perform PID tuning using Ziegler-Nichols method with ramp input signal starting at 8%
Kp, Ki, Kd = ziegler_nichols_tuning(engine, steps, ramp_rate)

if Kp is not None and Ki is not None and Kd is not None:
    # Print the PID coefficients in the console
    print(f"PID coefficients: Kp={Kp}, Ki={Ki}, Kd={Kd}")

    # Define the number of simulation steps and fuel pump settings for each step starting at 8%
    fuel_pump_settings = [max(8 + i,8) for i in range(steps//2)] + [max(8 + i,8) for i in range(steps//2, 0, -1)]

    # Simulate the engine operation with the original settings
    egt_values, rpm_values, fuel_pump_values = engine.simulate(steps, fuel_pump_settings)

    # Print the results in the console
    print("\nSimulation Results:")
    print("Step\tFuel Pump (%)\tEGT (°C)\tRPM")
    for i in range(len(fuel_pump_values)):
        print(f"{i+1}\t{fuel_pump_values[i]:.2f}\t\t{egt_values[i]:.2f}\t\t{rpm_values[i]:.2f}")

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
else:
    print("PID tuning failed due to insufficient data.")
