#include <TB6612_ESP32.h>

// Pines del ESP32 al TB6612FNG
#define AIN1 13
#define AIN2 14
#define PWMA 26
#define STBY 33

// Ajuste de dirección (1 o -1)
const int offsetA = 1;

// Solo un motor
Motor motor1 = Motor(AIN1, AIN2, PWMA, offsetA, STBY, 5000, 8, 1);

void setup() {
  Serial.begin(115200);   // Iniciamos el monitor serial
  Serial.println("Control del motor con comandos:");
  Serial.println("D = Derecha, I = Izquierda, S = Stop");
}

void loop() {
  if (Serial.available() > 0) {
    char comando = Serial.read();

    if (comando == 'D' || comando == 'd') {
      Serial.println("Motor girando a la derecha");
      motor1.drive(200);   // velocidad positiva
    } 
    else if (comando == 'I' || comando == 'i') {
      Serial.println("Motor girando a la izquierda");
      motor1.drive(-200);  // velocidad negativa
    } 
    else if (comando == 'S' || comando == 's') {
      Serial.println("Motor detenido");
      motor1.brake();      // freno
    }
  }
}
