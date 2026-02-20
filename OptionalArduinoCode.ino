#include <Servo.h>

Servo servo1;
Servo servo2;
Servo servo3;

void setup() {
  Serial.begin(9600);

  servo1.attach(9);
  servo2.attach(8);
  servo3.attach(7);

  servo1.write(90);
  servo2.write(90);
  servo3.write(90);

  Serial.println("3-Servo Calibration Ready");
  Serial.println("Format: <servo_number> <angle>");
  Serial.println("Example: 1 90");
}

void loop() {
  if (Serial.available()) {
    int servoNum = Serial.parseInt();
    int angle = Serial.parseInt();

    angle = constrain(angle, 0, 180);

    switch (servoNum) {
      case 1:
        servo1.write(angle);
        Serial.print("Servo 1 -> ");
        Serial.println(angle);
        break;

      case 2:
        servo2.write(angle);
        Serial.print("Servo 2 -> ");
        Serial.println(angle);
        break;

      case 3:
        servo3.write(angle);
        Serial.print("Servo 3 -> ");
        Serial.println(angle);
        break;

      default:
        Serial.println("Invalid servo number (use 1, 2, or 3)");
    }

    // clear serial buffer
    while (Serial.available()) Serial.read();
  }
}