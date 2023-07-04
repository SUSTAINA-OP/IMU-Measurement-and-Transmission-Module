#include "./src/ICM42688.h"

// an ICM42688 object with the ICM42688 sensor on SPI bus 0 and chip select pin 7
ICM42688 IMU(SPI, 3);

unsigned long ts, te;

byte _data[sizeof(float) * 6];

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
}

void loop()
{
  if (Serial.available())
  {
    byte received_char = Serial.read();
    if (received_char == 'r' || received_char == 'R')
    {
      // start communication with IMU
      int status = IMU.begin();
      if (status < 0)
      {
        while (1)
        {
        }
      }
      
      Serial.write("c", sizeof(byte));
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

      // convert 6 float variables to byte array
      byte *ptr = NULL;
      byte data[sizeof(float) * 6];
      for (int data_itr = 0; data_itr < 6; data_itr++)
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
