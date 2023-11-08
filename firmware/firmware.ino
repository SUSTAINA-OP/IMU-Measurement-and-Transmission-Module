#include "./src/ICM42688/ICM42688.h"
#include "./src/CRC16/CRC16.h"

#include <FlashStorage.h>
#include <vector>

/**
 * settings by users
 */
const uint8_t header[] = { 0xFE, 0xFE };
const uint8_t cs_pin = 3;
long usb_serial_baudrate = 115200;
const uint8_t firmware_version = 0x00;

//! command
const uint8_t estimatedAccelGyroTempCommand = 0xA0;
const uint8_t estimatedAccelCommand = 0xA1;
const uint8_t estimatedGyroCommand = 0xA2;
const uint8_t estimatedTempCommand = 0xA3;
const uint8_t estimatedBiasCommand = 0xA4;
const uint8_t storedBiasCommand = 0xA5;
const uint8_t adaptedBiasCommand = 0xA6;
const uint8_t resetupBiasCommand = 0xB0;
const uint8_t restartIMUCommand = 0xC0;
const uint8_t cheackFirmwareCommand = 0xD0;

//! error status
const uint8_t imuConnectionErrorStatus = 0x10;
const uint8_t crcErrorStatus = 0xA0;
const uint8_t commandErrorStatus = 0xB0;

/**
 * settings users do not need to change
 */

const size_t header_length = sizeof(header);
const size_t crc_length = sizeof(uint16_t);
size_t transmitted_data_length = 0;
size_t transmitted_packet_length = 0;

//! received packet length: headder + command + crc
const size_t received_packet_length = header_length + 1 + crc_length;

int16_t imu_begin_status = 0;
int16_t imu_status = 0;
bool imu_WhoAmI = false;

uint8_t transmitted_error = 0x00;

std::vector<float> transmitted_data;

ICM42688 IMU(SPI, cs_pin);

CRC16 CRC;

//! Settings for non-volatile flash memory
FlashStorage(gyrB_x, float);
FlashStorage(gyrB_y, float);
FlashStorage(gyrB_z, float);

void setup() {
  initializeSerial();
  initializeIMU();
}

void loop() {

  imu_WhoAmI = IMU.getWhoAmI();

  if (imu_begin_status < 0 || !imu_WhoAmI) {
    imu_begin_status = IMU.begin();
  }

  if (Serial.available() >= received_packet_length) {

    uint8_t received_packet[received_packet_length] = {};

    if (checkHeader(received_packet)) {

      //! save to received packet
      for (int i = header_length; i < received_packet_length; i++) {
        received_packet[i] = Serial.read();
      }

      uint8_t command = received_packet[header_length];
      transmitted_error = 0x00;

      //! default transmitted paclet length: headder + command + length + error + crc
      transmitted_packet_length = header_length + 3 + crc_length;

      //! clear variable-length array of transmittedted data
      transmitted_data.clear();

      //! crc adapted
      if (CRC.checkCrc16(received_packet, received_packet_length)) {

        processCommand(command);

        transmitted_data_length = transmitted_data.size() * sizeof(float);
        transmitted_packet_length += transmitted_data_length;

        if (command == cheackFirmwareCommand) {
          transmitted_packet_length += 1;
        }

      } else {
        transmitted_error = crcErrorStatus;
      }

      uint8_t transmitted_packet[transmitted_packet_length] = {};
      uint8_t packetIndex = 0;

      memcpy(transmitted_packet, header, header_length);
      packetIndex += header_length;

      transmitted_packet[packetIndex++] = command;
      transmitted_packet[packetIndex++] = (uint8_t)transmitted_packet_length;
      transmitted_packet[packetIndex++] = transmitted_error;

      if (!transmitted_data.empty()) {
        memcpy(transmitted_packet + packetIndex, transmitted_data.data(), transmitted_data_length);
        packetIndex += transmitted_data_length;
      }

      if (command == cheackFirmwareCommand) {
        transmitted_packet[packetIndex++] = firmware_version;
      }

      uint16_t transmitted_crc = CRC.getCrc16(transmitted_packet, transmitted_packet_length - crc_length);
      transmitted_packet[packetIndex++] = lowByte(transmitted_crc);
      transmitted_packet[packetIndex++] = highByte(transmitted_crc);

      Serial.write(transmitted_packet, transmitted_packet_length);
    }
  }
}

bool checkHeader(uint8_t received_packet[]) {
  for (int i = 0; i < header_length; i++) {
    if (Serial.read() != header[i]) {
      return false;
    }
    received_packet[i] = header[i];
  }
  return true;
}

