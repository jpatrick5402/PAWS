#include <calibrate.h>

void test_loop(NTPClient timeClient, HX711 food_scale, HX711 water_scale, int AUGER_PIN, int PUMP_PIN) {
  // comment out any lines you don't want to test
  while (true) {
    if (is_DST(timeClient.getEpochTime())) { timeClient.setTimeOffset(-14400); }
    else { timeClient.setTimeOffset(-18000); }

    Serial.println(timeClient.getFormattedTime());


    unsigned long epoch = timeClient.getEpochTime();
    Serial.printf("%d/%d/%d : ", month(epoch), day_of_month(epoch), year(epoch));
    if (is_DST(epoch)) Serial.println("It IS DST");
    else Serial.println("It's NOT DST");
    delay(1000);

    calibrate(food_scale, "Food Scale");
    calibrate(water_scale, "Water Scale");

    digitalWrite(PUMP_PIN, HIGH);
    delay(3000);
    digitalWrite(PUMP_PIN, LOW);
    delay(1000);

    digitalWrite(AUGER_PIN, HIGH);
    delay(3000);
    digitalWrite(AUGER_PIN, LOW);
    delay(1000);

    Serial.print("Water: ");
    Serial.println(water_scale.get_units(10));
    delay(1000);

    Serial.print("Food: ");
    Serial.println(food_scale.get_units(10));
    delay(1000);
  }
}
