#include <SPI.h>
#include <LoRa.h>

#define ss 10
#define rst 9
#define dio0 2

void setup() {
  Serial.begin(115200);

  Serial.println("LoRa Receiver");

  LoRa.setPins(ss, rst, dio0);
  
  LoRa.setSyncWord(0xA5);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
}

void loop() {
  // try to parse packet
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print("Received packet '");

    // read packet
    while (LoRa.available())
    {
      String LoRaData = LoRa.readString();
      Serial.print(LoRaData);
    }

    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
  }
}
