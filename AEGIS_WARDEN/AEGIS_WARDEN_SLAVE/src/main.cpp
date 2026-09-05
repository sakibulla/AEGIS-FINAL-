#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "warden_protocol.h"


// ============================================================
// PIN DEFINITIONS
// ============================================================

// TB6612 Motor Driver

#define PIN_PWMA 25
#define PIN_AIN1 26
#define PIN_AIN2 27

#define PIN_PWMB 32
#define PIN_BIN1 33
#define PIN_BIN2 14

#define PIN_STBY 13


// Servo

#define PIN_SERVO 15


// HC-SR04

#define PIN_TRIG 21
#define PIN_ECHO 22


// Gas Sensor

#define PIN_GAS_SENSOR 34


// ============================================================
// ROBOT CONSTANTS
// ============================================================

#define DISTANCE_THRESHOLD 20

#define MOTOR_SPEED 180


// ============================================================
// GAS SENSOR
// ============================================================

#define GAS_THRESHOLD 650

#define GAS_CHECK_INTERVAL 500


// ============================================================
// MOVEMENT CYCLE
// ============================================================

#define STATIONARY_DURATION 60000UL
#define MOVEMENT_DURATION 120000UL


// ============================================================
// ULTRASONIC
// ============================================================

#define USE_ULTRASONIC true


// ============================================================
// ESP-NOW
// ============================================================

#define ESPNOW_CHANNEL 6

// Warden Master's STA MAC — needed so the Slave can register it as an
// ESP-NOW peer and send status packets back (this link was previously
// Master -> Slave only, so the Slave never needed the Master's MAC before).
static const uint8_t WARDEN_MASTER_MAC[6] = {0x94, 0xA9, 0x90, 0x0A, 0xEF, 0x6C};


// ============================================================
// WARDEN SAFETY PAUSE
// ============================================================

#define FIRE_SMOKE_PAUSE_TIME 5000UL


// ============================================================
// GLOBAL VARIABLES
// ============================================================

Servo sensorServo;


// ============================================================
// GAS STATE
// ============================================================

unsigned long lastGasCheckTime = 0;

bool gasDetected = false;

uint16_t gasStatusSequence = 0;


// ============================================================
// MOVEMENT STATE
// ============================================================

unsigned long cycleStartTime = 0;

bool isMovementPhase = false;


// ============================================================
// WARDEN ALERT STATE
// ============================================================

volatile bool fireSmokeAlert = false;

volatile warden_command_t receivedCommand =
    WARDEN_CMD_CLEAR;

volatile uint16_t lastReceivedSequence = 0;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void moveForward();

void moveBackward();

void turnLeft();

void turnRight();

void stopMotors();


long readUltrasonicDistance();

int scanPathsAndSelectBest();


int readGasSensor();

void checkGasLevel();

void sendGasStatusToMaster(bool detected, int gasValue);


void initEspNow();


void handleWardenCommand(
    warden_command_t command,
    float fireConfidence,
    float smokeConfidence
);


const char *commandName(
    warden_command_t command
);


// ============================================================
// COMMAND NAME
// ============================================================

const char *commandName(
    warden_command_t command)
{
    switch (command)
    {
        case WARDEN_CMD_FIRE:
            return "FIRE";

        case WARDEN_CMD_SMOKE:
            return "SMOKE";

        case WARDEN_CMD_FIRE_SMOKE:
            return "FIRE + SMOKE";

        case WARDEN_CMD_CLEAR:
        default:
            return "CLEAR";
    }
}


// ============================================================
// ESP-NOW RECEIVE CALLBACK
// ============================================================

