#!/usr/bin/python3
import serial
import time
import struct
import argparse

from CRC16 import CRC16 as CRC

ser = serial.Serial('/dev/ttyACM0', 115200)
ser.timeout = 1/1000

# command
# commands to return the values: 0xA*
estimatedAccelGyroTempCommand = 0xA0
estimatedAccelCommand = 0xA1
estimatedGyroCommand = 0xA2
estimatedTempCommand = 0xA3
estimatedBiasCommand = 0xA4
storedBiasCommand = 0xA5
adaptedBiasCommand = 0xA6

# commands to change/return the values: 0xB*
setupBiasCommand = 0xB0
replaceSpecifiedBiasCommand = 0xB1
adaptedSpecifiedBiasCommand = 0xB2

restartIMUCommand = 0xC0
cheackFirmwareCommand = 0xD0

try:
    parser = argparse.ArgumentParser(description='Command and Loop Example')
    parser.add_argument('--loop', type=int, default=None, help='Number of times to loop')
    parser.add_argument('--cmd', type=str, default='0xA0', help='Command to execute')
    parser.add_argument('--txdata', type=float, nargs='+', default=None, help='Float values to send')
    args = parser.parse_args()

    loop_count = args.loop
    command = int(args.cmd,16)
    txData = args.txdata

    print(txData)
    
    n_transmissioned = 0
    n_received = 0
    n_corrected = 0

    cheack_crc = 0

    while True:
        ser.reset_input_buffer()
        
        header = [0xFE, 0xFE]

        txBuf = []
        txBuf.extend(header)
        txBuf.append(command)

        # packet length: header[] + command + length + crc(2)
        tx_length = len(header) + 4

        if txData is not None :
            tx_length += len(txData) * 4

        txBuf.append(tx_length)

        if txData is not None :
            txData_byte = b''.join([struct.pack('f', data) for data in txData])
            txBuf.extend(txData_byte)

        crc = CRC.getCrc16(txBuf)
        txBuf.extend(crc.to_bytes(2, byteorder="little"))

        txBuf_hex = ['{:02X}'.format(i) for i in txBuf]

        # Transmit the packet
        ser.write(bytes(txBuf))

        n_transmissioned += 1

        print("<----")
        print("command  : ", txBuf_hex[2])
        print("length   : ", int(txBuf_hex[3],16))

        if txData is not None :
            print("txdata   : ", txData)

        print("crc      : ", crc)
        print("txBuf    : ", txBuf_hex)
        print("<----")

        start_time = time.time()

        print("---->")

        while True:

            received_header = ser.read(2)

            if received_header == bytes(header):

                n_received += 1

                rxBuf = []
                rxBuf.extend(header)
                rxBuf.extend(list(ser.read(3)))

                received_length = int(rxBuf[3])

                rxBuf.extend(list(ser.read(received_length-5)))

                rxBuf_hex = ['{:02X}'.format(i) for i in rxBuf]

                print("rxBuf    : ", rxBuf_hex)
                print("command  : ", rxBuf_hex[2])
                print("length   : ", received_length)
                print("error    : ", format(int(rxBuf[4]),"#010b"))

                crc16 = int.from_bytes(bytes(rxBuf[-2:]), byteorder="little")
                print("crc      :", crc16)

                cheack_crc = CRC.checkCrc16(rxBuf)
                print("check crc:", cheack_crc )

                if not cheack_crc:
                    print("crc error!")
                    break

                n_corrected += 1

                if rxBuf_hex[2] == '{:02X}'.format(cheackFirmwareCommand) :
                    firmwareVersion = rxBuf_hex[5]
                    print("ver. :", firmwareVersion )
                    break

                received_data = []
                for i in range(5, len(rxBuf_hex)-2, 4):
                    received_data.append(struct.unpack('f', bytes.fromhex(''.join(rxBuf_hex[i:i + 4])))[0])
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
        else:
            print("\n")

        if cheack_crc and loop_count is not None:
            exit()

except KeyboardInterrupt:
    print("Program stopped.")
finally:
    ser.close()
