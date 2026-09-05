#include <Arduino.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

// ================= ESP-NOW =================
#define CMD_NORMAL 0
#define CMD_STOP 1
#define CMD_ALERT 2

// Guardian Master's STA MAC — commands are only accepted from this address.
// Fixed channel 6 to match, independent of the A34 AP's own state (this
// board never associates to A34 at all, only scans/pins its ESP-NOW radio).
static const uint8_t GUARDIAN_MASTER_MAC[6] = {0xE8, 0xF6, 0x0A, 0xD6, 0xC8, 0xE8};
#define GUARDIAN_ESPNOW_CHANNEL 6

// Guardian Master sends its commands as a raw guardian_command_t value
// (GuardianProtocol.h on the Master project — an unscoped C++ enum with no
// explicit underlying type, so it compiles to a plain 4-byte int). Not a
// shared header between these two projects, so mirrored here as a size
// constant rather than the type itself.
#define GUARDIAN_COMMAND_PACKET_SIZE 4

volatile bool robotStoppedByMaster = false;
uint8_t lastCommandReceived = CMD_NORMAL;

// ================= STATE =================
bool isCurrentlyMoving = false;
long lastCenterDist = 999;
long lastLeftDist = 999;
long lastRightDist = 999;

// ================= WIFI =================
const char *AP_SSID = "AEGIS_GUARDIAN";
const char *AP_PASSWORD = "AEGIS1234";

WebServer server(80);

// ================= PINS =================
#define PIN_PWMA 25
#define PIN_AIN1 26
#define PIN_AIN2 27
#define PIN_PWMB 32
#define PIN_BIN1 33
#define PIN_BIN2 14
#define PIN_STBY 13
#define PIN_SERVO 15
#define PIN_TRIG 21
#define PIN_ECHO 22

#define DISTANCE_THRESHOLD 20
#define MOTOR_SPEED 180

Servo sensorServo;

// ================= DECLARATIONS =================
void moveForward();
void moveBackward();
void turnLeft();
void turnRight();
void stopMotors();
long readUltrasonicDistance();
int scanPathsAndSelectBest();
void handleTelemetry();
void handleRoot();

// ================= ESP-NOW =================
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  // Only accept commands from Guardian Master, and only packets shaped like
  // an actual command (Guardian Master sends a raw guardian_command_t enum,
  // not the fuller guardian_message_t struct). This also rejects swarm
  // ESP-NOW traffic (42 bytes) that this Slave now overhears as a side
  // effect of sharing channel 6 with the swarm link — same MAC as Guardian
  // Master since swarm broadcasts originate from it too, so the size check
  // is what actually filters those out, not the MAC check alone.
  if (memcmp(mac, GUARDIAN_MASTER_MAC, 6) != 0) {
    return;
  }
  if (len != GUARDIAN_COMMAND_PACKET_SIZE && len != 1) {
    return;
  }

  uint8_t command = incomingData[0];
  lastCommandReceived = command;

  Serial.print("ESP-NOW command: ");
  Serial.println(command);

  if (command == CMD_STOP || command == CMD_ALERT) {
    robotStoppedByMaster = true;
    isCurrentlyMoving = false;
    Serial.println(">>> MASTER ALERT - ROBOT STOPPED");
  } else if (command == CMD_NORMAL) {
    robotStoppedByMaster = false;
    Serial.println(">>> MASTER NORMAL - ROBOT ACTIVE");
  }
}

void initESPNow() {
  // Fixed channel 6, independent of A34's own state — this board no longer
  // scans Wi-Fi for A34's SSID to infer a channel (that made ESP-NOW
  // dependent on A34 being up and reachable during boot for no real reason,
  // since this Slave never associates to it anyway).
  esp_wifi_set_channel(GUARDIAN_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onDataRecv);
  Serial.printf("ESP-NOW SLAVE READY on fixed Channel %d (Master: %02X:%02X:%02X:%02X:%02X:%02X)\n",
                GUARDIAN_ESPNOW_CHANNEL,
                GUARDIAN_MASTER_MAC[0], GUARDIAN_MASTER_MAC[1], GUARDIAN_MASTER_MAC[2],
                GUARDIAN_MASTER_MAC[3], GUARDIAN_MASTER_MAC[4], GUARDIAN_MASTER_MAC[5]);
}