void onDataRecv(
    const uint8_t *mac,
    const uint8_t *incomingData,
    int len)
{
    (void)mac;

    if (incomingData == nullptr)
    {
        return;
    }


    // ========================================================
    // CHECK PACKET SIZE
    // ========================================================

    if (len != sizeof(warden_packet_t))
    {
        Serial.print(
            "ESP-NOW packet size mismatch. Received: "
        );

        Serial.print(len);

        Serial.print(
            " Expected: "
        );

        Serial.println(
            sizeof(warden_packet_t)
        );

        return;
    }


    // ========================================================
    // COPY PACKET
    // ========================================================

    warden_packet_t packet;

    memcpy(
        &packet,
        incomingData,
        sizeof(packet)
    );


    // ========================================================
    // CHECK PROTOCOL VERSION
    // ========================================================

    if (
        packet.version !=
        WARDEN_PROTOCOL_VERSION
    )
    {
        Serial.print(
            "Invalid Warden protocol version: "
        );

        Serial.println(
            packet.version
        );

        return;
    }


    // ========================================================
    // CHECK SOURCE
    // ========================================================

    if (
        packet.source !=
        WARDEN_SOURCE_ID
    )
    {
        Serial.print(
            "Ignoring packet from source: "
        );

        Serial.println(
            packet.source
        );

        return;
    }


    // ========================================================
    // IGNORE DUPLICATE PACKETS
    // ========================================================

    if (
        packet.sequence ==
        lastReceivedSequence
    )
    {
        return;
    }

    lastReceivedSequence =
        packet.sequence;


    // ========================================================
    // VALIDATE COMMAND
    // ========================================================

    if (
        packet.command >
        WARDEN_CMD_FIRE_SMOKE
    )
    {
        Serial.print(
            "Invalid Warden command: "
        );

        Serial.println(
            packet.command
        );

        return;
    }


    // ========================================================
    // CONVERT CONFIDENCE
    // ========================================================

    float fireConfidence =
        packet.fire_confidence_x1000 /
        1000.0f;


    float smokeConfidence =
        packet.smoke_confidence_x1000 /
        1000.0f;


    // ========================================================
    // PRINT RECEIVED PACKET
    // ========================================================

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "     WARDEN ESP-NOW MESSAGE"
    );

    Serial.println(
        "================================="
    );


    Serial.print(
        "Protocol version: "
    );

    Serial.println(
        packet.version
    );


    Serial.print(
        "Source: "
    );

    Serial.println(
        packet.source
    );


    Serial.print(
        "Command: "
    );

    Serial.println(
        commandName(
            (warden_command_t)
                packet.command
        )
    );


    Serial.print(
        "Sequence: "
    );

    Serial.println(
        packet.sequence
    );


    Serial.print(
        "Fire confidence: "
    );

    Serial.println(
        fireConfidence,
        3
    );


    Serial.print(
        "Smoke confidence: "
    );

    Serial.println(
        smokeConfidence,
        3
    );


    Serial.println(
        "================================="
    );


    // ========================================================
    // HANDLE COMMAND
    // ========================================================

    handleWardenCommand(
        (warden_command_t)
            packet.command,

        fireConfidence,

        smokeConfidence
    );
}


// ============================================================
// HANDLE WARDEN COMMAND
// ============================================================

