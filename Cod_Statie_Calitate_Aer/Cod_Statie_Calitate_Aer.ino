#include "main.h"

// Obiecte globale
Adafruit_PM25AQI aqi;
particle_data_s particle_data;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/** Semaphore folosit pentru sincronizare */
SemaphoreHandle_t taskEvent = NULL;
/** Timer pentru wakeup */
SoftwareTimer taskWakeupTimer;
/** Buffer pentru date LoRaWAN primite */
uint8_t rcvdLoRaData[256];
uint8_t rcvdDataLen = 0;
/** Tipul evenimentului */
uint8_t eventType = -1;

void periodicWakeup(TimerHandle_t unused) {
  digitalWrite(LED_BUILTIN, HIGH);
  eventType = 1;
  xSemaphoreGiveFromISR(taskEvent, pdFALSE);
}

void setup_particle_sensor(void) {
#ifndef MAX_SAVE
  Serial.println("Initializing RAK12039 particle sensor...");
#endif
  Wire.begin();
  if (!aqi.begin_I2C()) {
#ifndef MAX_SAVE
    Serial.println("Could not find PM 2.5 sensor!");
#endif
    while (1) { delay(10); }
  }
#ifndef MAX_SAVE
  Serial.println("PM25 sensor found!");
#endif
}

bool read_particle_data(void) {
  PM25_AQI_Data data;
  if (!aqi.read(&data)) {
#ifndef MAX_SAVE
    Serial.println("Could not read from AQI sensor");
#endif
    return false;
  }
  // Salvăm datele în structura noastră
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
  Serial.println("---------------------------------------");
  Serial.println("Concentration Units (standard)");
  Serial.printf("PM 1.0: %d μg/m³\n", data.pm10_standard);
  Serial.printf("PM 2.5: %d μg/m³\n", data.pm25_standard);
  Serial.printf("PM 10: %d μg/m³\n", data.pm100_standard);
  Serial.println("---------------------------------------");
  Serial.println("Concentration Units (environmental)");
  Serial.printf("PM 1.0: %d μg/m³\n", data.pm10_env);
  Serial.printf("PM 2.5: %d μg/m³\n", data.pm25_env);
  Serial.printf("PM 10: %d μg/m³\n", data.pm100_env);
  Serial.println("---------------------------------------");
#endif
  return true;
}

void setup_oled_display() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);  // blocare
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("OLED Initialized");
  display.display();
}

void display_particle_data() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.printf("PM1.0: %u\n", particle_data.pm10_standard);
  display.printf("PM2.5: %u\n", particle_data.pm25_standard);
  display.printf("PM10 : %u\n", particle_data.pm100_standard);

  display.display();
}

void setup(void) {
  Serial.begin(115200);
  Wire.begin();

  setup_oled_display();

  setup_particle_sensor();

  if (initLoRaWan() != 0) {
    Serial.println("LoRaWAN init failed");
    while(1);
  }

  taskEvent = xSemaphoreCreateBinary();
  xSemaphoreGive(taskEvent);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(LED_CONN, OUTPUT);
  digitalWrite(LED_CONN, HIGH);

#ifndef MAX_SAVE
  // Serial already inițializat
  Serial.println("=====================================");
  Serial.println("RAK4630 LoRaWAN + RAK12039 Particle Sensor");
  Serial.println("=====================================");
#endif

  xSemaphoreTake(taskEvent, 10);
}

void loop(void) {
  // LED stins când mergem în sleep
  digitalWrite(LED_BUILTIN, LOW);

  // Citim date particule, afișăm pe OLED și trimitem prin LoRaWAN
  if (read_particle_data()) {
    display_particle_data();
    sendLoRaFrame();
  }

  delay(1000);

  // Așteptăm eveniment pentru trezire
  if (xSemaphoreTake(taskEvent, portMAX_DELAY) == pdTRUE) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);

    switch (eventType) {
      case 0: // Date primite pe LoRa
#ifndef MAX_SAVE
        Serial.println("Received package over LoRaWan");
#endif
        if (rcvdLoRaData[0] > 0x1F) {
#ifndef MAX_SAVE
          Serial.printf("%s\n", (char *)rcvdLoRaData);
#endif
        } else {
#ifndef MAX_SAVE
          for (int idx = 0; idx < rcvdDataLen; idx++) {
            Serial.printf("%X ", rcvdLoRaData[idx]);
          }
          Serial.println("");
#endif
        }
        break;

      case 1: // Timer wakeup
#ifndef MAX_SAVE
        Serial.println("Timer wakeup - reading particle sensor and sending data");
#endif
        if (sendLoRaFrame()) {
#ifndef MAX_SAVE
          Serial.println("Particle data sent successfully via LoRaWAN");
#endif
          for (int i = 0; i < 3; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
          }
        } else {
#ifndef MAX_SAVE
          Serial.println("Failed to send particle data via LoRaWAN");
#endif
        }
        break;

      default:
#ifndef MAX_SAVE
        Serial.println("This should never happen ;-)");
#endif
        break;
    }
    digitalWrite(LED_BUILTIN, LOW);
    xSemaphoreTake(taskEvent, 10);
  }
}
