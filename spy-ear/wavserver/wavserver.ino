#include "AudioTools.h"

AudioWAVServer server("vodafoneBBB2E4", "Arami&Benjamin"); // TODO: Change to match your WiFi
AnalogAudioStream input; // Provides raw input amplitudes (unsigned integer)
ConverterAutoCenter<int16_t> center; // Unsigned integer to +/-; leave argument channels=2 regardless of config.channels in setup()!

ConverterScaler<int16_t> volume(1, 0, 99999); // factor, offset, maxvalue
ConverterStream<int16_t, ConverterScaler<int16_t>> audio(input, volume);


void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info); // TODO: Change to ::Error once you've got it all sorted

  Serial.println("Starting input stream ...");
  auto config = input.defaultConfig(RX_MODE);
  config.channels = 1; // Number of audio channels (currently AnalogAudioStream only samples a single channel)
  config.setInputPin1(GPIO_NUM_32); // Default 34. TODO: Change for your wiring
  config.sample_rate = 16000; // Samples per second per channel (so 16 bits * 2)
  input.begin(config);
  Serial.println("Input stream started.");

  config.sample_rate *= 6;//6/config.channels; // TODO: this corrects the sample rate problem, but why? One *2 seems related to channels; another *2 for 16 vs 8 bits?
  if (true) { server.begin(input, config, &center); } // Use "raw" sound
  else { server.begin(audio, config, &center); } // Use "processed" sound
// server.begin(...) currently ignores the converter, so set converter afterwards
//   server.setConverter(&center); // TODO: the above works now - contribute code fix

  Serial.println("Server started.");
}


void loop() {
  volume.setFactor(10); // ... can change volume dynamically if you wish, eg. from a pin
  server.copy();  
}