void handleWardenCommand(
    warden_command_t command,
    float fireConfidence,
    float smokeConfidence)
{
    receivedCommand =
        command;


    // ========================================================
    // CLEAR
    // ========================================================

    if (
        command ==
        WARDEN_CMD_CLEAR
    )
    {
        fireSmokeAlert =
            false;


        stopMotors();


        Serial.println();

        Serial.println(
            "================================="
        );

        Serial.println(
            "WARDEN ALERT CLEARED"
        );

        Serial.println(
            "Robot operation normal"
        );

        Serial.println(
            "=================================");


        return;
    }


    // ========================================================
    // FIRE / SMOKE ALERT
    // ========================================================

    fireSmokeAlert =
        true;


    // Immediately stop robot

    stopMotors();


    Serial.println();

    Serial.println(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    );


    // ========================================================
    // FIRE
    // ========================================================

    if (
        command ==
        WARDEN_CMD_FIRE
    )
    {
        Serial.println(
            "FIRE DETECTED!"
        );
    }


    // ========================================================
    // SMOKE
    // ========================================================

    else if (
        command ==
        WARDEN_CMD_SMOKE
    )
    {
        Serial.println(
            "SMOKE DETECTED!"
        );
    }


    // ========================================================
    // FIRE + SMOKE
    // ========================================================

    else if (
        command ==
        WARDEN_CMD_FIRE_SMOKE
    )
    {
        Serial.println(
            "FIRE + SMOKE DETECTED!"
        );
    }


    // ========================================================
    // CONFIDENCE
    // ========================================================

    Serial.print(
        "Fire confidence: "
    );

    Serial.print(
        fireConfidence * 100.0f
    );

    Serial.println("%");


    Serial.print(
        "Smoke confidence: "
    );

    Serial.print(
        smokeConfidence * 100.0f
    );

    Serial.println("%");


    Serial.println(
        "ROBOT PAUSED!"
    );


    Serial.println(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    );


    // ========================================================
    // SAFETY PAUSE
    // ========================================================

    unsigned long pauseStart =
        millis();


    while (
        millis() -
        pauseStart <
        FIRE_SMOKE_PAUSE_TIME
    )
    {
        stopMotors();

        checkGasLevel();

        delay(50);
    }


    // ========================================================
    // KEEP ALERT ACTIVE UNTIL MASTER SENDS CLEAR
    // ========================================================

    /*
     * IMPORTANT:
     *
     * We DO NOT clear fireSmokeAlert here.
     *
     * The master must send WARDEN_CMD_CLEAR.
     *
     * Therefore the robot remains stopped after
     * the 5 second pause until the master confirms
     * that fire/smoke is no longer detected.
     */

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "5-second safety pause completed"
    );

    Serial.println(
        "WAITING FOR WARDEN CLEAR..."
    );

    Serial.println(
        "Robot remains STOPPED"
    );

    Serial.println(
        "================================="
    );
}


// ============================================================
// ESP-NOW INITIALIZATION
// ============================================================

