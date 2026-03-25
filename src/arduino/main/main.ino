// main.ino - source code for APPS system
// Delveloped by Joseph Patrick
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <HX711.h>
#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>
#include <credentials.h>
#include <time_convert.h>
#include <test.h>

bool production = true;

// Pin definitions
#define AUGER_PIN 12
#define PUMP_PIN 13
#define WATER_SCALE_SCK 26
#define WATER_SCALE_DOUT 25
#define FOOD_SCALE_SCK 14
#define FOOD_SCALE_DOUT 27

// see calibrate() to get DIVIDER and OFFSET
#define FOOD_SCALE_OFFSET -368370.33
#define FOOD_SCALE_DIVIDER 429.94
#define WATER_SCALE_OFFSET 164968.66
#define WATER_SCALE_DIVIDER 442.57

// amount of food and water until full
#define FOOD_THRESHOLD_G 50
#define WATER_THRESHOLD_G 100

// how much food/water to give per cycle
#define FOOD_PRECISION 250 // ms of auger moving 
#define WATER_PRECISION 7000 // ms of water delivery
// too many cycles = out of food/water 
#define CYCLE_LIMIT 15

HX711 water_scale;
HX711 food_scale;

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "north-america.pool.ntp.org", -18000, 60000);

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\r---------------------------------------------\n\r"));

  pinMode(PUMP_PIN, OUTPUT);

  pinMode(AUGER_PIN, OUTPUT);

  water_scale.begin(WATER_SCALE_DOUT, WATER_SCALE_SCK);
  food_scale.begin(FOOD_SCALE_DOUT, FOOD_SCALE_SCK);
  water_scale.set_offset(WATER_SCALE_OFFSET);
  food_scale.set_offset(FOOD_SCALE_OFFSET);
  water_scale.set_scale(WATER_SCALE_DIVIDER);
  food_scale.set_scale(FOOD_SCALE_DIVIDER);

  if (!production) test_loop(timeClient, food_scale, water_scale, AUGER_PIN, PUMP_PIN);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { // Wait for connection
    delay(500);
    Serial.print(".");
  }
  Serial.print("\n");
  Serial.print("Connected to the Wi-Fi network: ");
  Serial.println(WiFi.SSID());
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();

  ssl_client.setInsecure();
}

void send_email(String body) {
  auto statusCallback = [](SMTPStatus status) {
    Serial.println(status.text);
  };

  int connect_counter = 0;
  for (int i = 0; i < 10; i++) {
    smtp.connect(EMAIL_SMTP_HOST, EMAIL_SMTP_PORT, statusCallback);
    if (smtp.isConnected()) break;
  }

  if (!smtp.isConnected()) return;

  smtp.authenticate(EMAIL_FROM, EMAIL_PASSWORD, readymail_auth_password);

  SMTPMessage msg;
  msg.headers.add(rfc822_from, EMAIL_FROM);
  msg.headers.add(rfc822_to, EMAIL_TO);
  msg.headers.add(rfc822_to, EMAIL_TO_2);
  msg.headers.add(rfc822_subject, "APPS Notification");
  msg.text.body(body);

  configTime(0, 0, "pool.ntp.org");
  while (time(nullptr) < 100000) delay(100);
  msg.timestamp = time(nullptr);

  smtp.send(msg);
}

void loop() {
  String times[2] = {"07:00", "18:00"}; // times to output provisions
  if (is_DST(timeClient.getEpochTime())) { timeClient.setTimeOffset(-14400); }
  else { timeClient.setTimeOffset(-18000); }
  String time = timeClient.getFormattedTime().substring(0,5);
  delay(10000);
  Serial.println(time);
  for (String t : times) { // loop through times and check if any of them are now
    if (t == time) {
      // add food until full
      int counter = 0; // counter if food runs out
      int bowlAmmount = food_scale.get_units(10);
      while (bowlAmmount < FOOD_THRESHOLD_G && counter < CYCLE_LIMIT) {
        Serial.print("Dispensing food: ");
        Serial.println(bowlAmmount);
        digitalWrite(AUGER_PIN, HIGH);
        delay(FOOD_PRECISION);
        digitalWrite(AUGER_PIN, LOW);
        counter++;
        bowlAmmount = food_scale.get_units(10);
      }

      // send email if refill needed
      if (counter >= CYCLE_LIMIT) send_email("Food Container is empty or obstruction detected, please remedy ASAP");

      // add water until full
      counter = 0; // reset counter
      bowlAmmount = water_scale.get_units(10);
      while (water_scale.get_units(10) < WATER_THRESHOLD_G && counter < CYCLE_LIMIT) {
        Serial.print("Dispensing water: ");
        Serial.println(bowlAmmount);
        digitalWrite(PUMP_PIN, HIGH);
        delay(WATER_PRECISION);
        digitalWrite(PUMP_PIN, LOW);
        counter++;
        bowlAmmount = water_scale.get_units(10);
      }

      // send email if refill needed
      if (counter >= CYCLE_LIMIT) send_email("Water Resivoir is empty or obstruction detected, please remedy ASAP");

      delay(60000); // wait for the minute to be over so it doesn't re-trigger
      timeClient.update(); // update to current time via NTP
    }
  }
}