void processCommand(uint8_t command) {
  switch (command) {
    case estimatedAccelGyroTempCommand:
      /**
            * @brief: read the latest data from ICM42688
            * @return: latest 3-axis acceleration [g's], 3-axis gyro [bps], temperature [celsius]
            */

      imu_status = IMU.getAGT();

      transmitted_data.push_back(IMU.accX());
      transmitted_data.push_back(IMU.accY());
      transmitted_data.push_back(IMU.accZ());
      transmitted_data.push_back(IMU.gyrX());
      transmitted_data.push_back(IMU.gyrY());
      transmitted_data.push_back(IMU.gyrZ());
      transmitted_data.push_back(IMU.temp());
      break;

    case estimatedAccelCommand:
      /**
            * @brief: read the latest data from ICM42688
            * @return: latest 3-axis acceleration [g's]
            */

      IMU.getAGT();

      transmitted_data.push_back(IMU.accX());
      transmitted_data.push_back(IMU.accY());
      transmitted_data.push_back(IMU.accZ());
      break;

    case estimatedGyroCommand:
      /**
            * @brief: read the latest data from ICM42688
            * @return: latest 3-axis gyro [bps]
            */

      imu_status = IMU.getAGT();

      transmitted_data.push_back(IMU.gyrX());
      transmitted_data.push_back(IMU.gyrY());
      transmitted_data.push_back(IMU.gyrZ());
      break;

    case estimatedTempCommand:
      /**
            * @brief: read the latest data from ICM42688
            * @return: latest temperature [celsius]
            */

      imu_status = IMU.getAGT();

      transmitted_data.push_back(IMU.temp());
      break;

    case estimatedBiasCommand:
      /**
            * @brief: estimates the gyro bias
            * @return: estimated 3-axis gyro bias [dps]
            */

      imu_status = IMU.calibrateGyro();

      transmitted_data.push_back(IMU.getGyroBiasX());
      transmitted_data.push_back(IMU.getGyroBiasY());
      transmitted_data.push_back(IMU.getGyroBiasZ());

      //! restores the bias stored in non-volatile flash memory
      //! due to the estimated bias being adapted
      IMU.setGyroBiasX(gyrB_x.read());
      IMU.setGyroBiasY(gyrB_y.read());
      IMU.setGyroBiasZ(gyrB_z.read());
      break;

    case storedBiasCommand:
      /**
            * @brief: reads gyro bias stored in non-volatile flash memory
            * @return: 3-axis gyro bias [dps] in non-volatile flash memory
            */

      transmitted_data.push_back(gyrB_x.read());
      transmitted_data.push_back(gyrB_y.read());
      transmitted_data.push_back(gyrB_z.read());
      break;

    case adaptedBiasCommand:
      /**
            * @brief: reads adapted gyro bias
            * @return: adapted 3-axis gyro bias [dps]
            */

      transmitted_data.push_back(IMU.getGyroBiasX());
      transmitted_data.push_back(IMU.getGyroBiasY());
      transmitted_data.push_back(IMU.getGyroBiasZ());
      break;

    case resetupBiasCommand:
      /**
            * @brief: estimates the gyro bias, adapt the estimated gyro bias, store it in non-volatile flash memory
            * @return: estimated 3-axis gyro bias [dps]
            */

      imu_status = IMU.calibrateGyro();

      //! store bias in non-volatile flash memory
      gyrB_x.write(IMU.getGyroBiasX());
      gyrB_y.write(IMU.getGyroBiasY());
      gyrB_z.write(IMU.getGyroBiasZ());

      transmitted_data.push_back(IMU.getGyroBiasX());
      transmitted_data.push_back(IMU.getGyroBiasY());
      transmitted_data.push_back(IMU.getGyroBiasZ());
      break;

    case restartIMUCommand:
      /**
            * @brief: restarts communication with the ICM42688 
            * @return: 
            */

      imu_begin_status = IMU.begin();
      break;

    case cheackFirmwareCommand:
      /**
            * @brief:
            * @return: firmware virsion
            */

      break;

    default:
      transmitted_error = commandErrorStatus;
  }
}

void initializeSerial() {
  Serial.begin(usb_serial_baudrate);
  while (!Serial) {
    // Wait for the serial port to be ready
  }
}

void initializeIMU() {
  imu_begin_status = IMU.begin();

  IMU.setGyroBiasX(gyrB_x.read());
  IMU.setGyroBiasY(gyrB_y.read());
  IMU.setGyroBiasZ(gyrB_z.read());
}