void initEspNow()
{
    Serial.println();

    Serial.println(
        "Initializing ESP-NOW..."
    );


    // ========================================================
    // WIFI STATION MODE
    // ========================================================

    WiFi.mode(
        WIFI_STA
    );

    delay(100);


    // ========================================================
    // PRINT SLAVE MAC
    // ========================================================

    Serial.print(
        "Slave MAC Address: "
    );

    Serial.println(
        WiFi.macAddress()
    );


    // ========================================================
    // SET ESP-NOW CHANNEL
    // ========================================================

    esp_err_t channelResult =
        esp_wifi_set_channel(
            ESPNOW_CHANNEL,
            WIFI_SECOND_CHAN_NONE
        );


    if (
        channelResult != ESP_OK
    )
    {
        Serial.print(
            "Failed to set WiFi channel: "
        );

        Serial.println(
            esp_err_to_name(
                channelResult
            )
        );
    }
    else
    {
        Serial.print(
            "ESP-NOW Channel: "
        );

        Serial.println(
            ESPNOW_CHANNEL
        );
    }


    // ========================================================
    // INITIALIZE ESP-NOW
    // ========================================================

    esp_err_t result =
        esp_now_init();


    if (
        result != ESP_OK
    )
    {
        Serial.print(
            "ESP-NOW initialization failed: "
        );

        Serial.println(
            esp_err_to_name(result)
        );

        return;
    }


    // ========================================================
    // REGISTER RECEIVE CALLBACK
    // ========================================================

    result =
        esp_now_register_recv_cb(
            onDataRecv
        );


    if (
        result != ESP_OK
    )
    {
        Serial.print(
            "ESP-NOW callback registration failed: "
        );

        Serial.println(
            esp_err_to_name(result)
        );

        return;
    }


    // ========================================================
    // REGISTER MASTER AS PEER
    //
    // This link was previously Master -> Slave only — the Slave never sent
    // anything back, so it never needed the Master registered as a peer.
    // Required now to send warden_slave_status_t (gas sensor) back up.
    // ========================================================

    esp_now_peer_info_t masterPeer = {};

    memcpy(
        masterPeer.peer_addr,
        WARDEN_MASTER_MAC,
        6
    );

    masterPeer.channel = ESPNOW_CHANNEL;
    masterPeer.encrypt = false;

    if (!esp_now_is_peer_exist(WARDEN_MASTER_MAC))
    {
        esp_err_t peerResult = esp_now_add_peer(&masterPeer);

        if (peerResult != ESP_OK)
        {
            Serial.print(
                "Failed to add Master as ESP-NOW peer: "
            );

            Serial.println(
                esp_err_to_name(peerResult)
            );
        }
    }


    // ========================================================
    // VERIFY ACTUAL CHANNEL
    // ========================================================

    uint8_t primary = 0;

    wifi_second_chan_t second =
        WIFI_SECOND_CHAN_NONE;


    esp_wifi_get_channel(
        &primary,
        &second
    );


    Serial.print(
        "Actual WiFi channel: "
    );

    Serial.println(
        primary
    );


    Serial.println(
        "ESP-NOW initialized successfully."
    );


    Serial.println(
        "Waiting for Warden Master..."
    );
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(
        115200
    );


    delay(1000);


    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "      A.E.G.I.S. ROBOT SLAVE"
    );

    Serial.println(
        "================================="
    );


    // ========================================================
    // MOTOR DRIVER
    // ========================================================

    pinMode(
        PIN_PWMA,
        OUTPUT
    );

    pinMode(
        PIN_AIN1,
        OUTPUT
    );

    pinMode(
        PIN_AIN2,
        OUTPUT
    );


    pinMode(
        PIN_PWMB,
        OUTPUT
    );

    pinMode(
        PIN_BIN1,
        OUTPUT
    );

    pinMode(
        PIN_BIN2,
        OUTPUT
    );


    pinMode(
        PIN_STBY,
        OUTPUT
    );


    // ========================================================
    // ULTRASONIC
    // ========================================================

    pinMode(
        PIN_TRIG,
        OUTPUT
    );

    pinMode(
        PIN_ECHO,
        INPUT
    );


    // ========================================================
    // GAS SENSOR
    // ========================================================

    pinMode(
        PIN_GAS_SENSOR,
        INPUT
    );


    // ========================================================
    // ENABLE TB6612
    // ========================================================

    digitalWrite(
        PIN_STBY,
        HIGH
    );


    // ========================================================
    // SERVO
    // ========================================================

    ESP32PWM::allocateTimer(0);

    sensorServo.setPeriodHertz(
        50
    );


    sensorServo.attach(
        PIN_SERVO,
        500,
        2400
    );


    sensorServo.write(
        90
    );


    delay(1000);


    // ========================================================
    // MOTORS STOPPED AT BOOT
    // ========================================================

    stopMotors();


    // ========================================================
    // ESP-NOW
    // ========================================================

    initEspNow();


    // ========================================================
    // SYSTEM INFORMATION
    // ========================================================

    Serial.println();

    Serial.println(
        "================================="
    );


    Serial.print(
        "ESP32 MAC Address: "
    );

    Serial.println(
        WiFi.macAddress()
    );


    Serial.print(
        "Protocol packet size: "
    );

    Serial.print(
        sizeof(warden_packet_t)
    );

    Serial.println(
        " bytes"
    );


    Serial.print(
        "Expected Master Source ID: "
    );

    Serial.println(
        WARDEN_SOURCE_ID
    );


    Serial.print(
        "Protocol Version: "
    );

    Serial.println(
        WARDEN_PROTOCOL_VERSION
    );


    Serial.print(
        "ESP-NOW Channel: "
    );

    Serial.println(
        ESPNOW_CHANNEL
    );


    Serial.println(
        "================================="
    );


    Serial.println(
        "System Initialized."
    );


    Serial.println(
        "Gas Monitoring Cycle Started:"
    );


    Serial.println(
        "- Stationary for 1 minute"
    );


    Serial.println(
        "- Movement for 2 minutes"
    );


