""" 
All Python code for interacting with the Antenna Analyzer and analyzing readings goes in this file.
 
Author: Angad Gill
"""
import serial
import math, cmath

ADC_SCALE = 1.8*2/(2**12)  # 1.8 (reference voltage) * 2 (single-ended) / 2**12 (resolution)


class AntennaAnalyzer(object):
    def __init__(self, port, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self._ser = serial.Serial(port=port, baudrate=baudrate, timeout=0.1)

    def __del__(self):
        self._ser.close()

    def write(self, text):
        """ 
        Send text on serial port and return the response. 
        Trailing \r\n is added to the text and converted to bytes.
        """
        text += '\r\n'
        text = str.encode(text)
        self._ser.write(text)
        return self.read()

    def read(self, n=1000):
        """ Read n characters from the serial port and return as list of bytestring separated by \r\n """
        text = self._ser.read(n)
        if len(text) > 0:
            if text[-1] == ord('>'):
                text = text[:-1]
        text = text.split(b'\r\n')[:-1]
        return text

    @property
    def freq(self, refresh=True):
        """ Get DDS frequency """
        cmd = "ant get f"
        return int(self.write(cmd)[0])

    @freq.setter
    def freq(self, f):
        """ Set DDS frequency """
        cmd = "ant set f {}".format(f)
        self.write(cmd)

    @property
    def switch(self, refresh=True):
        """ Get AMux switch position """
        cmd = "ant get s"
        return int(self.write(cmd)[0])

    @switch.setter
    def switch(self, s):
        """ Set AMux switch position """
        cmd = "ant set s {}".format(s)
        self.write(cmd)

    def vmag(self):
        """ Get raw ADC reading for VMAG """
        cmd = "ant get m"
        return int(self.write(cmd)[0])

    def vphs(self):
        """ Get raw ADC reading for VPHS """
        cmd = "ant get p"
        return int(self.write(cmd)[0])

    @staticmethod
    def vmag_to_ratio(vmag):
        """ Converts raw ADC reading for VMAG to a voltage magnitude as a ratio """
        vmag_volts = vmag * ADC_SCALE
        # Formula from AD8302 datasheet page 2
        vmag_db = ((vmag_volts - 0.03)/1.8)*60 - 30
        vmag_ratio = 10**(vmag_db/20)
        return vmag_ratio

    @staticmethod
    def vphs_to_degree(vphs):
        """ Converts raw ADC reading for VPHS to a phase difference in degree """
        vphs_volts = vphs * ADC_SCALE
        # Formula from AD8302 datasheet page 2
        vphs_degree = 180 - ((vphs_volts - 0.03)/1.8) * 180
        return vphs_degree

    def impedance(self):
        """ 
        Computer the impedance of the antenna and return as a complex number.
        """
        self.switch = 1  # reference point on the resistor bridge
        m_ref, p_ref = self.vmag_to_ratio(self.vmag()), self.vphs_to_degree(self.vphs())
        self.switch = 2  # antenna connection on the resistor bridge
        m_ant, p_ant = self.vmag_to_ratio(self.vmag()), self.vphs_to_degree(self.vphs())

        v_ref = cmath.rect(m_ref, 0)
        v_ant = cmath.rect(m_ant, math.radians(p_ant-p_ref))
        impedance = 50 * (v_ant/((2 * v_ref) - v_ant))
        return impedance


if __name__ == '__main__':
    ant = AntennaAnalyzer('COM4')
    ant.freq = 1e5
    print(ant.freq)
    # print(ant.impedance())
    # print(ant.impedance())
    # print(ant.impedance())
    # print(ant.impedance())
