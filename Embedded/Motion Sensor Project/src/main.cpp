#include <Arduino.h>

#define PIR_PIN 13
#define LED_PIN 25
#define BUZZER_PIN 14

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("Motion Sensor Ready");
}

void loop() {

  int motion = digitalRead(PIR_PIN);

  if (motion == HIGH) {
    Serial.println("Motion Detected!");

    digitalWrite(LED_PIN, HIGH);

    digitalWrite(BUZZER_PIN, HIGH);

    delay(3000);

  } 
  else {
    Serial.println("No Motion");

    digitalWrite(LED_PIN, LOW);

    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(100);
}