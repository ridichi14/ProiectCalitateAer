/**
 * RAK Air Quality Station v1.0 - MAIN FILE
 * RAK4631 + RAK19001 + RAK12039 (UART on IO Slot B)
 * 
 * UART: Serial1 (TXD1=pin16, RXD1=pin15) at 9600 baud
 * I2C: OLED SSD1306 at 0x3C or 0x3D
 * LoRaWAN: OTAA on EU868
 */

#include "main.h"

// ============ GLOBAL OBJECTS ============
Adafruit_PM25AQI aqi;
particle_data_s particle_data;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

SemaphoreHandle_t taskEvent = NULL;
SoftwareTimer taskWakeupTimer;
uint8_t rcvdLoRaData[256];
uint8_t rcvdDataLen = 0;
uint8_t eventType = -1;

bool sensorReady = false;
unsigned long sensorStartTime = 0;

// ============ PERIODIC WAKEUP ============
void periodicWakeup(TimerHandle_t unused) {
  digitalWrite(LED_BUILTIN, HIGH);
  eventType = 1;
  xSemaphoreGiveFromISR(taskEvent, pdFALSE);
}

// ============ I2C SCANNER ============
void i2c_scan_and_print() {
#ifndef MAX_SAVE
  Serial.println("\n📡 Scanning I2C bus for OLED...");
#endif
  
  Wire.begin();
  bool found = false;
  
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
#ifndef MAX_SAVE
      Serial.printf("  ✓ I2C device at 0x%02X", addr);
      if (addr == 0x3C || addr == 0x3D) {
        Serial.print(" (OLED)");
      }
      Serial.println();
#endif
      found = true;
    }
  }
  
  if (!found) {
#ifndef MAX_SAVE
    Serial.println("  ℹ No I2C devices found (OLED may not be present)");
#endif
  }
}

// ============ PM2.5 SENSOR SETUP ============
bool setup_particle_sensor(void) {
#ifndef MAX_SAVE
  Serial.println("\n💨 Initializing RAK12039 PM2.5 sensor (UART)...");
  Serial.println("   IO Slot B: TXD1=pin16, RXD1=pin15 @ 9600 baud");
#endif
  
  // Initialize UART for PM sensor
  Serial1.begin(9600);
  delay(1000);
  
  // Clear any garbage in buffer
  while(Serial1.available()) {
    Serial1.read();
  }
  
#ifndef MAX_SAVE
  Serial.println("\n=== UART DIAGNOSTIC TEST (5 sec) ===");
#endif
  
  // Test 1: Listen passively for data
  unsigned long startTest = millis();
  int bytesReceived = 0;
  
  while(millis() - startTest < 5000) {
    if (Serial1.available()) {
      uint8_t b = Serial1.read();
      bytesReceived++;
#ifndef MAX_SAVE
      Serial.printf("%02X ", b);
      if (bytesReceived % 16 == 0) Serial.println();
#endif
    }
    delay(10);
  }
  
#ifndef MAX_SAVE
  Serial.printf("\nPassive receive: %d bytes\n", bytesReceived);
#endif
  
  // If no data, try wake-up command
  if (bytesReceived == 0) {
#ifndef MAX_SAVE
    Serial.println("\n⚠ No passive data. Sending wake-up command...");
#endif
    
    // Plantower PMS5003 wake command
    uint8_t wakeCmd[] = {0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74};
    Serial1.write(wakeCmd, 7);
    delay(2000);
    
    bytesReceived = 0;
    startTest = millis();
    while(millis() - startTest < 3000) {
      if (Serial1.available()) {
        uint8_t b = Serial1.read();
        bytesReceived++;
#ifndef MAX_SAVE
        Serial.printf("%02X ", b);
        if (bytesReceived % 16 == 0) Serial.println();
#endif
      }
      delay(10);
    }
#ifndef MAX_SAVE
    Serial.printf("\nAfter wake command: %d bytes\n", bytesReceived);
#endif
  }
  
#ifndef MAX_SAVE
  Serial.println("=== END DIAGNOSTIC ===\n");
#endif
  
  // Try Adafruit library
  bool ok = aqi.begin_UART(&Serial1);
  
  if (!ok) {
#ifndef MAX_SAVE
    Serial.println("❌ PM2.5 sensor NOT found!");
    Serial.println("Troubleshooting:");
    Serial.println("  1. TX/RX wires reversed?");
    Serial.println("  2. Baud rate wrong? (should be 9600)");
    Serial.println("  3. Sensor in sleep mode?");
    Serial.println("  4. Defective sensor?");
#endif
    
    delay(1000);
    // Retry once
    ok = aqi.begin_UART(&Serial1);
    if (!ok) {
#ifndef MAX_SAVE
      Serial.println("Retry failed - giving up sensor init");
#endif
      return false;
    }
  }
  
#ifndef MAX_SAVE
  Serial.println("✓ PM2.5 sensor initialized!");
  Serial.println("  Warming up... (60 sec)");
#endif
  
  sensorStartTime = millis();
  sensorReady = false;
  
  return true;
}