// ================= TELEMETRY =================
void handleTelemetry() {
  String current_state = "PATROL";
  if (robotStoppedByMaster)
    current_state = "STOPPED";
  else if (isCurrentlyMoving)
    current_state = "MOVING";

  bool obstacle_center =
      (lastCenterDist <= DISTANCE_THRESHOLD && lastCenterDist > 0);
  bool obstacle_left = (lastLeftDist <= DISTANCE_THRESHOLD && lastLeftDist > 0);
  bool obstacle_right =
      (lastRightDist <= DISTANCE_THRESHOLD && lastRightDist > 0);

  String json = "{";
  json += "\"device_id\":\"guardian_slave_01\",";
  json += "\"status\":\"ONLINE\",";
  json += "\"current_state\":\"" + current_state + "\",";
  json += "\"battery_pct\":0,";
  json += "\"is_moving\":" + String(isCurrentlyMoving ? "true" : "false") + ",";
  json += "\"last_command\":" + String(lastCommandReceived) + ",";
  json += "\"distance_cm\":" + String(lastCenterDist) + ",";
  json += "\"left_dist_cm\":" + String(lastLeftDist) + ",";
  json += "\"right_dist_cm\":" + String(lastRightDist) + ",";
  json +=
      "\"obstacle_center\":" + String(obstacle_center ? "true" : "false") + ",";
  json += "\"obstacle_left\":" + String(obstacle_left ? "true" : "false") + ",";
  json +=
      "\"obstacle_right\":" + String(obstacle_right ? "true" : "false") + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleRoot() {
  server.send(200, "text/plain", "Guardian Slave OK - use /api/telemetry");
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Stop background scanning / channel hopping

  Serial.print("Slave STA MAC Address: ");
  Serial.println(WiFi.macAddress());

  initESPNow();

  pinMode(PIN_PWMA, OUTPUT);
  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_PWMB, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT);
  pinMode(PIN_BIN2, OUTPUT);
  pinMode(PIN_STBY, OUTPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_STBY, HIGH);

  ESP32PWM::allocateTimer(0);
  sensorServo.setPeriodHertz(50);
  sensorServo.attach(PIN_SERVO, 500, 2400);
  sensorServo.write(90);
  stopMotors();

  server.on("/", handleRoot);
  server.on("/api/telemetry", handleTelemetry);
  server.begin();
  Serial.println("HTTP server started → /api/telemetry");

  Serial.println("================================");
  Serial.println("SLAVE ROBOT READY");
  Serial.println("================================");
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  if (robotStoppedByMaster) {
    stopMotors();
    delay(200);
    return;
  }

  lastCenterDist = readUltrasonicDistance();
  Serial.print("Center: ");
  Serial.print(lastCenterDist);
  Serial.println(" cm");

  if (lastCenterDist > DISTANCE_THRESHOLD && lastCenterDist != 0) {
    moveForward();
  } else {
    stopMotors();
    delay(200);
    int turnDirection = scanPathsAndSelectBest();

    if (robotStoppedByMaster) {
      stopMotors();
      return;
    }

    if (turnDirection == 1) {
      turnRight();
      delay(400);
    } else if (turnDirection == -1) {
      turnLeft();
      delay(400);
    } else {
      moveBackward();
      delay(500);
      if (robotStoppedByMaster) {
        stopMotors();
        return;
      }
      turnRight();
      delay(600);
    }
    stopMotors();
    delay(200);
  }
  delay(30);
}

// ================= MOTORS =================
void moveForward() {
  isCurrentlyMoving = true;
  digitalWrite(PIN_AIN1, HIGH);
  digitalWrite(PIN_AIN2, LOW);
  analogWrite(PIN_PWMA, MOTOR_SPEED);
  digitalWrite(PIN_BIN1, HIGH);
  digitalWrite(PIN_BIN2, LOW);
  analogWrite(PIN_PWMB, MOTOR_SPEED);
}

void moveBackward() {
  isCurrentlyMoving = true;
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, HIGH);
  analogWrite(PIN_PWMA, MOTOR_SPEED);
  digitalWrite(PIN_BIN1, LOW);
  digitalWrite(PIN_BIN2, HIGH);
  analogWrite(PIN_PWMB, MOTOR_SPEED);
}

void turnLeft() {
  isCurrentlyMoving = true;
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, HIGH);
  analogWrite(PIN_PWMA, MOTOR_SPEED);
  digitalWrite(PIN_BIN1, HIGH);
  digitalWrite(PIN_BIN2, LOW);
  analogWrite(PIN_PWMB, MOTOR_SPEED);
}

void turnRight() {
  isCurrentlyMoving = true;
  digitalWrite(PIN_AIN1, HIGH);
  digitalWrite(PIN_AIN2, LOW);
  analogWrite(PIN_PWMA, MOTOR_SPEED);
  digitalWrite(PIN_BIN1, LOW);
  digitalWrite(PIN_BIN2, HIGH);
  analogWrite(PIN_PWMB, MOTOR_SPEED);
}

void stopMotors() {
  isCurrentlyMoving = false;
  analogWrite(PIN_PWMA, 0);
  analogWrite(PIN_PWMB, 0);
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, LOW);
  digitalWrite(PIN_BIN1, LOW);
  digitalWrite(PIN_BIN2, LOW);
}

// ================= ULTRASONIC =================
long readUltrasonicDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duration = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duration == 0)
    return 999;
  return duration * 0.0343 / 2;
}

int scanPathsAndSelectBest() {
  sensorServo.write(20);
  delay(400);
  lastRightDist = readUltrasonicDistance();
  if (robotStoppedByMaster) {
    sensorServo.write(90);
    return 0;
  }
  sensorServo.write(160);
  delay(400);
  lastLeftDist = readUltrasonicDistance();
  sensorServo.write(90);
  delay(300);

  Serial.print("Scan L:");
  Serial.print(lastLeftDist);
  Serial.print(" R:");
  Serial.println(lastRightDist);

  if (lastRightDist <= DISTANCE_THRESHOLD && lastLeftDist <= DISTANCE_THRESHOLD)
    return 0;
  if (lastRightDist >= lastLeftDist)
    return 1;
  return -1;
}