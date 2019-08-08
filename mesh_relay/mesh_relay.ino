#include <EspNowFloodingMesh.h>
#include<SimpleMqtt.h>

/********NODE SETUP********/
#define ESP_NOW_CHANNEL 1
const char deviceName[] = "relayAutotalli";
int bsid = 0x112233;
unsigned char secredKey[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
unsigned char iv[16] = {0xb2, 0x4b, 0xf2, 0xf7, 0x7a, 0xc5, 0xec, 0x0c, 0x5e, 0x1f, 0x4d, 0xc1, 0xae, 0x46, 0x5e, 0x75};;
const int ttl = 3;
//#include "/home/arttu/git/myEspNowMeshConfig.h" //My secred mesh setup...
/*****************************/

#define RELAY_PIN 0

bool relayValue=false;
bool relaySet=false;

SimpleMQTT simpleMqtt = SimpleMQTT(ttl, deviceName);

void espNowFloodingMeshRecv(const uint8_t *data, int len, uint32_t replyPrt) {
  if (len > 0) {
    simpleMqtt.parse(data, len, replyPrt); //Parse simple Mqtt protocol messages
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  //Set device in AP mode to begin with
  espNowFloodingMesh_RecvCB(espNowFloodingMeshRecv);
  espNowFloodingMesh_secredkey(secredKey);
  espNowFloodingMesh_setAesInitializationVector(iv);
  espNowFloodingMesh_setToMasterRole(false, ttl);
  espNowFloodingMesh_setToBatteryNode();
  espNowFloodingMesh_begin(ESP_NOW_CHANNEL, bsid);

  espNowFloodingMesh_ErrorDebugCB([](int level, const char *str) {
    Serial.print(level); Serial.println(str); //If you want print some debug prints
  });


  //Handshake with master
  if (!espNowFloodingMesh_syncWithMasterAndWait()) {
    //Sync failed??? No connection to master????
    Serial.println("No connection to master!!! Reboot");
    delay(10000);
    ESP.restart();
  }

  //Handle MQTT events from master. Do not call publish inside of call back. --> Endless event loop and crash
  simpleMqtt.handleEvents([](const char *topic, const char* value) {
        simpleMqtt._ifSwitch(VALUE, "led", [](MQTT_switch value){ //<--> Listening topic switch/led/value/value
      if(value==SWITCH_ON) {
        relayValue = true;
      }
      if(value==SWITCH_OFF) {
        relayValue = false;
      }
    });
    simpleMqtt._ifSwitch(SET, "led", [](MQTT_switch set){ //<-->Listening topic device1/switch/led/set
      if(set==SWITCH_ON) {
        relaySet = true;
      }
      if(set==SWITCH_OFF) {
        relaySet = false;
      }
    });
  });

  if (!simpleMqtt._switch(SUBSCRIBE, "relay")) { //Subscribe topic device1/switch/relay/set and get topic device1/switch/relay/value from cache
    Serial.println("MQTT operation failed. No connection to gateway");
    delay(10000);
    ESP.restart();
  }
}

void loop() {
  espNowFloodingMesh_loop();
  if(relayValue!=relaySet) {
    digitalWrite(RELAY_PIN, relaySet);
    relayValue = relaySet;
    if (!simpleMqtt._switch(PUBLISH, "relay", relaySet?SWITCH_ON:SWITCH_OFF)) { //publish topic device1/switch/relay/value on/off
      Serial.println("Publish failed... Reboot");
      delay(10000);
      ESP.restart();
    }
  }
}
