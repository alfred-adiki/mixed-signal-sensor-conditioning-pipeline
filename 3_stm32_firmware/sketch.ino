#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Hardware Pin Definitions (STM32 Nucleo-C031C6)
#define SENSOR_ADC_PIN   A0
#define HEARTBEAT_LED    LED_BUILTIN

// ADC Constants
#define ADC_MAX_COUNTS   4095.0f
#define V_REF            3.3f

// Default Calibration Constants
#define DEFAULT_OFFSET_V 1.65f
#define DEFAULT_GAIN     1.0482f

// Task Timing Intervals (ms)
#define SAMPLE_INTERVAL_MS    10    // 100 Hz ADC sampling
#define TELEMETRY_INTERVAL_MS 250   // 4 Hz UART output
#define HEARTBEAT_INTERVAL_MS 500   // 1 Hz LED blink

// UART Command Buffer Size
#define RX_BUFFER_SIZE        32

// Non-Blocking Timer Trackers
uint32_t last_sample_time    = 0;
uint32_t last_telemetry_time = 0;
uint32_t last_heartbeat_time = 0;

// Shared System State Variables
uint32_t adc_accumulator  = 0;
uint16_t sample_count     = 0;
bool     led_state        = false;
bool     telemetry_active = true;

// Mutable Calibration Parameters
float dc_offset_v      = DEFAULT_OFFSET_V;
float calibration_gain = DEFAULT_GAIN;

// UART Receive Line Buffer
char    rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_index = 0;

void process_uart_command(const char* cmd);

void setup() {
  Serial.setRx(PA3);
  Serial.setTx(PA2);
  Serial.begin(115200);
  
  pinMode(HEARTBEAT_LED, OUTPUT);
  analogReadResolution(12);
  
  Serial.println("=== STM32 CLI Ready (Flash-Optimized) ===");
  Serial.println("Cmds: STATUS | PAUSE | RESUME | RESET | GAIN=<x1000> | OFFSET=<mV>");
}

void loop() {
  uint32_t current_time = millis();

  // TASK 1: Non-Blocking UART Command Listener
  while (Serial.available() > 0) {
    char incoming_char = (char)Serial.read();

    if (incoming_char == '\n' || incoming_char == '\r') {
      if (rx_index > 0) {
        rx_buffer[rx_index] = '\0';
        process_uart_command(rx_buffer);
        rx_index = 0;
      }
    } 
    else if (rx_index < (RX_BUFFER_SIZE - 1)) {
      rx_buffer[rx_index++] = incoming_char;
    }
  }

  // TASK 2: High-Speed ADC Sampling (100 Hz)
  if ((current_time - last_sample_time) >= SAMPLE_INTERVAL_MS) {
    last_sample_time = current_time;
    adc_accumulator += analogRead(SENSOR_ADC_PIN);
    sample_count++;
  }

  // TASK 3: Calibration & Telemetry Output (4 Hz)
  if ((current_time - last_telemetry_time) >= TELEMETRY_INTERVAL_MS) {
    last_telemetry_time = current_time;

    if (sample_count > 0) {
      float avg_raw_adc = (float)adc_accumulator / (float)sample_count;
      uint16_t samples_processed = sample_count;
      adc_accumulator = 0;
      sample_count = 0;

      if (telemetry_active) {
        float measured_v = (avg_raw_adc / ADC_MAX_COUNTS) * V_REF;
        float ac_uncal   = measured_v - dc_offset_v;
        float ac_cal     = ac_uncal * calibration_gain;

        Serial.print("N=");
        Serial.print(samples_processed);
        Serial.print(" | Pin: ");
        Serial.print(measured_v, 3);
        Serial.print("V | Cal AC: ");
        Serial.print(ac_cal, 3);
        Serial.println("V");
      }
    }
  }

  // TASK 4: System Heartbeat LED (1 Hz)
  if ((current_time - last_heartbeat_time) >= HEARTBEAT_INTERVAL_MS) {
    last_heartbeat_time = current_time;
    led_state = !led_state;
    digitalWrite(HEARTBEAT_LED, led_state);
  }
}

// ------------------------------------------------------------------
// FLASH-OPTIMIZED COMMAND PARSER (Uses atoi instead of heavy atof)
// ------------------------------------------------------------------
void process_uart_command(const char* cmd) {
  Serial.print("\n[CMD] -> ");
  Serial.println(cmd);

  if (strcmp(cmd, "STATUS") == 0) {
    Serial.print("State: ");
    Serial.print(telemetry_active ? "RUN" : "PAUSE");
    Serial.print(" | Gain: ");
    Serial.print(calibration_gain, 4);
    Serial.print(" | Offset: ");
    Serial.print(dc_offset_v, 3);
    Serial.println("V\n");
  }
  else if (strcmp(cmd, "PAUSE") == 0) {
    telemetry_active = false;
    Serial.println("PAUSED\n");
  }
  else if (strcmp(cmd, "RESUME") == 0) {
    telemetry_active = true;
    Serial.println("RESUMED\n");
  }
  else if (strcmp(cmd, "RESET") == 0) {
    calibration_gain = DEFAULT_GAIN;
    dc_offset_v      = DEFAULT_OFFSET_V;
    Serial.println("RESET OK\n");
  }
  else if (strncmp(cmd, "GAIN=", 5) == 0) {
    // Expects gain scaled by 1000 (e.g., GAIN=1048 -> 1.048)
    int gain_x1000 = atoi(cmd + 5);
    if (gain_x1000 > 0 && gain_x1000 <= 10000) {
      calibration_gain = (float)gain_x1000 / 1000.0f;
      Serial.print("New Gain: ");
      Serial.println(calibration_gain, 4);
    } else {
      Serial.println("ERR: Use 1 to 10000 (e.g. GAIN=1048)\n");
    }
  }
  else if (strncmp(cmd, "OFFSET=", 7) == 0) {
    // Expects offset in millivolts (e.g., OFFSET=1650 -> 1.650V)
    int offset_mv = atoi(cmd + 7);
    if (offset_mv >= 0 && offset_mv <= 3300) {
      dc_offset_v = (float)offset_mv / 1000.0f;
      Serial.print("New Offset: ");
      Serial.print(dc_offset_v, 3);
      Serial.println("V\n");
    } else {
      Serial.println("ERR: Use 0 to 3300 mV (e.g. OFFSET=1650)\n");
    }
  }
  else {
    Serial.println("ERR: Unknown Cmd\n");
  }
}