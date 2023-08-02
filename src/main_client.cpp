#include <Arduino.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
#include <WiFi.h>
//#include "ESPOTADASH.h"

#define DEBUG 0
#define INFO 1
#define INTERVAL 250

// RS485/Modbus-configuration
#define RS485_BAUD 9600
#define PIN_RS485_TX 27
#define PIN_RS485_RX 26
#define PIN_RS485_REDE 4
#define SLAVE_ID 247
//#define REGISTER 33000
ModbusMaster node;

// ESP OTA Dashboard integration
const char* ssid = "RRHomeWPA";
const char* password = "eagle!12";
const char* hostName = "ESP MTEC Gateway";
//const char* serverAddress = "http://192.168.0.99:3000";
//unsigned long heartbeateInterval = 10000;
//unsigned long registrationInterval = 30000;
//const char* firmwareVersion = "1.0.0";
//ESPOTADASH ota(ssid, password, hostName, serverAddress, heartbeateInterval, registrationInterval, firmwareVersion);

// MQTT integration
const char* clientid = "esp_mtec_gateway";
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* willTopic = "m-tec/local/alive";
const uint8_t willQoS = 0;
boolean willRetain = true;
const char* willMessage = "OFF";
boolean cleanSession = true;
const char* mqttServer = "192.168.0.232";
const int mqttPort = 1883;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
WiFiClient espClient;
PubSubClient mqttclient(espClient);
String basetopic = "m-tec/local";


// Wifi access
unsigned long previousMillis = 0;
unsigned long interval = 30000;

typedef struct {
  String _name;
  unsigned int _address;
  unsigned int _register_len;
  String _register_datatype;
  signed int _scale;
  String _description;
} Mtec_data;

const Mtec_data bms_soc_data = {
  "BMS_SOC", 33000, 1, "uint16", -2, "Ladezustand der Batterie"
};

const Mtec_data bms_voltage = {
  "BMS_V", 30254, 1, "uint16", -1, "Batteriespannung"
};

const Mtec_data bms_current = {
  "BMS_A", 30255, 1, "int16", -1, "Batteriestrom"
};

const Mtec_data bms_temperature = {
  "BMS_Temp", 33003, 1, "int16", -1, "Batterie Temperatur"
};

const Mtec_data bms_status = {
  "BMS_Status", 33002, 1, "uint16", 0, "BMS Status Code"
};

const Mtec_data bms_error = {
  "BMS_Error", 33016, 2, "uint32", 0, "BMS Error Code"
};

const Mtec_data bms_warning = {
  "BMS_Warning", 33018, 2, "uint32", 0, "BMS Warning Code"
};

const Mtec_data power_ac_grid = {
  "AC_Power_Grid", 11016, 2, "int32", 1, "Leistung am AC Eingang des Wechselrichters"
};

const Mtec_data power_ac_backup = {
  "AC_Power_Backup", 30230, 2, "int32", 1, "Leistung am Backup Ausgang des Wechselrichters"
};

const Mtec_data power_pv = {
  "PV_Power", 11028, 2, "uint32", 1, "PV Erzeugung Gesamt"
};

const Mtec_data power_nvp = {
  "NVP_Power", 11000, 2, "int32", 1, "Leistung am NVP Zaehler"
};

const Mtec_data power_battery = {
  "Battery_Power", 30258, 2, "int32", 1, "Lade/Entladeleistung der Batterie"
};

const Mtec_data inverter_running_state = {
  "INV_Running_State", 10105, 1, "uint16", 0, "Betriebszustand des Wechselrichters" //0: wait/wait for on-grid; 1: check/self-check; 2: On Grid; 3: fault; 4: flash/firmware update; 5: Off Grid
};

const Mtec_data fault_flag1 = {
  "INV_Fault_Flag1", 10112, 2, "uint32", 0, "Fehler siehe Tabelle 1"
};

const Mtec_data fault_flag2 = {
  "INV_Fault_Flag2", 10114, 2, "uint32", 0, "Fehler siehe Tabelle 2"
};