// ============ READ PM2.5 DATA ============
bool read_particle_data(void) {
  // Wait for warm-up period
  if (!sensorReady) {
    unsigned long elapsed = millis() - sensorStartTime;
    if (elapsed < 60000) {
#ifndef MAX_SAVE
      Serial.printf("⏳ Warming up... %lu/60 sec\n", elapsed/1000);
#endif
      return false;
    }
    sensorReady = true;
#ifndef MAX_SAVE
    Serial.println("✓ Sensor ready for measurements!");
#endif
  }
  
  PM25_AQI_Data data;
  
  delay(1000);  // Give sensor time to prepare data
  
#ifndef MAX_SAVE
  Serial.printf("Reading sensor... (UART available: %d bytes)\n", Serial1.available());
#endif
  
  // If no data waiting, wait more
  if (Serial1.available() == 0) {
#ifndef MAX_SAVE
    Serial.println("No data yet, waiting 2 sec...");
#endif
    delay(2000);
  }
  
  // Attempt to read
  if (!aqi.read(&data)) {
#ifndef MAX_SAVE
    Serial.println("❌ Could not read from PM2.5 sensor");
    if (Serial1.available() > 0) {
      Serial.print("Raw UART data: ");
      int count = 0;
      while(Serial1.available() && count < 20) {
        Serial.printf("%02X ", Serial1.read());
        count++;
      }
      Serial.println();
    }
#endif
    return false;
  }
  
  // Validate data (all zeros might indicate warming up)
  if (data.pm10_standard == 0 && data.pm25_standard == 0 && data.pm100_standard == 0) {
#ifndef MAX_SAVE
    Serial.println("⚠ All readings are 0 - sensor still stabilizing");
#endif
  }
  
  // Copy to global struct
  particle_data.pm10_standard = data.pm10_standard;
  particle_data.pm25_standard = data.pm25_standard;
  particle_data.pm100_standard = data.pm100_standard;
  particle_data.pm10_env = data.pm10_env;
  particle_data.pm25_env = data.pm25_env;
  particle_data.pm100_env = data.pm100_env;
  particle_data.particles_03um = data.particles_03um;
  particle_data.particles_05um = data.particles_05um;
  particle_data.particles_10um = data.particles_10um;
  particle_data.particles_25um = data.particles_25um;
  particle_data.particles_50um = data.particles_50um;
  particle_data.particles_100um = data.particles_100um;

#ifndef MAX_SAVE
  Serial.println("✓ Data read successfully!");
  Serial.println("---------------------------------------");
  Serial.printf("PM 1.0: %d μg/m³\n", data.pm10_standard);
  Serial.printf("PM 2.5: %d μg/m³\n", data.pm25_standard);
  Serial.printf("PM 10 : %d μg/m³\n", data.pm100_standard);
  Serial.println("---------------------------------------");
#endif
  
  return true;
}

// ============ OLED DISPLAY SETUP ============
void setup_oled_display() {
#ifndef MAX_SAVE
  Serial.println("📺 Setting up OLED display...");
#endif
  
  Wire.begin();
  bool ok = false;
  
  // Try both common I2C addresses
  for (int attempt = 0; attempt < 2; attempt++) {
    uint8_t addr = (attempt == 0) ? 0x3C : 0x3D;
    if (display.begin(SSD1306_SWITCHCAPVCC, addr)) {
      ok = true;
#ifndef MAX_SAVE
      Serial.printf("✓ OLED initialized at 0x%02X\n", addr);
#endif
      break;
    }
  }

  if (!ok) {
#ifndef MAX_SAVE
    Serial.println("⚠ OLED not found - continuing without display");
#endif
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("RAK Air Quality");
  display.println("Station v1.0");
  display.println("Initializing...");
  display.display();
}

// ============ DISPLAY PARTICLE DATA ============
void display_particle_data() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  display.println("Air Quality Data:");
  display.println("");
  
  display.print("PM1.0: ");
  display.print(particle_data.pm10_standard);
  display.println(" ug/m3");
  
  display.print("PM2.5: ");
  display.print(particle_data.pm25_standard);
  display.println(" ug/m3");
  
  display.print("PM10 : ");
  display.print(particle_data.pm100_standard);
  display.println(" ug/m3");
  
  display.println("");
  
  // Air quality indicator
  if (particle_data.pm25_standard < 12) {
    display.println("Quality: GOOD");
  } else if (particle_data.pm25_standard < 35) {
    display.println("Quality: MODERATE");
  } else {
    display.println("Quality: POOR");
  }
  
  display.display();
}

// ============ DISPLAY STATUS MESSAGE ============
void display_status(const char* status) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("RAK Air Quality");
  display.println("");
  display.println(status);
  display.display();
}

