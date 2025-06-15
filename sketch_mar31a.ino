#include <ESP8266WiFi.h>
#include <DHT.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>
#include <PubSubClient.h>
#include <time.h>
#include <TZ.h>
#include <FS.h>
#include <ArduinoJson.h>

#define RELAY1 D1
#define RELAY2 D2

#define DHTPIN D5
#define DHTTYPE DHT11

#define USERID 1
#define DEVICEID 1
#define TOPIC "waterPump/1/sensor_data"
#define GARDENID 1

DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "tuandeptrai";
const char* password = "12345678";

const char* ca_certs = R"EOF(
-----BEGIN CERTIFICATE-----
MIID...
... (nội dung chứng chỉ) ...
-----END CERTIFICATE-----
)EOF";

// Mqtt 
const char* mqtt_server = "3c8284fc593942649ce5a137437254d6.s1.eu.hivemq.cloud";
const char* mqtt_username = "huutuan";
const char* mqtt_password = "Huutuan1";
const int mqtt_port = 8883;
const char* topic = "waterPump/1/sensor_data";

// Biến thời gian
unsigned long lastSendTime = 0;
const unsigned long interval = 60000; // 60 giây


WiFiClientSecure espClient;
PubSubClient * client;
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// Biến lưu thông tin lịch hẹn tưới
#define MAX_SCHEDULES 5
struct Schedule {
  bool enabled = false;
  String repeat = "";
  time_t time = 0;
  bool wateredToday = false;
};
Schedule wateringSchedules[MAX_SCHEDULES];
int scheduleCount = 0;

void setup_Wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to:");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setDateTime() {
  configTime(25200, 0, "pool.ntp.org", "time.nist.gov"); //25200: GMT+7 = 7*3600

  Serial.println("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  int retries = 0;

  while (now < 8 * 3600 * 2 && retries < 50) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
    retries++;
  }
  Serial.println();

  if (now < 8 * 3600 * 2) {
    Serial.println("Failed to sync time via NTP.");
    return;
  }

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  // display current UTC time
  Serial.printf("%s %s", tzname[0], asctime(&timeinfo));

}

