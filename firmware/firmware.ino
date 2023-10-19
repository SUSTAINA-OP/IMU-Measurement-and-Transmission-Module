#include "./src/ICM42688.h"
#include <FlashStorage.h>

// an ICM42688 object with the ICM42688 sensor on SPI bus 0 and chip select pin 3
ICM42688 IMU(SPI, 3);

byte _data[sizeof(float) * 7];

FlashStorage(gyrB_x, float);
FlashStorage(gyrB_y, float);
FlashStorage(gyrB_z, float);

void setup()
{
  // serial to display data
  Serial.begin(115200);
  while (!Serial)
  {
  }

  // start communication with IMU
  int status = IMU.begin();
  if (status < 0)
  {
    while (1)
    {
    }
  }

  float read_gyrB_x = gyrB_x.read();
  float read_gyrB_y = gyrB_y.read();
  float read_gyrB_z = gyrB_z.read();

  IMU.setGyroBiasX(read_gyrB_x);
  IMU.setGyroBiasY(read_gyrB_y);
  IMU.setGyroBiasZ(read_gyrB_z);
}

void loop()
{
  if (Serial.available())
  {
    byte received_char = Serial.read();
    
    if (received_char == 'b' || received_char == 'B') {
      // estimates the gyro biases
      IMU.calibrateGyro();

      float gyro_bias_x = IMU.getGyroBiasX();
      float gyro_bias_y = IMU.getGyroBiasY();
      float gyro_bias_z = IMU.getGyroBiasZ();

      gyrB_x.write(gyro_bias_x);
      gyrB_y.write(gyro_bias_y);
      gyrB_z.write(gyro_bias_z);
    }
    else if (received_char == 't' || received_char == 'T'){
      // estimates the gyro biases
      IMU.calibrateGyro();

      Serial.println("BiasX: " + String(IMU.getGyroBiasX()) + ", BiasY: " + String(IMU.getGyroBiasY()) + ", BiasZ: " + String(IMU.getGyroBiasZ()) + ", Temp.: " + String(IMU.temp()));
    }
    else
    {
      // read the sensor
      IMU.getAGT();

      float _accX = IMU.accX();
      float _accY = IMU.accY();
      float _accZ = IMU.accZ();
      float _gyrX = IMU.gyrX();
      float _gyrY = IMU.gyrY();
      float _gyrZ = IMU.gyrZ();
      float _temp = IMU.temp();

      // convert 6 float variables to byte array
      byte *ptr = NULL;
      byte data[sizeof(float) * 7];
      for (int data_itr = 0; data_itr < 7; data_itr++)
      {
        switch (data_itr)
        {
          case 0:
            ptr = (byte *)&_accX;
            break;
          case 1:
            ptr = (byte *)&_accY;
            break;
          case 2:
            ptr = (byte *)&_accZ;
            break;
          case 3:
            ptr = (byte *)&_gyrX;
            break;
          case 4:
            ptr = (byte *)&_gyrY;
            break;
          case 5:
            ptr = (byte *)&_gyrZ;
            break;
          case 6:
            ptr = (byte *)&_temp;
            break;
        }
        for (int ptr_itr = 0; ptr_itr < sizeof(float); ptr_itr++)
        {
          data[data_itr * sizeof(float) + ptr_itr] = *ptr;
          ptr++;
        }
      }

      // send byte array via serial communication
      Serial.write(data, sizeof(data));
    }
  }
}
