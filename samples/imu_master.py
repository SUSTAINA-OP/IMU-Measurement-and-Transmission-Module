#!/usr/bin/python3
import serial
import time
import struct

from CRC16 import CRC16 as CRC

ser = serial.Serial('/dev/ttyACM0', 115200)
ser.timeout = 1/1000

firmwareCheackCommand = 0xD0

try:
    n_transmissioned = 0
    n_received = 0
    n_corrected = 0

    while True:
        ser.reset_input_buffer()
        
        header = [0xFE, 0xFE]
        command = 0xA0

        txbuf = []
        txbuf.extend(header)
        txbuf.append(command)

        crc = CRC.getCrc16(txbuf)
        txbuf.extend(crc.to_bytes(2, byteorder="little"))

        txbuf_hex = ['{:02X}'.format(i) for i in txbuf]

        # Transmit the packet
        ser.write(bytes(txbuf))

        n_transmissioned += 1

        print("<----")
        print("command  : ", txbuf_hex[2])
        print("crc      : ", crc)
        print("txbuf    : ", txbuf_hex)
        print("<----")

        start_time = time.time()

        print("---->")

        while True:

            received_header = ser.read(2)

            if received_header == bytes(header):

                n_received += 1

                rxbuf = []
                rxbuf.extend(header)
                rxbuf.extend(list(ser.read(3)))

                received_length = int(rxbuf[3])

                rxbuf.extend(list(ser.read(received_length-5)))

                rxbuf_hex = ['{:02X}'.format(i) for i in rxbuf]

                print("rxbuf    : ", rxbuf_hex)
                print("command  : ", rxbuf_hex[2])
                print("length   : ", received_length)
                print("error    : ", rxbuf_hex[4])

                crc16 = int.from_bytes(bytes(rxbuf[-2:]), byteorder="little")
                print("crc      :", crc16)

                cheack_crc = CRC.checkCrc16(rxbuf)
                print("check crc:", cheack_crc )

                if not cheack_crc:
                    print("crc error!")
                    break

                n_corrected += 1

                if rxbuf_hex[2] == '{:02X}'.format(firmwareCheackCommand) :
                    firmwareVersion = rxbuf_hex[5]
                    print("ver. :", firmwareVersion )
                    break

                received_data = []
                for i in range(5, len(rxbuf_hex)-2, 4):
                    received_data.append(struct.unpack('f', bytes.fromhex(''.join(rxbuf_hex[i:i + 4])))[0])
                print("data     :", received_data )
                break

            if time.time() - start_time > 1/200:
                print("timeout")
                break

        print("---->")
        print("tx       : ", n_transmissioned)
        print("rx       : ", n_received)
        if n_received != 0:
            print("crc error: ", (n_received - n_corrected)/n_received*100, "%\n")

except KeyboardInterrupt:
    print("Program stopped.")
finally:
    ser.close()
