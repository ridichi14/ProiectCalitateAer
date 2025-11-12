/**
 * LoRaWAN Handler
 * Handles OTAA join, TX/RX, and TTN communication
 */

#include "main.h"

#define PIN_LORA_DIO_1 47
#define LORAWAN_APP_DATA_BUFF_SIZE 64
#define JOINREQ_NBTRIALS 8

static uint8_t m_lora_app_data_buffer[LORAWAN_APP_DATA_BUFF_SIZE];
static lmh_app_data_t m_lora_app_data = {m_lora_app_data_buffer, 0, 0, 0, 0};

// Forward declarations
static void lorawan_has_joined_handler(void);
static void lorawan_join_failed_handler(void);
static void lorawan_rx_handler(lmh_app_data_t *app_data);
static void lorawan_confirm_class_handler(DeviceClass_t Class);

static lmh_param_t lora_param_init = {
  LORAWAN_ADR_OFF, 
  DR_3, 
  LORAWAN_PUBLIC_NETWORK, 
  JOINREQ_NBTRIALS, 
  LORAWAN_DEFAULT_TX_POWER, 
  LORAWAN_DUTYCYCLE_OFF
};

static lmh_callback_t lora_callbacks = {
  BoardGetBatteryLevel, 
  BoardGetUniqueId, 
  BoardGetRandomSeed,
  lorawan_rx_handler, 
  lorawan_has_joined_handler, 
  lorawan_confirm_class_handler, 
  lorawan_join_failed_handler
};

