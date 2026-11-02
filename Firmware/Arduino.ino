#include <Servo.h>

Servo servo;

#define SERVO_PIN 9

void setup() {
  Serial.begin(115200);

  servo.attach(SERVO_PIN);

  servo.write(90);
  delay(500);
}

void loop() {

  if (Serial.available()) {

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("ANGLE:")) {

      float angle = cmd.substring(6).toFloat();

      // Limit to 0–180°
      angle = constrain(angle, 0, 180);

      servo.write((int)round(angle));

      Serial.print("SERVO=");
      Serial.println((int)round(angle));
    }
  }
}
