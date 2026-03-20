// main.ino - source code for APPS system
// Delveloped by Joseph Patrick
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP_Mail_Client.h>
#include <HX711.h>

#include <time_convert.h>
#include <test.h>

#include <credentials.h>

bool test = true;

// Pin definitions
#define AUGER_PIN 12
#define PUMP_PIN 13
#define WATER_SCALE_SCK 26
#define WATER_SCALE_DOUT 25
#define FOOD_SCALE_SCK 14
#define FOOD_SCALE_DOUT 27

// see calibrate() to get DIVIDER and OFFSET
#define FOOD_SCALE_OFFSET -368175
#define FOOD_SCALE_DIVIDER 427.33
#define WATER_SCALE_OFFSET 169276.5
#define WATER_SCALE_DIVIDER 438.16

// amount of food and water until full
#define FOOD_THRESHOLD_G 40
#define WATER_THRESHOLD_G 160

// how much food/water to give per cycle
#define FOOD_PRECISION 5000 // ms of auger moving 
#define WATER_PRECISION 5000 // ms of water delivery
// too many cycles = out of food/water 
#define CYCLE_LIMIT 30

HX711 water_scale;
HX711 food_scale;

SMTPSession smtp;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "north-america.pool.ntp.org", -18000, 60000);

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\r---------------------------------------------\n\r"));

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

  pinMode(PUMP_PIN, OUTPUT);

  pinMode(AUGER_PIN, OUTPUT);

  water_scale.begin(WATER_SCALE_DOUT, WATER_SCALE_SCK);
  food_scale.begin(FOOD_SCALE_DOUT, FOOD_SCALE_SCK);
  water_scale.set_offset(WATER_SCALE_OFFSET);
  food_scale.set_offset(FOOD_SCALE_OFFSET);
  water_scale.set_scale(WATER_SCALE_DIVIDER);
  food_scale.set_scale(FOOD_SCALE_DIVIDER);
}

void send_email(String body) {
  ESP_Mail_Session session;
  session.server.host_name = EMAIL_SMTP_HOST;
  session.server.port = EMAIL_SMTP_PORT;
  session.login.email = EMAIL_FROM;
  session.login.password = EMAIL_PASSWORD;

  SMTP_Message message;
  message.sender.email = EMAIL_FROM;
  message.subject = "APPS Notification";
  message.addRecipient("", EMAIL_TO);
  message.addRecipient("", EMAIL_TO_2);
  message.html.content = body;

  smtp.connect(&session);
  MailClient.sendMail(&smtp, &message);
}

void loop() {
  String times[2] = {"07:00", "18:00"}; // times to output provisions
  if (is_DST(timeClient.getEpochTime())) { timeClient.setTimeOffset(-14400); }
  else { timeClient.setTimeOffset(-18000); }
  String time = timeClient.getFormattedTime().substring(0,5);
  if (test) test_loop(timeClient, food_scale, water_scale, AUGER_PIN, PUMP_PIN);
  delay(5000);
  Serial.println(time);
  for (String t : times) { // loop through times and check if any of them are now
    if (t == time) {
      // add food until full
      int counter = 0; // counter if food runs out
      while (food_scale.get_units(10) < FOOD_THRESHOLD_G && counter < CYCLE_LIMIT) {
        digitalWrite(AUGER_PIN, HIGH);
        delay(FOOD_PRECISION);
        digitalWrite(AUGER_PIN, LOW);
        counter++;
      }

      // send email if refill needed
      if (! counter < CYCLE_LIMIT) send_email("Food Container is empty, please refill ASAP");

      // add water until full
      counter = 0; // reset counter
      while (water_scale.get_units(10) < WATER_THRESHOLD_G && counter < CYCLE_LIMIT) {
        digitalWrite(PUMP_PIN, HIGH);
        delay(WATER_PRECISION);
        digitalWrite(PUMP_PIN, LOW);
        counter++;
      }

      // send email if refill needed
      if (! counter < CYCLE_LIMIT) send_email("Water Container is empty, please refill ASAP");

      delay(60000); // wait for the minute to be over so it doesn't re-trigger
      timeClient.update(); // update to current time via NTP
    }
  }
}