#if USE_ULTRASONIC

    Serial.println(
        "- Ultrasonic sensor: ENABLED"
    );

#else

    Serial.println(
        "- Ultrasonic sensor: DISABLED"
    );

#endif


    Serial.println(
        "- Warden FIRE/SMOKE detection: ENABLED"
    );


    Serial.println(
        "- Waiting for MASTER alerts"
    );


    Serial.println(
        "================================="
    );


    // ========================================================
    // START STATIONARY
    // ========================================================

    cycleStartTime =
        millis();


    isMovementPhase =
        false;


    Serial.println(
        ">>> STATIONARY PHASE - Monitoring gas..."
    );
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
    // ========================================================
    // GAS MONITORING
    // ========================================================

    checkGasLevel();


    // ========================================================
    // WARDEN FIRE/SMOKE SAFETY
    // ========================================================

    if (
        fireSmokeAlert
    )
    {
        stopMotors();

        delay(50);

        return;
    }


    // ========================================================
    // CURRENT TIME
    // ========================================================

    unsigned long currentTime =
        millis();


    unsigned long elapsed =
        currentTime -
        cycleStartTime;


    // ========================================================
    // STATIONARY -> MOVEMENT
    // ========================================================

    if (
        !isMovementPhase &&
        elapsed >=
            STATIONARY_DURATION
    )
    {
        isMovementPhase =
            true;


        cycleStartTime =
            currentTime;


        Serial.println();

        Serial.println(
            "================================="
        );


        Serial.println(
            ">>> MOVEMENT PHASE - Exploring..."
        );


        Serial.println(
            "================================="
        );
    }


    // ========================================================
    // MOVEMENT -> STATIONARY
    // ========================================================

    else if (
        isMovementPhase &&
        elapsed >=
            MOVEMENT_DURATION
    )
    {
        isMovementPhase =
            false;


        cycleStartTime =
            currentTime;


        stopMotors();


        Serial.println();

        Serial.println(
            "================================="
        );


        Serial.println(
            ">>> STATIONARY PHASE - Monitoring gas..."
        );


        Serial.println(
            "================================="
        );
    }


    // ========================================================
    // MOVEMENT
    // ========================================================

    if (
        isMovementPhase
    )
    {

#if USE_ULTRASONIC

        // ====================================================
        // CENTER DISTANCE
        // ====================================================

        long centerDistance =
            readUltrasonicDistance();


        Serial.print(
            "Center Distance: "
        );


        Serial.print(
            centerDistance
        );


        Serial.println(
            " cm"
        );


        // ====================================================
        // PATH CLEAR
        // ====================================================

        if (
            centerDistance >
                DISTANCE_THRESHOLD &&
            centerDistance != 0 &&
            centerDistance < 500
        )
        {
            moveForward();
        }


        // ====================================================
        // SENSOR TIMEOUT
        // ====================================================

        else if (
            centerDistance == 999 ||
            centerDistance > 500
        )
        {
            moveForward();
        }


        // ====================================================
        // OBSTACLE
        // ====================================================

        else
        {
            stopMotors();

            delay(200);


            int turnDirection =
                scanPathsAndSelectBest();


            // =================================================
            // RIGHT
            // =================================================

            if (
                turnDirection == 1
            )
            {
                Serial.println(
                    "Turning Right..."
                );


                turnRight();

                delay(400);
            }


            // =================================================
            // LEFT
            // =================================================

            else if (
                turnDirection == -1
            )
            {
                Serial.println(
                    "Turning Left..."
                );


                turnLeft();

                delay(400);
            }


            // =================================================
            // BOTH BLOCKED
            // =================================================

            else
            {
                Serial.println(
                    "Path blocked. Backing up..."
                );


                moveBackward();

                delay(500);


                turnRight();

                delay(600);
            }


            stopMotors();

            delay(200);
        }

#else

        // ====================================================
        // SIMPLE MOVEMENT MODE
        // ====================================================

        static unsigned long lastMoveChange =
            0;


        static int movePattern =
            0;


        if (
            currentTime -
            lastMoveChange >
            3000
        )
        {
            lastMoveChange =
                currentTime;


            movePattern =
                (movePattern + 1) % 4;


            switch (
                movePattern
            )
            {
                case 0:

                    moveForward();

                    break;


                case 1:

                    moveForward();

                    break;


                case 2:

                    turnLeft();

                    break;


                case 3:

                    turnRight();

                    break;
            }
        }

#endif
    }


    // ========================================================
    // STATIONARY
    // ========================================================

    else
    {
        stopMotors();


        static unsigned long lastStatusPrint =
            0;


        if (
            currentTime -
            lastStatusPrint >
            30000
        )
        {
            lastStatusPrint =
                currentTime;


            unsigned long remainingTime =
                STATIONARY_DURATION -
                elapsed;


            Serial.print(
                "Stationary time remaining: "
            );


            Serial.print(
                remainingTime / 60000
            );


            Serial.print(
                " min "
            );


            Serial.print(
                (remainingTime % 60000) /
                1000
            );


            Serial.println(
                " sec"
            );
        }
    }


    delay(50);
}