/*
/ Tabelle 1:
/ 10112 + 10113 (Fault Flag1):
/    Bit0 = 0x00000001/Dec 1:   Mains Lost
/    Bit1 = 0x00000002/Dec 2:   Grid Voltage Fault
/    Bit2 = 0x00000004/Dec 4:   Grid Frequency Fault
/    Bit3 = 0x00000008/Dec 8:   DCI Fault
/    Bit4 = 0x00000010/Dec 16:  ISO Over Limitation
/    Bit5 = 0x00000020/Dec 32:  GFCI Fault
/    Bit6 = 0x00000040/Dec 64:  PV Over Voltage
/    Bit7 = 0x00000080/Dec 128: Bus Over Voltage
/    Bit8 = 0x00000100/Dec 256: Inverter Over Temperature

/ Tabelle 2:
/ 10114 + 10115 (Fault Flag2):
/    Bit1 = 0x00000002/Dec 2:   SPI Fault
/    Bit2 = 0x00000004/Dec 4:   E2 Fault
/    Bit3 = 0x00000008/Dec 8:   GFCI Device Fault
/    Bit4 = 0x00000010/Dec 16:  AC Transducer Fault
/    Bit5 = 0x00000020/Dec 32:  Relay Check Fail
/    Bit6 = 0x00000040/Dec 64:  Internal Fan Fault
/    Bit7 = 0x00000080/Dec 128: External Fan Fault

*/

Mtec_data mtec_data[15];

void createMtecTable() {
  mtec_data[0] = bms_soc_data;
  mtec_data[1] = bms_voltage;
  mtec_data[2] = bms_current;
  mtec_data[3] = bms_temperature;
  mtec_data[4] = bms_status;
  mtec_data[5] = bms_error;
  mtec_data[6] = bms_warning;
  mtec_data[7] = power_ac_grid;
  mtec_data[8] = power_ac_backup;
  mtec_data[9] = power_pv;
  mtec_data[10] = power_nvp;
  mtec_data[11] = power_battery;
  mtec_data[12] = inverter_running_state;
  mtec_data[13] = fault_flag1;
  mtec_data[14] = fault_flag2;
}

void preTransmission() {
  digitalWrite(PIN_RS485_REDE, HIGH);
}

void postTransmission() {
  digitalWrite(PIN_RS485_REDE, LOW);
}

void blink() {
  digitalWrite(18, HIGH);
  delay(10);
  digitalWrite(18, LOW);
}

String process_value(double value_raw, int factor) {
  if (factor == 0) {
    int32_t value = (int32_t) value_raw;
    if (DEBUG) {
      Serial.print(millis());
      Serial.print(":  factor == 0: process value ");
      Serial.print(value_raw);
      Serial.print(" to ");
      Serial.println(value);
    }
    return (String) value;
  }
  else if (factor == 1) {
    if (DEBUG) {
      Serial.print(millis());
      Serial.print(":  factor == 1: return value ");
      Serial.print(value_raw);
      Serial.println(" as is");
    }
    return (String) value_raw;
  }
  else if (factor < 0 || factor > 1) {
    double value = value_raw * pow(10, factor);
    if (DEBUG) {
      Serial.print(millis());
      Serial.print(":  factor == ");
      Serial.print(factor);
      Serial.print(" : process value ");
      Serial.print(value_raw);
      Serial.print(" to ");
      Serial.println(value);
    }
    return (String) value;
  }
  else {
    return (String) value_raw;
  }
}

