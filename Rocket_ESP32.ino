#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <LoRa.h>

#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

#define ss 5
#define rst 14
#define dio0 2

static float vdistance;
static float home;
typedef struct Message {
  float latitude, longitude, height, spd, lastspd;
  int tmhour, tmmin, tmsec;
} Message;

static QueueHandle_t queue = NULL;
static SemaphoreHandle_t mutex;
static TimerHandle_t timer = NULL;

TinyGPSPlus gps;
HardwareSerial SerialGPS(1);

void getGPS(void *parameters)
{
  while (1)
  {
    while (SerialGPS.available() > 0)
    {
      if (gps.encode(SerialGPS.read()))
      {
        if (gps.location.isValid())
        {
//Get available data using GPS
          Message msg;
          msg.latitude = gps.location.lat();
          msg.longitude = gps.location.lng();
          msg.height = gps.altitude.meters();
          calHeight();
          msg.spd = gps.speed.mph();
          if (msg.spd < msg.lastspd) //give alert if rocket is slowing down
          {
            xTimerStart(timer, portMAX_DELAY);
          }
          msg.lastspd = msg.spd;
          msg.tmhour = gps.time.hour() + 8;
          msg.tmmin = gps.time.minute();
          msg.tmsec = gps.time.second();

          xQueueSend(queue, (void *)&msg, portMAX_DELAY);
        }
      }
    }
  }
}

void sendData(void *parameters)
{
  while (1)
  {
    Message rcv_msg;
    if (xQueueReceive(queue, (void *)&rcv_msg, portMAX_DELAY) == pdTRUE)
    {
//Start sending data through LoRa
      LoRa.beginPacket();
      LoRa.print(rcv_msg.tmhour);
      LoRa.print(":");
      LoRa.print(rcv_msg.tmmin);
      LoRa.print(":");
      LoRa.print(rcv_msg.tmsec);
      LoRa.print("\t");
      LoRa.print(rcv_msg.latitude, 6);
      LoRa.print("\t");
      LoRa.print(rcv_msg.longitude, 6);
      LoRa.print("\t");
      LoRa.print(vdistance, 2);
      LoRa.print("\t");
      LoRa.print(rcv_msg.spd, 2);
      LoRa.endPacket();

    }
  }
}

void calHeight()
{
  // take mutex
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (home == 0) //get in if no set ground position
  {
    home = gps.altitude.meters();
  }
	// calculate height of rocket from the ground
  vdistance = gps.altitude.meters() - home;
  xSemaphoreGive(mutex); // release mutex
}

void myTimerCallback(TimerHandle_t xTimer) {
LoRa.beginPacket();
  Serial.println("*****ROCKET SLOWING DOWN*****");
LoRa.endPacket();
}

void setup() {

  Serial.begin(115200);
  SerialGPS.begin(9600, SERIAL_8N1, 16, 17);

  LoRa.setPins(ss, rst, dio0);

  while (!LoRa.begin(433E6))
  {
    Serial.println(".");
    delay(500);
  }

  LoRa.setSyncWord(0xB3);
  Serial.println("Rocket Telemetery Begin..");

  queue = xQueueCreate(6, sizeof(Message));
  mutex = xSemaphoreCreateMutex();

  timer = xTimerCreate("Timer",
                       10 / portTICK_PERIOD_MS,
                       pdFALSE,
                       (void *)0,
                       myTimerCallback);

  xTaskCreatePinnedToCore(getGPS,
                          "Get Info",
                          1024,
                          NULL,
                          1,
                          NULL,
                          app_cpu);

  xTaskCreatePinnedToCore(sendData,
                          "Send Data",
                          1024,
                          NULL,
                          1,
                          NULL,
                          app_cpu);
}

void loop() {}
