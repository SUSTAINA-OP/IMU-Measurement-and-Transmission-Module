/**
  * Arduino Firmware for IMU Measurement and Transmission Module
  *
  * This firmware is intended to be written to an Arduino board mounted on the IMU Measurement and Transmission Module.
  * The Arduino board is assumed to be Seeeduino XIAO (CPU: SAMD21G18) made by Seed Studio.
  *
  * This firmware uses the FlashStorage library for SAMD21.
  * Therefore, this firmware is not compatible with another XAIO series board.
  *
  * In this firmware, communication with the IMU must be via SPI connection.
  * For I2C connection, change the arguments in the object definition of the class.
  */

#include "./src/ICM42688/ICM42688.h"
#include "./src/CRC16/CRC16.h"

#include <FlashStorage.h>
#include <vector>

/**
 * settings by users
 */
const uint8_t headerPacket[] = { 0xFE, 0xFE };
const uint8_t cs_pin = 3;
const uint32_t UsbSerial_baudrate = 115200;
const uint8_t firmwareVersion = 0x01;

//! command
//! commands to return the values: 0xA*
const uint8_t estimatedAccelGyroTempCommand = 0xA0;
const uint8_t estimatedAccelCommand = 0xA1;
const uint8_t estimatedGyroCommand = 0xA2;
const uint8_t estimatedTempCommand = 0xA3;
const uint8_t estimatedBiasCommand = 0xA4;
const uint8_t storedBiasCommand = 0xA5;
const uint8_t adaptedBiasCommand = 0xA6;

//! commands to change/return the values: 0xB*
const uint8_t setupBiasCommand = 0xB0;
const uint8_t replaceSpecifiedBiasCommand = 0xB1;
const uint8_t adaptedSpecifiedBiasCommand = 0xB2;

const uint8_t cheackFirmwareCommand = 0xD0;

//! error status
// const uint8_t rxPacket_errorStatus = 0b00000001;
const uint8_t crc_errorStatus = 0b00000010;
const uint8_t imuConnection_errorStatus = 0b00000100;
const uint8_t imuResponse_errorStatus = 0b00001000;
const uint8_t commandUnsupport_errorStatus = 0b00010000;
const uint8_t commandProcessing_errorStatus = 0b00100000;

/**
 * settings users do not need to change
 */

const size_t headerPacket_length = sizeof(headerPacket);
const size_t crc_length = sizeof(uint16_t);

//! rx packet: headder + command + length + data * n + crc
const size_t rxPacket_forward_length = headerPacket_length + 2;
const size_t rxPacket_min_length = rxPacket_forward_length + crc_length;

//! tx packet: headder + command + length + error + data * n + crc
const size_t txPacket_min_length = headerPacket_length + 3 + crc_length;

int16_t imuBeginStatus = 0;
int16_t imuResponseStatus = 0;

std::vector<float> rxFloatData;
std::vector<float> txFloatData;
std::vector<uint8_t> txByteData;

ICM42688 IMU(SPI, cs_pin);

CRC16 CRC;

//! settings for non-volatile flash memory
FlashStorage(gyrB_x, float);
FlashStorage(gyrB_y, float);
FlashStorage(gyrB_z, float);

void setup() {
  initializeSerial(UsbSerial_baudrate);
  initializeIMU();
}