void connectWifi() {
  delay(10);
  Serial.println();
  Serial.print(millis());
  Serial.print(": Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostName);
  WiFi.begin(ssid, password);

  Serial.println("Connecting to Wifi ...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();
  Serial.print(millis());
  Serial.println(": Wifi connected");
  Serial.print(millis());
  Serial.print(": IP address; ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void checkWifi() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    if (DEBUG) {
      Serial.println();
      Serial.print(millis());
      Serial.println(": Check WiFi connected");
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print(millis());
      Serial.println(": Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
    }

    if (DEBUG) {
      Serial.print(millis());
      Serial.println(": WiFi connected");
    }
    previousMillis = currentMillis;
  }
}

void send_message(String topic, const char* msg, boolean retain) {
  if (INFO) {
    Serial.print(millis());
    Serial.print(": Send message to topic " + topic);
    Serial.print(" : ");
    Serial.println(msg);
  }
  mqttclient.publish(topic.c_str(), msg, retain);
}

void reconnect_mqtt() {
  while(!mqttclient.connected()) {
    Serial.print(millis());
    Serial.print(": Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if(mqttclient.connect(clientId.c_str(), willTopic, willQoS, willRetain, willMessage)) {
      Serial.print(millis());
      Serial.println(": connected");
      String topic = basetopic + "/alive";
      String msg = "ON";
      send_message(topic, msg.c_str(), true);
    }
    else {
      Serial.print(millis());
      Serial.print(": failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");

      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  connectWifi();
  mqttclient.setServer(mqttServer, mqttPort);
  //ota.begin();


  // set RE/DE-pin
  pinMode(PIN_RS485_REDE, OUTPUT);
  postTransmission();

  Serial2.begin(RS485_BAUD);
  node.begin(SLAVE_ID, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // set blink-pin
  pinMode(18, OUTPUT);
  digitalWrite(18, LOW);

  createMtecTable();

//  // initial status-blink
//  blink();
//  delay(500);
//  blink();
//  delay(500);
//  blink();
//  delay(2000);
}

void loop() {
  //ota.loop();

  if (!mqttclient.connected()) {
    reconnect_mqtt();
  }
  mqttclient.loop();

//  uint8_t result;
//  uint16_t data[6];
//
//  result = node.readHoldingRegisters(REGISTER, 2);
//  if (result == node.ku8MBSuccess) {
//    Serial.print("read register: ");
//    Serial.println(node.getResponseBuffer(0x00));
//    blink();
//  } 

  for (int i = 0; i < 15; i++) {
    //break;

    uint16_t result;
    double value;
    String value_out;


    if (DEBUG) {
      Serial.println();
      Serial.print(millis());
      Serial.print(": ");
      Serial.print(i);
      Serial.print(" = ");
      Serial.println("name " + mtec_data[i]._name);
      Serial.print(millis());
      Serial.print(": read " + String(mtec_data[i]._register_len));
      Serial.println(" bytes from register " + String(mtec_data[i]._address));
    }
    result = node.readHoldingRegisters(mtec_data[i]._address, mtec_data[i]._register_len);
    if (result == node.ku8MBSuccess) {
      if (mtec_data[i]._register_datatype == "int16") {
        int16_t value_raw = node.getResponseBuffer(0x00);
        value = (double) value_raw;
      }
      else if (mtec_data[i]._register_datatype == "uint16") {
        uint16_t value_raw = node.getResponseBuffer(0x00);
        value = (double) value_raw;
      }
      else if (mtec_data[i]._register_datatype == "int32") {
        int32_t value_raw;
        int16_t msw, lsw;
        msw = node.getResponseBuffer(0x00);
        lsw = node.getResponseBuffer(0x01);
        value_raw = (int32_t) msw<<16 | lsw;
        value = (double) value_raw;
      }
      else if (mtec_data[i]._register_datatype == "uint32") {
        uint32_t value_raw;
        uint16_t msw, lsw;
        msw = node.getResponseBuffer(0x00);
        lsw = node.getResponseBuffer(0x01);
        value_raw = (uint32_t) msw<<16 | lsw;
        value = (double) value_raw;
      }

      value_out = process_value(value, mtec_data[i]._scale);

      if (DEBUG) {
        Serial.print(millis());
        Serial.print(": result = ");
        Serial.println(value);
      }
      send_message(basetopic + "/" + mtec_data[i]._name, value_out.c_str(), false);
    }
    delay(INTERVAL);
  }
  checkWifi();
}