// ============================================================
// MOTOR CONTROL
// ============================================================

void moveForward()
{
    digitalWrite(
        PIN_AIN1,
        HIGH
    );


    digitalWrite(
        PIN_AIN2,
        LOW
    );


    analogWrite(
        PIN_PWMA,
        MOTOR_SPEED
    );


    digitalWrite(
        PIN_BIN1,
        HIGH
    );


    digitalWrite(
        PIN_BIN2,
        LOW
    );


    analogWrite(
        PIN_PWMB,
        MOTOR_SPEED
    );
}


// ============================================================

void moveBackward()
{
    digitalWrite(
        PIN_AIN1,
        LOW
    );


    digitalWrite(
        PIN_AIN2,
        HIGH
    );


    analogWrite(
        PIN_PWMA,
        MOTOR_SPEED
    );


    digitalWrite(
        PIN_BIN1,
        LOW
    );


    digitalWrite(
        PIN_BIN2,
        HIGH
    );


    analogWrite(
        PIN_PWMB,
        MOTOR_SPEED
    );
}


// ============================================================

void turnLeft()
{
    digitalWrite(
        PIN_AIN1,
        LOW
    );


    digitalWrite(
        PIN_AIN2,
        HIGH
    );


    analogWrite(
        PIN_PWMA,
        MOTOR_SPEED
    );


    digitalWrite(
        PIN_BIN1,
        HIGH
    );


    digitalWrite(
        PIN_BIN2,
        LOW
    );


    analogWrite(
        PIN_PWMB,
        MOTOR_SPEED
    );
}


// ============================================================

void turnRight()
{
    digitalWrite(
        PIN_AIN1,
        HIGH
    );


    digitalWrite(
        PIN_AIN2,
        LOW
    );


    analogWrite(
        PIN_PWMA,
        MOTOR_SPEED
    );


    digitalWrite(
        PIN_BIN1,
        LOW
    );


    digitalWrite(
        PIN_BIN2,
        HIGH
    );


    analogWrite(
        PIN_PWMB,
        MOTOR_SPEED
    );
}


// ============================================================

void stopMotors()
{
    analogWrite(
        PIN_PWMA,
        0
    );


    analogWrite(
        PIN_PWMB,
        0
    );
}


// ============================================================
// ULTRASONIC
// ============================================================