// Hàm parse và lưu lịch hẹn từ JSON
void handleScheduleJson(const char* payload, unsigned int length) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("Failed to parse schedule JSON: ");
    Serial.println(error.c_str());
    return;
  }
  // Reset all schedules
  scheduleCount = 0;
  if (doc.is<JsonArray>()) {
    for (JsonObject sched : doc.as<JsonArray>()) {
      if (scheduleCount >= MAX_SCHEDULES) break;
      if (sched["enabled"].isNull() || sched["time"].isNull()) continue;
      wateringSchedules[scheduleCount].enabled = sched["enabled"];
      wateringSchedules[scheduleCount].repeat = sched["repeat"] | "";
      if (sched["time"].containsKey("_seconds")) {
        wateringSchedules[scheduleCount].time = (time_t)sched["time"]["_seconds"];
      } else {
        wateringSchedules[scheduleCount].time = 0;
      }
      wateringSchedules[scheduleCount].wateredToday = false;
      scheduleCount++;
    }
  } else if (doc.is<JsonObject>()) {
    // fallback: single schedule
    if (doc["enabled"].isNull() || doc["time"].isNull()) return;
    wateringSchedules[0].enabled = doc["enabled"];
    wateringSchedules[0].repeat = doc["repeat"] | "";
    if (doc["time"].containsKey("_seconds")) {
      wateringSchedules[0].time = (time_t)doc["time"]["_seconds"];
    } else {
      wateringSchedules[0].time = 0;
    }
    wateringSchedules[0].wateredToday = false;
    scheduleCount = 1;
  }
  Serial.print("Đã lưu số lịch hẹn tưới: ");
  Serial.println(scheduleCount);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("]");
  for(int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Xử lý lịch hẹn tưới nếu đúng topic
  if (String(topic) == "waterPump/1/schedule") {
    handleScheduleJson((const char*)payload, length);
    return;
  }

  // Xử lý trạng thái bơm nếu đúng topic
  if (String(topic) == "waterPump/1/status") {
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (!error && doc.containsKey("pumpPower")) {
      bool pumpPower = doc["pumpPower"];
      int pumpDuration = 5; // default 5 seconds
      if (doc.containsKey("pumpDuration")) {
        pumpDuration = doc["pumpDuration"];
      }
      if (pumpPower) {
        digitalWrite(RELAY1, LOW);
        digitalWrite(RELAY2, LOW);
        Serial.print("Bật bơm theo lệnh status trong ");
        Serial.print(pumpDuration);
        Serial.println(" giây!");
        delay(pumpDuration * 1000);
        digitalWrite(RELAY1, HIGH);
        digitalWrite(RELAY2, HIGH);
        Serial.println("Tắt bơm sau khi hết thời gian pumpDuration!");
      } else {
        digitalWrite(RELAY1, HIGH);
        digitalWrite(RELAY2, HIGH);
        Serial.println("Tắt bơm theo lệnh status!");
      }
    } else {
      Serial.println("Lỗi parse JSON hoặc không có trường pumpPower!");
    }
    return;
  }

  // nhay den neu nhan duoc payload
  if(payload != NULL && payload[0] != '\0'){
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}


void reconnect() {
  // loop until we're reconnected
  while(!client->connected()){
    Serial.print("Attempting MQTT connection...");
    String clientID = "ESP8266";
    //attempt to connect
    if(client->connect(clientID.c_str(), mqtt_username, mqtt_password)){
      Serial.println("connected");
      client->publish("testTopic", "hi");
      client->subscribe("testTopic");
      client->subscribe("waterPump/1/schedule");
      client->subscribe("waterPump/1/status");
    } else {
      Serial.print("failed, rc = ");
      Serial.print(client->state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  delay(500);
  Serial.begin(9600);
  delay(500);
  LittleFS.begin();
  dht.begin();
  setup_Wifi();
  setDateTime();

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  digitalWrite(RELAY1,HIGH);
  digitalWrite(RELAY2,HIGH);

  espClient.setInsecure();

  // int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  // // int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  // Serial.printf("Number of CA certs read: %d\n", numCerts);
  // if(numCerts == 0){
  //   Serial.printf("No certs found.\n");
  //   return;
  // }
  // BearSSL::WiFiClientSecure *bear = new BearSSL::WiFiClientSecure();
  // // Integrate the cert store with this connection
  // bear->setCertStore(&certStore);

  client = new PubSubClient(espClient);

  client->setServer(mqtt_server, mqtt_port);
  client->setCallback(callback);

  // put your setup code here, to run once:
  // pinMode(2, OUTPUT); // GPIO2 (D4) là OUTPUT
  // digitalWrite(2, LOW);
  // WiFi.begin(ssid, password);
  

  // if(!LittleFS.begin()) {
  //   Serial.print("Little mount failed!");
  //   return ;
  // }
  // Serial.print("LittleFS mount success!");
  // File file = LittleFS.open("/certs.ar", "w");
  // if(file) {
  //   Serial.print("opened file");
  //   return;
  // }
  // Serial.print("File opened successfully!");

  // file.print(R"EOF(
  //   -----BEGIN CERTIFICATE------
  //   ABCD
  //   -----END CERTIFICATE--------
  // )EOF");
  // file.close();
  // Serial.print("certificate save!");

}

void loop() {
  // put your main code here, to run repeatedly:
  if(!client->connected()){
    reconnect();
  }
  client->loop();
  unsigned long now = millis();
  
  if (now - lastSendTime >= interval) {
    lastSendTime = now;

    int soilMoisture = analogRead(A0);
  Serial.print("Raw: ");
  Serial.print(soilMoisture);
  

  float temperature = dht.readTemperature(); // doc nhiet do (*C)
  float humidity = dht.readHumidity();

  StaticJsonDocument<128> jsonDoc;
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  jsonDoc["soilMoisture"] = soilMoisture;
  jsonDoc["deviceId"] = DEVICEID;
  jsonDoc["userId"] = USERID;
  jsonDoc["gardenId"] = GARDENID;

  char buffer[128];
  serializeJson(jsonDoc, buffer);
  client->publish(TOPIC, buffer);
  digitalWrite(RELAY1,LOW);
  digitalWrite(RELAY2,LOW);
  
  delay(5000);
  digitalWrite(RELAY1,HIGH);
  digitalWrite(RELAY2,HIGH);
  }

  // Kiểm tra nhiều lịch hẹn tưới
  time_t nowTime = time(nullptr);
  struct tm *nowTm = localtime(&nowTime);
  for (int i = 0; i < scheduleCount; i++) {
    Schedule &sched = wateringSchedules[i];
    if (sched.enabled && sched.time > 0) {
      struct tm *schedTm = localtime(&sched.time);
      if (sched.repeat == "everyday") {
        if (nowTm->tm_hour == schedTm->tm_hour && nowTm->tm_min == schedTm->tm_min && !sched.wateredToday) {
          Serial.print("Tưới cây theo lịch hẹn số ");
          Serial.println(i+1);
          digitalWrite(RELAY1,LOW);
          digitalWrite(RELAY2,LOW);
          delay(5000);
          digitalWrite(RELAY1,HIGH);
          digitalWrite(RELAY2,HIGH);
          sched.wateredToday = true;
        }
        // Reset cờ vào đầu ngày mới
        if (nowTm->tm_hour == 0 && nowTm->tm_min == 0) sched.wateredToday = false;
      }
      // Có thể mở rộng cho các kiểu repeat khác
    }
  }
  
}
