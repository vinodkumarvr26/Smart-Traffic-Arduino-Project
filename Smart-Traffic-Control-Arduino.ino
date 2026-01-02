#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 5
#define SS_PIN 53
MFRC522 rfid(SS_PIN, RST_PIN);

#define NUM_LANES 4
const int trigPins[NUM_LANES] = {22, 23, 24, 25};
const int echoPins[NUM_LANES] = {26, 27, 28, 29};
const int lightPins[NUM_LANES][3] = {
  {30, 31, 32},  // Lane 1: Green, Yellow, Red
  {33, 34, 35},
  {36, 37, 38},
  {42, 43, 41}
};

const String rfidUIDs[NUM_LANES] = {
  "9DA79F3C", // Lane 1
  "F4FEC8AF", // Lane 2
  "E74EF8B4", // Lane 3
  "056BC001"  // Lane 4
};

const int greenTime = 5000;
const int yellowTime = 2000;
const int emergencyTime = 8000;
const int vehicleThreshold = 20;

int currentLane = 0;
bool inEmergency = false;
bool vehicleDetected[NUM_LANES] = {false, false, false, false};  // To track vehicle detection state
bool rfidVerified[NUM_LANES] = {false, false, false, false};    // To track RFID verification

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  for (int i = 0; i < NUM_LANES; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
    for (int j = 0; j < 3; j++) {
      pinMode(lightPins[i][j], OUTPUT);
    }
  }
  Serial.println("System Ready");
}

long getDistanceCM(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 25000);
  return duration * 0.034 / 2;
}

bool isVehiclePresent(int lane) {
  long d = getDistanceCM(trigPins[lane], echoPins[lane]);
  return (d > 0 && d < vehicleThreshold);
}

String getScannedUID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return "";
  }
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return uid;
}

void setAllRed() {
  for (int i = 0; i < NUM_LANES; i++) {
    digitalWrite(lightPins[i][0], LOW);
    digitalWrite(lightPins[i][1], LOW);
    digitalWrite(lightPins[i][2], HIGH);
  }
}

void setLaneGreen(int lane) {
  setAllRed();
  digitalWrite(lightPins[lane][0], HIGH);
  digitalWrite(lightPins[lane][2], LOW);
}

void setLaneYellow(int lane) {
  setAllRed();
  digitalWrite(lightPins[lane][1], HIGH);
  digitalWrite(lightPins[lane][2], LOW);
}

void loop() {
  // Print distances from each sensor
  for (int i = 0; i < NUM_LANES; i++) {
    long dist = getDistanceCM(trigPins[i], echoPins[i]);
    Serial.print("Lane ");
    Serial.print(i + 1);
    Serial.print(" Distance: ");
    Serial.print(dist);
    Serial.println(" cm");

    // Vehicle detection
    if (dist < vehicleThreshold && !vehicleDetected[i]) {
      vehicleDetected[i] = true;
      Serial.print("Vehicle detected in Lane ");
      Serial.println(i + 1);
      rfidVerified[i] = false;  // Reset RFID status for this lane
    } else if (dist >= vehicleThreshold && vehicleDetected[i]) {
      vehicleDetected[i] = false;
      rfidVerified[i] = false;  // Reset RFID status when no vehicle is present
    }
  }

  // Check for RFID tag only if vehicle is detected in a lane
  if (!inEmergency) {
    String uid = getScannedUID();
    for (int i = 0; i < NUM_LANES; i++) {
      if (vehicleDetected[i] && !rfidVerified[i] && uid == rfidUIDs[i]) {
        Serial.print("EMERGENCY OVERRIDE: Lane ");
        Serial.println(i + 1);
        rfidVerified[i] = true;  // Mark RFID verified
        inEmergency = true;
        setLaneGreen(i);
        delay(emergencyTime);
        inEmergency = false;
        break;
      }
    }
  }

  // Normal round-robin cycle
  if (!inEmergency) {
    Serial.print("Normal Cycle: Lane ");
    Serial.println(currentLane + 1);
    setLaneGreen(currentLane);
    delay(greenTime);

    setLaneYellow(currentLane);
    delay(yellowTime);

    currentLane = (currentLane + 1) % NUM_LANES;
  }
}