// ============ SETUP ============
void setup(void) {
  Serial.begin(115200);
  delay(100);
  
#ifndef MAX_SAVE
  Serial.println("\n===========================================");
  Serial.println("RAK Air Quality Station v1.0");
  Serial.println("RAK4631 + RAK19001 + RAK12039");
  Serial.println("===========================================");
#endif

  // Initialize LEDs
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(LED_CONN, OUTPUT);
  digitalWrite(LED_CONN, HIGH);

  // Setup I2C for OLED
  Wire.begin();
  delay(100);

  // Scan I2C bus
  i2c_scan_and_print();

  // Setup OLED display
  setup_oled_display();
  delay(500);

  // Setup PM2.5 sensor on UART
  display_status("Init sensor...\nIO Slot B");
  bool particle_ok = setup_particle_sensor();
  
  if (!particle_ok) {
#ifndef MAX_SAVE
    Serial.println("========================================");
    Serial.println("ERROR: Particle sensor not found!");
    Serial.println("========================================");
    Serial.println("Troubleshooting:");
    Serial.println("1. RAK12039 in IO Slot B?");
    Serial.println("2. IO Slot B Power = ON?");
    Serial.println("3. Sensor powered? (check LED)");
    Serial.println("4. Fan running?");
    Serial.println("========================================");
#endif
    display_status("SENSOR ERROR!\nCheck:\n-IO Slot B\n-Power ON");
    delay(5000);
  } else {
    display_status("Sensor OK!\nJoining network...");
  }

  // Initialize LoRaWAN
  if (initLoRaWan() != 0) {
#ifndef MAX_SAVE
    Serial.println("❌ LoRaWAN init failed");
#endif
    display_status("LoRaWAN ERROR!");
    delay(2000);
  }

  // Create semaphore
  taskEvent = xSemaphoreCreateBinary();
  xSemaphoreGive(taskEvent);

  if (particle_ok) {
    display_status("Warming up...\nPlease wait 60s");
  }
  
#ifndef MAX_SAVE
  Serial.println("✓ Setup complete!\n");
#endif
}

// ============ MAIN LOOP ============
void loop(void) {
  static unsigned long lastReadTime = 0;
  unsigned long currentTime = millis();
  
  digitalWrite(LED_BUILTIN, LOW);

  // Read sensor every 30 seconds
  if (currentTime - lastReadTime >= 30000) {
    lastReadTime = currentTime;
    
    if (read_particle_data()) {
      display_particle_data();
      
      if (lmh_join_status_get() == LMH_SET) {
        if (sendLoRaFrame()) {
#ifndef MAX_SAVE
          Serial.println("✓ Data sent to TTN!");
#endif
          // Blink confirmation
          for (int i = 0; i < 3; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
          }
        } else {
#ifndef MAX_SAVE
          Serial.println("❌ Failed to send to TTN");
#endif
        }
      } else {
#ifndef MAX_SAVE
        Serial.println("⚠ Not joined to LoRaWAN yet");
#endif
        display_status("Waiting for\nLoRaWAN join...");
      }
    } else {
      // Display warm-up status
      if (!sensorReady) {
        unsigned long elapsed = millis() - sensorStartTime;
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Warming up sensor");
        display.println("");
        display.printf("%lu / 60 sec\n", elapsed/1000);
        display.println("");
        display.println("Fan active");
        display.println("Please wait...");
        display.display();
      } else {
        display_status("Sensor read fail\nRetrying...");
#ifndef MAX_SAVE
        Serial.println("⚠ Retrying sensor...");
#endif
      }
    }
  }

  // Handle LoRaWAN events
  if (taskEvent != NULL) {
    if (xSemaphoreTake(taskEvent, 100) == pdTRUE) {
      digitalWrite(LED_BUILTIN, HIGH);

      switch (eventType) {
        case 0:  // LoRa RX
#ifndef MAX_SAVE
          Serial.println("Received LoRaWAN packet");
#endif
          if (rcvdLoRaData[0] > 0x1F) {
#ifndef MAX_SAVE
            Serial.printf("%s\n", (char *)rcvdLoRaData);
#endif
          } else {
#ifndef MAX_SAVE
            for (int idx = 0; idx < rcvdDataLen; idx++) {
              Serial.printf("%02X ", rcvdLoRaData[idx]);
            }
            Serial.println();
#endif
          }
          break;

        case 1:  // Timer wakeup
#ifndef MAX_SAVE
          Serial.println("Timer wakeup - reading and sending");
#endif
          if (read_particle_data()) {
            display_particle_data();
            sendLoRaFrame();
          }
          break;

        default:
          break;
      }
      
      digitalWrite(LED_BUILTIN, LOW);
      xSemaphoreTake(taskEvent, 10);
    }
  }
  
  delay(100);
}