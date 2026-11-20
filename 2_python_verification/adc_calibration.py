import os
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def run_full_pipeline_dashboard(file_name):
    # 1. Load LTspice Data
    script_dir = os.path.dirname(os.path.abspath(__file__))
    file_path = os.path.join(script_dir, file_name)

    if not os.path.exists(file_path):
        print(f" ERROR: Cannot find '{file_name}' in {script_dir}")
        return

    try:
        df = pd.read_csv(file_path, sep="\t", encoding="utf-16")
    except UnicodeDecodeError:
        df = pd.read_csv(file_path, sep="\t", encoding="utf-8")

    time_ms = df.iloc[:, 0].to_numpy() * 1000  # Convert to milliseconds
    analog_voltage = df.iloc[:, 1].to_numpy()

    # ---------------------------------------------------------
    # 2. HARDWARE ADC & QUANTIZATION MATH (12-Bit, 0V - 5.0V)
    # ---------------------------------------------------------
    ADC_BITS = 12
    V_REF = 5.0
    MAX_COUNTS = (2**ADC_BITS) - 1  # 4095
    LSB_VOLTAGE = V_REF / MAX_COUNTS  # ~1.22 mV

    clamped_voltage = np.clip(analog_voltage, 0.0, V_REF)
    adc_counts = np.round((clamped_voltage / V_REF) * MAX_COUNTS).astype(int)
    quantized_voltage = (adc_counts / MAX_COUNTS) * V_REF

    # Quantization error in millivolts
    quant_error_mv = (quantized_voltage - clamped_voltage) * 1000
    half_lsb_mv = (LSB_VOLTAGE / 2) * 1000

    # ---------------------------------------------------------
    # 3. FIRMWARE RECONSTRUCTION & CALIBRATION MATH
    # ---------------------------------------------------------
    DC_OFFSET = 2.5
    CALIBRATION_GAIN = 1.0482  # Restores the ~4.6% 50Hz RC filter loss

    ac_uncalibrated = quantized_voltage - DC_OFFSET
    ac_calibrated = ac_uncalibrated * CALIBRATION_GAIN

    # Zoom window mask (4.5 ms to 5.5 ms around the first peak)
    zoom_mask = (time_ms >= 4.5) & (time_ms <= 5.5)

    # ---------------------------------------------------------
    # 4. PRINT TERMINAL SUMMARY
    # ---------------------------------------------------------
    print("=== END-TO-END MIXED-SIGNAL VERIFICATION ===")
    print(
        f"1. Analog Input Range:   {np.min(analog_voltage):.3f}V to {np.max(analog_voltage):.3f}V (Avg: {np.mean(analog_voltage):.2f}V)"
    )
    print(
        f"2. Raw ADC Counts:       {np.min(adc_counts)} to {np.max(adc_counts)} (1 LSB = {LSB_VOLTAGE*1000:.2f} mV)"
    )
    print(
        f"3. Calibrated Peak:      ±{np.max(ac_calibrated):.3f}V (Target: ±1.500V)"
    )

    # ---------------------------------------------------------
    # 5. BUILD 2x2 DASHBOARD
    # ---------------------------------------------------------
    fig, axs = plt.subplots(2, 2, figsize=(14, 9))
    fig.suptitle(
        "Mixed-Signal Verification Dashboard: LTspice to Firmware Pipeline",
        fontsize=14,
        fontweight="bold",
    )

    # [Top-Left] Plot 1: Buffered Analog Input
    axs[0, 0].plot(
        time_ms, analog_voltage, color="blue", label="V(adc_buffered)"
    )
    axs[0, 0].axhline(
        np.mean(analog_voltage),
        color="red",
        linestyle="--",
        label=f"DC Offset ({np.mean(analog_voltage):.2f}V)",
    )
    axs[0, 0].set_title("1. Analog Stage: Buffered & Offset Signal")
    axs[0, 0].set_ylabel("Voltage (V)")
    axs[0, 0].grid(True, linestyle=":", alpha=0.6)
    axs[0, 0].legend(loc="upper right")

    # [Top-Right] Plot 2: Raw 12-Bit ADC Register Values
    axs[0, 1].plot(
        time_ms, adc_counts, color="purple", label="ADC Register (0-4095)"
    )
    axs[0, 1].set_title("2. Hardware Stage: 12-Bit Digital Counts")
    axs[0, 1].set_ylabel("Counts")
    axs[0, 1].grid(True, linestyle=":", alpha=0.6)
    axs[0, 1].legend(loc="upper right")

    # [Bottom-Left] Plot 3: Zoomed Quantization Steps & Error
    axs[1, 0].plot(
        time_ms[zoom_mask],
        clamped_voltage[zoom_mask],
        color="blue",
        label="Analog Input",
    )
    axs[1, 0].step(
        time_ms[zoom_mask],
        quantized_voltage[zoom_mask],
        where="post",
        color="darkorange",
        label="12-Bit Staircase",
    )
    axs[1, 0].set_title("3. Quantization Zoom (4.5ms - 5.5ms Peak)")
    axs[1, 0].set_xlabel("Time (ms)")
    axs[1, 0].set_ylabel("Voltage (V)")
    axs[1, 0].grid(True, linestyle=":", alpha=0.6)
    axs[1, 0].legend(loc="lower center")

    # [Bottom-Right] Plot 4: Firmware Calibration Output
    axs[1, 1].plot(
        time_ms,
        ac_uncalibrated,
        color="gray",
        linestyle="--",
        label="Uncalibrated (±1.43V)",
    )
    axs[1, 1].plot(
        time_ms,
        ac_calibrated,
        color="green",
        label="Calibrated Output (±1.50V)",
    )
    axs[1, 1].axhline(1.5, color="red", linestyle=":", alpha=0.6)
    axs[1, 1].axhline(-1.5, color="red", linestyle=":", alpha=0.6)
    axs[1, 1].set_title("4. Firmware Stage: Offset Removed & Gain Calibrated")
    axs[1, 1].set_xlabel("Time (ms)")
    axs[1, 1].set_ylabel("Reconstructed AC (V)")
    axs[1, 1].grid(True, linestyle=":", alpha=0.6)
    axs[1, 1].legend(loc="upper right")

    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.show()


if __name__ == "__main__":
    run_full_pipeline_dashboard("simulation_data.txt.txt")