long readUltrasonicDistance()
{
    digitalWrite(
        PIN_TRIG,
        LOW
    );


    delayMicroseconds(2);


    digitalWrite(
        PIN_TRIG,
        HIGH
    );


    delayMicroseconds(10);


    digitalWrite(
        PIN_TRIG,
        LOW
    );


    long duration =
        pulseIn(
            PIN_ECHO,
            HIGH,
            30000
        );


    if (
        duration == 0
    )
    {
        return 999;
    }


    return duration *
           0.0343 /
           2;
}


// ============================================================
// PATH SCAN
// ============================================================

int scanPathsAndSelectBest()
{
    // ========================================================
    // RIGHT
    // ========================================================

    sensorServo.write(
        20
    );


    delay(500);


    long rightDist =
        readUltrasonicDistance();


    // ========================================================
    // LEFT
    // ========================================================

    sensorServo.write(
        160
    );


    delay(500);


    long leftDist =
        readUltrasonicDistance();


    // ========================================================
    // CENTER
    // ========================================================

    sensorServo.write(
        90
    );


    delay(400);


    Serial.print(
        "Scan - Left: "
    );


    Serial.print(
        leftDist
    );


    Serial.print(
        " cm | Right: "
    );


    Serial.print(
        rightDist
    );


    Serial.println(
        " cm"
    );


    // ========================================================
    // BOTH BLOCKED
    // ========================================================

    if (
        rightDist <=
            DISTANCE_THRESHOLD &&
        leftDist <=
            DISTANCE_THRESHOLD
    )
    {
        return 0;
    }


    // ========================================================
    // RIGHT
    // ========================================================

    if (
        rightDist >=
        leftDist
    )
    {
        return 1;
    }


    // ========================================================
    // LEFT
    // ========================================================

    return -1;
}


// ============================================================
// GAS SENSOR
// ============================================================

int readGasSensor()
{
    return analogRead(
        PIN_GAS_SENSOR
    );
}


// ============================================================

// ============================================================
// SEND GAS STATUS TO MASTER
// ============================================================

void sendGasStatusToMaster(
    bool detected,
    int gasValue)
{
    warden_slave_status_t status = {};

    status.magic = WARDEN_SLAVE_STATUS_MAGIC;
    status.version = WARDEN_SLAVE_STATUS_VERSION;
    status.source = WARDEN_SLAVE_ID;
    status.sequence = ++gasStatusSequence;
    status.gas_detected = detected ? 1 : 0;
    status.gas_value = (uint16_t)gasValue;

    esp_err_t result = esp_now_send(
        WARDEN_MASTER_MAC,
        (const uint8_t *)&status,
        sizeof(status)
    );

    if (result != ESP_OK)
    {
        Serial.print(
            "Failed to send gas status to Master: "
        );

        Serial.println(
            esp_err_to_name(result)
        );
    }
}


// ============================================================

void checkGasLevel()
{
    unsigned long currentTime =
        millis();


    if (
        currentTime -
        lastGasCheckTime <
        GAS_CHECK_INTERVAL
    )
    {
        return;
    }


    lastGasCheckTime =
        currentTime;


    int gasValue =
        readGasSensor();


    // ========================================================
    // GAS DETECTED
    // ========================================================

    if (
        gasValue >
        GAS_THRESHOLD
    )
    {
        if (
            !gasDetected
        )
        {
            gasDetected =
                true;


            Serial.println();

            Serial.println(
                "GAS DETECTED!"
            );

            sendGasStatusToMaster(true, gasValue);
        }
    }


    // ========================================================
    // GAS NORMAL
    // ========================================================

    else
    {
        if (
            gasDetected
        )
        {
            gasDetected =
                false;


            Serial.println(
                "Gas level normal."
            );

            sendGasStatusToMaster(false, gasValue);
        }
    }


    // ========================================================
    // PRINT GAS LEVEL
    // ========================================================

    Serial.print(
        "Gas Sensor Value: "
    );


    Serial.print(
        gasValue
    );


    Serial.print(
        " / 4095"
    );


    if (
        gasDetected
    )
    {
        Serial.print(
            " - ALERT!"
        );
    }


    Serial.println();
}