void loop() {

  if (imuBeginStatus < 0 || !IMU.verifyWhoAmI()) {
    imuBeginStatus = IMU.begin();
  }

  if (Serial.available() >= rxPacket_min_length) {

    uint8_t rxPacket_forward[rxPacket_forward_length] = {};

    if (checkHeader(headerPacket, headerPacket_length, rxPacket_forward)) {

      for (int i = headerPacket_length; i < rxPacket_forward_length; i++) {
        rxPacket_forward[i] = Serial.read();
      }

      uint8_t rxCommand = rxPacket_forward[headerPacket_length];
      size_t rxPacket_length = rxPacket_forward[headerPacket_length + 1];

      uint8_t rxPacket[rxPacket_length] = {};

      for (int i = 0; i < rxPacket_length; i++) {
        if (i < rxPacket_forward_length) {
          rxPacket[i] = rxPacket_forward[i];
        } else {
          rxPacket[i] = Serial.read();
        }
      }

      uint8_t tx_errorStatus = 0b00000000;

      rxFloatData.clear();
      txFloatData.clear();
      txByteData.clear();

      size_t txFloatData_length = 0;
      size_t txByteData_length = 0;
      size_t txPacket_length = txPacket_min_length;

      if (CRC.checkCrc16(rxPacket, rxPacket_length)) {

        if (rxPacket_length > rxPacket_min_length) {

          size_t rxFloatData_length = rxPacket_length - rxPacket_min_length;

          uint8_t rxFloatData_bytes[rxFloatData_length] = {};
          for (int i = 0; i < rxFloatData_length; i++) {
            rxFloatData_bytes[i] = rxPacket[i + rxPacket_forward_length];
          }

          size_t rxFloatData_count = rxFloatData_length / sizeof(float);

          for (int i = 0; i < rxFloatData_count; i++) {
            float rxFloatData_float = 0;
            byte* bytePtr = (byte*)&rxFloatData_float;

            for (int j = 0; j < sizeof(float); j++) {
              bytePtr[j] = rxFloatData_bytes[i * sizeof(float) + j];
            }

            rxFloatData.push_back(rxFloatData_float);
          }
        }

        processCommand(rxCommand, &tx_errorStatus);

        if (!txFloatData.empty()) {
          txFloatData_length = txFloatData.size() * sizeof(float);
          txPacket_length += txFloatData_length;
        }

        if (!txByteData.empty()) {
          txByteData_length = txByteData.size() * sizeof(uint8_t);
          txPacket_length += txByteData_length;
        }
      } else {
        tx_errorStatus |= crc_errorStatus;
      }

      uint8_t txPacket[txPacket_length] = {};
      size_t packetIndex = 0;

      memcpy(txPacket, headerPacket, headerPacket_length);
      packetIndex += headerPacket_length;

      txPacket[packetIndex++] = rxCommand;
      txPacket[packetIndex++] = (uint8_t)txPacket_length;
      txPacket[packetIndex++] = tx_errorStatus;

      if (!txFloatData.empty()) {
        memcpy(txPacket + packetIndex, txFloatData.data(), txFloatData_length);
        packetIndex += txFloatData_length;
      }

      if (!txByteData.empty()) {
        memcpy(txPacket + packetIndex, txByteData.data(), txByteData_length);
        packetIndex += txByteData_length;
      }

      uint16_t txCrc = CRC.getCrc16(txPacket, txPacket_length - crc_length);
      txPacket[packetIndex++] = lowByte(txCrc);
      txPacket[packetIndex++] = highByte(txCrc);

      Serial.write(txPacket, txPacket_length);

      if (tx_errorStatus != 0b00000000) {
        digitalWrite(13, LOW);  // light on
      } else {
        digitalWrite(13, HIGH);  //light on
      }
    }
  }
}

bool checkHeader(const uint8_t header[], const size_t length, uint8_t packet[]) {
  for (int i = 0; i < length; i++) {
    if (Serial.read() != header[i]) {
      return false;
    }
    packet[i] = header[i];
  }
  return true;
}

