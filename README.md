# Mixed-Signal Sensor Conditioning & Embedded Calibration Pipeline

An end-to-end hardware-software co-design project demonstrating analog signal conditioning (**LTspice**), automated DSP/ADC verification (**Python**), and real-time, memory-optimized embedded firmware (**C on ARM Cortex-M0+ / STM32**).

---

## System Architecture

```text
[Raw AC Sensor] ──► [RC Low-Pass Filter] ──► [DC Bias + Op-Amp Buffer] ──► [12-Bit STM32 ADC] ──► [UART CLI & Telemetry]
   (±1.5V, 50Hz)        (fc ≈ 159 Hz)          (0V – 3.3V Safe Window)      (Non-Blocking C)        (Python / Serial)
```

### Repository Structure
```text
├── 1_analog_ltspice/
│   ├── sensor_frontend.asc     # LTspice analog conditioning schematic
│   └── ltspice_waveform.png    # Transient simulation screenshot
├── 2_python_verification/
│   ├── adc_calibration.py      # 2x2 verification dashboard & quantization model
│   ├── simulation_data.txt     # Exported LTspice time-domain dataset
│   └── dashboard_output.png    # 4-panel Matplotlib verification plot
└── 3_stm32_firmware/
    ├── sketch.ino              # Non-blocking STM32 C firmware with UART CLI
    ├── diagram.json            # Wokwi hardware connections & serial config
    └── wokwi_demo.png          # Live simulation & CLI telemetry screenshot
```

---

## Stage 1: Analog Front-End Design (`1_analog_ltspice/`)
* **Anti-Aliasing Low-Pass Filter:** Designed a first-order RC filter ($R = 1\text{ k}\Omega$, $C = 1\ \mu\text{F}$) with a cutoff frequency of $f_c = \frac{1}{2\pi RC} \approx 159.15\text{ Hz}$ to attenuate high-frequency switching noise prior to ADC sampling.
* **DC Level Shifting:** Biased the bipolar AC sensor signal into the positive domain so the waveform stays strictly within safe microcontroller ADC rail limits (preventing pin damage and negative-half clipping).
* **Impedance Isolation:** Implemented a single-supply **Operational Amplifier Voltage Follower** (unity-gain buffer) between the RC filter and ADC input. High input impedance prevents loading the passive filter network, while low output impedance rapidly charges the MCU's internal sample-and-hold capacitor.

*(Add screenshot: `![LTspice Simulation](1_analog_ltspice/ltspice_waveform.png)`)*

---

## Stage 2: Python ADC & DSP Verification (`2_python_verification/`)
Built an automated verification script using `NumPy`, `Pandas`, and `Matplotlib` to ingest exported LTspice waveforms and model digital hardware behavior before writing embedded C code:
* **12-Bit Quantization Modeling:** Mapped continuous analog voltages onto $2^{12} = 4096$ discrete steps ($1\text{ LSB} \approx 1.22\text{ mV}$ at $5\text{V}$ / $0.81\text{ mV}$ at $3.3\text{V}$) and verified the theoretical $\pm 0.5\text{ LSB}$ quantization noise floor.
* **Filter Roll-Off Compensation:** Identified a **~4.6% passband attenuation** at $50\text{ Hz}$ caused by the $159\text{ Hz}$ RC filter roll-off. Verified a digital gain calibration factor ($G = 1.0482$) in Python to restore true sensor amplitude.

*(Add screenshot: `![Python Dashboard](2_python_verification/dashboard_output.png)`)*

---

## Stage 3: Real-Time STM32 Embedded C Firmware (`3_stm32_firmware/`)
Developed and tested non-blocking firmware targeting the **STM32 Nucleo-C031C6 (ARM Cortex-M0+, 32 KB Flash)** in Wokwi:
* **Non-Blocking Multi-Rate Scheduler:** Replaced blocking `delay()` calls with unsigned 32-bit `millis()` rollover-safe timers running three concurrent tasks:
  * **100 Hz (10 ms):** High-speed 12-bit ADC sampling and accumulation.
  * **4 Hz (250 ms):** $25\times$ oversampling average calculation, DC offset removal, gain calibration, and UART telemetry transmission.
  * **1 Hz (500 ms):** Onboard heartbeat LED toggle (`PA5`) confirming deterministic CPU responsiveness.
* **Zero-Heap UART Command Line Interface (CLI):** Implemented a fixed-size `char` buffer parser (`RX_BUFFER_SIZE = 32`) with bounds checking to prevent buffer overflows and avoid heap fragmentation.

### Supported UART Commands (`115200 baud`)

| Command | Action | Example |
| :--- | :--- | :--- |
| `STATUS` | Prints telemetry state, active gain, and DC offset | `STATUS` |
| `GAIN=<val>` | Updates calibration gain (scaled $\times 1000$) | `GAIN=1048` ($\rightarrow 1.048$) |
| `OFFSET=<val>` | Updates DC offset in millivolts ($\text{mV}$) | `OFFSET=1650` ($\rightarrow 1.650\text{ V}$) |
| `PAUSE` / `RESUME` | Toggles live telemetry streaming without halting ADC sampling | `PAUSE` |
| `RESET` | Restores default factory calibration constants | `RESET` |

*(Add screenshot: `![Wokwi STM32 Demo](3_stm32_firmware/wokwi_demo.png)`)*

---

## Key Engineering Trade-Offs & Debugging Highlights
1. **Flash Memory Optimization (Saving >15 KB ROM):** Initial builds using standard `<stdlib.h>` `atof()` overflowed the STM32C031C6's 32 KB Flash by ~15.2 KB due to `strtod` library overhead. Refactored the serial CLI protocol to accept scaled integer payloads (`mV` and $\text{gain} \times 1000$) parsed via lightweight `atoi()`, cutting binary size dramatically.
2. **ARM `newlib-nano` Print Formatting:** Configured UART output formatting to avoid heavy `printf` float overhead while maintaining millimeter-accurate voltage telemetry (`Serial.print(val, 3)`).
3. **Oversampling for Noise Reduction:** Averaging 25 consecutive 12-bit samples per telemetry frame reduces effective quantization noise without requiring external hardware changes.

---

## How to Run
1. **LTspice:** Open `1_analog_ltspice/sensor_frontend.asc` in LTspice XVII/24 and run the `.tran 0.1` simulation.
2. **Python:** Install dependencies (`pip install numpy pandas matplotlib`) and run `python 2_python_verification/adc_calibration.py`.
3. **STM32 Firmware:** Load `3_stm32_firmware/sketch.ino` and `diagram.json` into [Wokwi](https://wokwi.com) (STM32 Nucleo-C031C6 template), start the simulation, and interact via the Serial Monitor at `115200 baud`.