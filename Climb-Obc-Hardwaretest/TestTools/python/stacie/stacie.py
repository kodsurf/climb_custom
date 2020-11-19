import json
import os
import serial
import enum


class ComState(enum.Enum):
    IDLE = 0
    RX = 1


class StacieSim:
    def __init__(self, configFilename):
        with open(configFilename) as json_data_file:
            self.state = ComState.IDLE
            data = json.load(json_data_file)
            p = data['port']
            if os.name == 'nt':
                p = 'COM' + p
            else:
                p = '/dev/ttyS' + p
            self.ser = serial.Serial(p, 9600)
            self.commands = data['commands']

    def main_non_blocking(self):
        while self.ser.inWaiting() > 0:
            self.process_byte(ord(self.ser.read(1)))

    def main_loop(self):
        while True:
            self.main_non_blocking()

    def process_byte(self, byte):
        if self.state == ComState.IDLE:
            if byte == 0x31:
                self.state = ComState.RX
                self.received = bytearray()
        elif self.state == ComState.RX:
            self.received.append(byte)
            if byte == 0x39:
                self.state = ComState.IDLE
                print(self.ser.portstr + " received:")
                print(''.join('0x{:02x} '.format(x) for x in self.received))
                print(list(self.received))

    def has_command(self, c):
        for cmd in self.commands:
            if (cmd['short'] == c):
                return True
        return False

    def send_command(self, c):
        for cmd in self.commands:
            if (cmd['short'] == c):
                print(self.ser.portstr+' Tx: ', end='')
                for hex in cmd['hexbytes']:
                    self.ser.write(bytes.fromhex(hex))
                    print(hex+' ', end='')
                print(' ')
    