void processCommand(uint8_t command, uint8_t* error) {

  float _GyroBias[3] = {};

  switch (command) {
    case estimatedAccelGyroTempCommand:
      /**
            * @brief: read the latest data from ICM42688
            * @return: latest 3-axis acceleration [g's], 3-axis gyro [bps], temperature [celsius]
            */
      if (imuBeginStatus < 0) {
        *error |= imuConnection_errorStatus;
        break;
      }

      imuResponseStatus = IMU.getAGT();

      txFloatData.push_back(IMU.accX());
      txFloatData.push_back(IMU.accY());
      txFloatData.push_back(IMU.accZ());
      txFloatData.push_back(IMU.gyrX());
      txFloatData.push_back(IMU.gyrY());
      txFloatData.push_back(IMU.gyrZ());
      txFloatData.push_back(IMU.temp());
      break;

    case estimatedAccelCommand:
      /**
            * @brief: read the latest data from ICM42688
            * @return: latest 3-axis acceleration [g's]
            */
      if (imuBeginStatus < 0) {
        *error |= imuConnection_errorStatus;
        break;
      }

      IMU.getAGT();

      txFloatData.push_back(IMU.accX());
      txFloatData.push_back(IMU.accY());
      txFloatData.push_back(IMU.accZ());
      break;

    case estimatedGyroCommand:
      /**
            * @brief: read the latest data from ICM42688
            * @return: latest 3-axis gyro [bps]
            */
      if (imuBeginStatus < 0) {
        *error |= imuConnection_errorStatus;
        break;
      }

      imuResponseStatus = IMU.getAGT();

      txFloatData.push_back(IMU.gyrX());
      txFloatData.push_back(IMU.gyrY());
      txFloatData.push_back(IMU.gyrZ());
      break;

    case estimatedTempCommand:
      /**
            * @brief: read the latest data from ICM42688
            * @return: latest temperature [celsius]
            */
      if (imuBeginStatus < 0) {
        *error |= imuConnection_errorStatus;
        break;
      }

      imuResponseStatus = IMU.getAGT();

      txFloatData.push_back(IMU.temp());
      break;

    case estimatedBiasCommand:
      /**
            * @brief: estimates the gyro bias
            * @return: estimated 3-axis gyro bias [dps]
            */
      if (imuBeginStatus < 0) {
        *error |= imuConnection_errorStatus;
        break;
      }

      _GyroBias[0] = IMU.getGyroBiasX();
      _GyroBias[1] = IMU.getGyroBiasY();
      _GyroBias[2] = IMU.getGyroBiasZ();

      imuResponseStatus = IMU.calibrateGyro();

      txFloatData.push_back(IMU.getGyroBiasX());
      txFloatData.push_back(IMU.getGyroBiasY());
      txFloatData.push_back(IMU.getGyroBiasZ());

      //! restores the bias stored in non-volatile flash memory
      //! due to the estimated bias being adapted
      IMU.setGyroBiasX(_GyroBias[0]);
      IMU.setGyroBiasY(_GyroBias[1]);
      IMU.setGyroBiasZ(_GyroBias[2]);
      break;

    case storedBiasCommand:
      /**
            * @brief: reads gyro bias stored in non-volatile flash memory
            * @return: 3-axis gyro bias [dps] in non-volatile flash memory
            */

      txFloatData.push_back(gyrB_x.read());
      txFloatData.push_back(gyrB_y.read());
      txFloatData.push_back(gyrB_z.read());
      break;

    case adaptedBiasCommand:
      /**
            * @brief: reads adapted gyro bias
            * @return: adapted 3-axis gyro bias [dps]
            */

      txFloatData.push_back(IMU.getGyroBiasX());
      txFloatData.push_back(IMU.getGyroBiasY());
      txFloatData.push_back(IMU.getGyroBiasZ());
      break;

    case setupBiasCommand:
      /**
            * @brief: estimates the gyro bias, adapt the estimated gyro bias, store it in non-volatile flash memory
            * @return: estimated 3-axis gyro bias [dps]
            */
      if (imuBeginStatus < 0) {
        *error |= imuConnection_errorStatus;
        break;
      }

      imuResponseStatus = IMU.calibrateGyro();

      //! store bias in non-volatile flash memory
      gyrB_x.write(IMU.getGyroBiasX());
      gyrB_y.write(IMU.getGyroBiasY());
      gyrB_z.write(IMU.getGyroBiasZ());

      txFloatData.push_back(IMU.getGyroBiasX());
      txFloatData.push_back(IMU.getGyroBiasY());
      txFloatData.push_back(IMU.getGyroBiasZ());
      break;

    case replaceSpecifiedBiasCommand:
      /**
            * @brief: adapt the specified gyro bias and store it in non-volatile flash memory
            * @return: 3-axis gyro bias [dps] after adaptation
            */

      if (rxFloatData.empty() || rxFloatData.size() != 3) {
        *error |= commandProcessing_errorStatus;
        break;
      }

      IMU.setGyroBiasX(rxFloatData.at(0));
      IMU.setGyroBiasY(rxFloatData.at(1));
      IMU.setGyroBiasZ(rxFloatData.at(2));

      //! store bias in non-volatile flash memory
      gyrB_x.write(rxFloatData.at(0));
      gyrB_y.write(rxFloatData.at(1));
      gyrB_z.write(rxFloatData.at(2));

      txFloatData.push_back(IMU.getGyroBiasX());
      txFloatData.push_back(IMU.getGyroBiasY());
      txFloatData.push_back(IMU.getGyroBiasZ());
      break;

    case adaptedSpecifiedBiasCommand:
      /**
            * @brief: adapt the specified gyro bias
            * @return: 3-axis gyro bias [dps] after adaptation
            */

      if (rxFloatData.empty() || rxFloatData.size() != 3) {
        *error |= commandProcessing_errorStatus;
        break;
      }

      IMU.setGyroBiasX(rxFloatData.at(0));
      IMU.setGyroBiasY(rxFloatData.at(1));
      IMU.setGyroBiasZ(rxFloatData.at(2));

      txFloatData.push_back(IMU.getGyroBiasX());
      txFloatData.push_back(IMU.getGyroBiasY());
      txFloatData.push_back(IMU.getGyroBiasZ());
      break;

    case cheackFirmwareCommand:
      /**
            * @brief:
            * @return: firmware virsion
            */

      txByteData.push_back(firmwareVersion);
      break;

    default:
      *error |= commandUnsupport_errorStatus;
  }

  if (imuResponseStatus < 0) {
    *error |= imuResponse_errorStatus;
  }
}

void initializeSerial(uint32_t baudrate) {
  Serial.begin(baudrate);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB
  }
}

void initializeIMU() {
  imuBeginStatus = IMU.begin();

  IMU.setGyroBiasX(gyrB_x.read());
  IMU.setGyroBiasY(gyrB_y.read());
  IMU.setGyroBiasZ(gyrB_z.read());
}
