/**
 * Adapted from https://www.instructables.com/The-Best-Way-for-Sampling-Audio-With-ESP32/
 * ESP documentation https://docs.espressif.com/projects/esp-idf/en/v4.2/esp32/api-reference/peripherals/adc.html
 * 
 * Simply select the pin to use below, and connect your analog microphone to it.
 * After uploading (and resetting), open the "Serial Plotter" tool to watch the signal scroll by.
 */ 
#include <driver/i2s.h>
#define ARRAYSIZE(a)  (sizeof(a)/sizeof(a[0]))

/* MUST use ADC1*, not ADC2 when WiFi is active */
//#define ADC_INPUT ADC1_CHANNEL_0 //pin D32 seems to respond much much faster than 6 & 7 - higher gain?
#define ADC_INPUT ADC1_CHANNEL_6 //pin D34
//#define ADC_INPUT ADC1_CHANNEL_7 //pin D35

uint16_t offset = (int)ADC_INPUT * 0x1000; // The sampled values are offset by this amount

const i2s_port_t I2S_PORT = I2S_NUM_0; // Not sure what 1 is for

const double samplingFrequency = 16000;
const int SAMPLEBLOCK = 8;
uint16_t samples[SAMPLEBLOCK];


void setup() {
  Serial.begin(115200);
  Serial.println("Setting up Audio Input I2S");
  setupI2S();
  Serial.println("Audio input setup completed");
  delay(1000);
}

void loop() {
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, 
          (void*)samples, 
          sizeof(samples),
          &bytesRead,
          portMAX_DELAY);
  if (bytesRead != sizeof(samples)) {
      Serial.printf("Could only read %u bytes of %u - needs debugging!\n", bytesRead, sizeof(samples));
  }
                              
  for (uint16_t i = 0; i < ARRAYSIZE(samples); i++) {
      Serial.printf("%7d,",samples[i]-offset);
  }
  Serial.printf("\n"); 
}  
 
void setupI2S() {
  esp_err_t err;
  const i2s_config_t i2s_config = { 
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
      .sample_rate = samplingFrequency,                        
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S_MSB,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = SAMPLEBLOCK,
      .use_apll = true                     // Audio-quality phase locked loop
  };
 
  err = i2s_driver_install(I2S_NUM_0, &i2s_config,  0, NULL);
  CHECK_ERR(err, "Failed installing driver");

  err = i2s_set_adc_mode(ADC_UNIT_1, ADC_INPUT);
  CHECK_ERR(err, "Failed setting up adc mode");
  
  err = adc1_config_channel_atten(ADC_INPUT, ADC_ATTEN_0db);
  CHECK_ERR(err, "Failed to set attenuation");
}

void CHECK_ERR(esp_err_t err, char* message) {
  if (err != ESP_OK) {
    Serial.printf("%s :%d\n", message, err);
    while (true); // Stay here, don't exit
  }
}