// ============ LoRaWAN KEYS ============
// IMPORTANT: Update these with YOUR TTN keys!
uint8_t nodeDeviceEUI[8] = {0xAC, 0x1F, 0x09, 0xFF, 0xFE, 0x14, 0x77, 0x97};
uint8_t nodeAppEUI[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t nodeAppKey[16] = {0x60, 0x69, 0xD2, 0x00, 0x5F, 0xF4, 0xA7, 0x4C, 
                          0x9F, 0x29, 0x28, 0x7E, 0xAE, 0x9C, 0x08, 0xD8};

// For ABP mode (if needed)
uint32_t nodeDevAddr = 0x26021FB6;
uint8_t nodeNwsKey[16] = {0x32,0x3D,0x15,0x5A,0x00,0x0D,0xF3,0x35,0x30,0x7A,0x16,0xDA,0x0C,0x9D,0xF5,0x3F};
uint8_t nodeAppsKey[16] = {0x3F,0x6A,0x66,0x45,0x9D,0x5E,0xDC,0xA6,0x3C,0xBC,0x46,0x19,0xCD,0x61,0xA1,0x1E};

bool doOTAA = true;
DeviceClass_t gCurrentClass = CLASS_A;
LoRaMacRegion_t gCurrentRegion = LORAMAC_REGION_EU868;

// ============ INITIALIZE LoRaWAN ============
int8_t initLoRaWan(void) {
#ifndef MAX_SAVE
  Serial.println("📡 Initializing LoRaWAN...");
#endif
  
  if (lora_rak4630_init() != 0) {
#ifndef MAX_SAVE
    Serial.println("❌ lora_rak4630_init() failed");
#endif
    return -1;
  }

  if (doOTAA) {
#ifndef MAX_SAVE
    Serial.println("  Mode: OTAA");
#endif
    lmh_setDevEui(nodeDeviceEUI);
    lmh_setAppEui(nodeAppEUI);
    lmh_setAppKey(nodeAppKey);
  } else {
#ifndef MAX_SAVE
    Serial.println("  Mode: ABP");
#endif
    lmh_setNwkSKey(nodeNwsKey);
    lmh_setAppSKey(nodeAppsKey);
    lmh_setDevAddr(nodeDevAddr);
  }

  if (lmh_init(&lora_callbacks, lora_param_init, doOTAA, gCurrentClass, gCurrentRegion) != 0) {
#ifndef MAX_SAVE
    Serial.println("❌ lmh_init() failed");
#endif
    return -2;
  }

  if (!lmh_setSubBandChannels(1)) {
#ifndef MAX_SAVE
    Serial.println("❌ lmh_setSubBandChannels() failed");
#endif
    return -3;
  }

#ifndef MAX_SAVE
  Serial.println("✓ LoRaWAN initialized, starting join request...");
#endif
  lmh_join();

  return 0;
}

// ============ SEND DATA TO TTN ============
bool sendLoRaFrame(void) {
  if (lmh_join_status_get() != LMH_SET) {
#ifndef MAX_SAVE
    Serial.println("⚠ Not joined to network, skipping send");
#endif
    return false;
  }

  m_lora_app_data.port = LORAWAN_APP_PORT;

  // Pack PM data: PM2.5, PM10 (each uint16 big-endian)
  uint8_t buffSize = 0;
  uint16_t pm25 = particle_data.pm25_standard;
  uint16_t pm10 = particle_data.pm100_standard;

  m_lora_app_data_buffer[buffSize++] = (pm25 >> 8) & 0xFF;
  m_lora_app_data_buffer[buffSize++] = pm25 & 0xFF;
  m_lora_app_data_buffer[buffSize++] = (pm10 >> 8) & 0xFF;
  m_lora_app_data_buffer[buffSize++] = pm10 & 0xFF;

  m_lora_app_data.buffsize = buffSize;

#ifndef MAX_SAVE
  Serial.printf("Sending: PM2.5=%u, PM10=%u\n", pm25, pm10);
#endif

  lmh_error_status error = lmh_send(&m_lora_app_data, LMH_UNCONFIRMED_MSG);
  
  if (error != 0) {
#ifndef MAX_SAVE
    Serial.printf("❌ Send failed with error: %d\n", error);
#endif
    return false;
  }

  return true;
}

// ============ CALLBACKS ============

static void lorawan_has_joined_handler(void) {
#ifndef MAX_SAVE
  Serial.println("✓ LoRaWAN network joined!");
#endif
  
  if (doOTAA) {
    uint32_t otaaDevAddr = lmh_getDevAddr();
#ifndef MAX_SAVE
    Serial.printf("  OTAA DevAddr: %08X\n", otaaDevAddr);
#endif
  }
  
  digitalWrite(LED_CONN, LOW);

  // Start periodic timer
  taskWakeupTimer.begin(SLEEP_TIME, periodicWakeup);
  taskWakeupTimer.start();
}

static void lorawan_join_failed_handler(void) {
#ifndef MAX_SAVE
  Serial.println("❌ LoRaWAN JOIN FAILED!");
  Serial.println("  Check your keys!");
  Serial.println("  Check if gateway is in range!");
#endif
}

static void lorawan_rx_handler(lmh_app_data_t *app_data) {
#ifndef MAX_SAVE
  Serial.printf("📥 RX on port %d, size: %d bytes (RSSI: %d, SNR: %d)\n",
                app_data->port, app_data->buffsize, app_data->rssi, app_data->snr);
#endif

  switch (app_data->port) {
    case 3:  // Class switching
      if (app_data->buffsize == 1) {
        switch (app_data->buffer[0]) {
          case 0:
            lmh_class_request(CLASS_A);
#ifndef MAX_SAVE
            Serial.println("  Request: Switch to CLASS A");
#endif
            break;
          case 1:
            lmh_class_request(CLASS_B);
#ifndef MAX_SAVE
            Serial.println("  Request: Switch to CLASS B");
#endif
            break;
          case 2:
            lmh_class_request(CLASS_C);
#ifndef MAX_SAVE
            Serial.println("  Request: Switch to CLASS C");
#endif
            break;
          default:
            break;
        }
      }
      break;

    case LORAWAN_APP_PORT:  // Application data
      memcpy(rcvdLoRaData, app_data->buffer, app_data->buffsize);
      rcvdDataLen = app_data->buffsize;
      eventType = 0;
      
      if (taskEvent != NULL) {
#ifndef MAX_SAVE
        Serial.println("  Waking up main task");
#endif
        xSemaphoreGive(taskEvent);
      }
      break;

    default:
      break;
  }
}

static void lorawan_confirm_class_handler(DeviceClass_t Class) {
#ifndef MAX_SAVE
  Serial.printf("✓ Switched to class %c\n", "ABC"[Class]);
#endif
  
  m_lora_app_data.buffsize = 0;
  m_lora_app_data.port = LORAWAN_APP_PORT;
  lmh_send(&m_lora_app_data, LMH_UNCONFIRMED_MSG);
}