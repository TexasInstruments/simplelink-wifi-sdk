import os
import logging
import time
import serial


SEC_TYPE_VAL = {
    "OPEN": "OPEN",
    "WPA-PSK": "WPA2_PLUS", #"WPA",
    "WPA2-PSK": "WPA2_PLUS", #"WPA2",
    "WPA2_PLUS": "WPA2_PLUS",
    "WPA3": "WPA3",
    "WPA2-WPA3": "WPA2/WPA3",  
    "WPS-PBC": "WPS", #3,  # ask Noam: WPS is PIN? With pass, PBC? Witout pass
    "WPS-PIN": "WPS", #4,  # ask Noam: WPS is PIN? With pass, PBC? Witout pass
    "P2P-PBC": 6,
    "P2P-PIN": 7,
    "P2P-PIN-DISPLAY": 8,
}

class DeviceBaseClass:
    def __init__(self, logger=None):
        self._is_uart_connect = False
        self._is_started = False
        self._is_wlan_sta_role_up = False
        self._is_wlan_ap_role_up = False
        # self._ip_v4 = "0.0.0.0"
        # self._ip_v4_ap = "0.0.0.0"
        self.p2p_mac = None
        self.serial_port = None
        self.logger = None

        if logger is None:
            self.log_me()
        else:
            self.logger = logger

    def log_me(self):
        try:
            filename = self.log_path if self.log_path is not None else "log.log"
        except AttributeError:
            filename = 'log.log'
        filename = os.path.abspath(filename)
        logging.basicConfig(
            filename=filename,
            format='%(levelname)s :  %(message)s',
            filemode='w')
        # Creating an object
        self.logger = logging.getLogger()

        # Setting the threshold of logger to INFO
        self.logger.setLevel(logging.INFO)
        ch = ColorHandler()
        self.logger.addHandler(ch)
        self.logger.info(f"Logging to - {filename}")

    def connect_serial(self, com, rts_cts=False):
        # connect to COM Port for UART
        self.logger.info(f"{' CONNECT TO SERIAL PORT ':-^40}")
        rs232_port_num = str(com)
        self.logger.info(f" {rs232_port_num :-^40} ")
        try:
            self.serial_port = serial.Serial(
                port=rs232_port_num,
                baudrate=115200,
                bytesize=8,
                timeout=1,
                stopbits=serial.STOPBITS_ONE,
                parity=serial.PARITY_NONE,
                xonxoff=False,
                rtscts=rts_cts,
                dsrdtr=False
            )
            self.logger.info("Connected to serial port")
            # clean buffer
            while self.serial_port.in_waiting != 0:
                self.serial_port.reset_input_buffer()
                self.serial_port.reset_output_buffer()
                self.serial_port.flush()
                time.sleep(1)
            self.logger.info("Cleared serial port")
            self._is_uart_connect = True
        except Exception as err:
            self.logger.error("Serial port connection error " + str(err))
            self._is_uart_connect = False
            exit(1)

    def serial_close(self):
        self.serial_port.close()
        self._is_uart_connect = False
        self.logger.info("Serial port closed")

    def serial_clear(self):
        # clean buffer
        while self.serial_port.in_waiting != 0:
            self.serial_port.reset_input_buffer()
            self.serial_port.reset_output_buffer()
            self.serial_port.flush()
            time.sleep(1)

    def serial_write(self, data, info='', print_flag=True):
        if print_flag:
            self.logger.info(f"{' SERIAL PORT WRITE ':-^40}")
            if info != '':
                self.logger.info(f"{info}")
            self.logger.info(f"Writing data - {data}")
        if isinstance(data, str):
            self.serial_port.write(data.encode())
        else:
            self.serial_port.write(data)

    def serial_read_block(self, decoding_format='utf-8', timeout=1000, user=True, print_flag=True):
        if print_flag:
            self.logger.info(f"{' SERIAL PORT READ BLOCK ':-^40}")
            self.logger.info("Waiting for serial port to send data")
        serial_byte = b''
        serial_string = ''
        timeout = time.time() + timeout  # timeout_sec
        while serial_string == '':
            if time.time() > timeout:
                self.logger.error('Timeout occurred on reading from serial UART')
                exit(1)
            if not user:
                while time.time() <= timeout:
                    if self.serial_port.in_waiting > 0:
                        serial_byte += self.serial_port.read()
                        serial_string = serial_byte.decode(decoding_format)
                    if serial_string != '':
                        self.logger.debug(serial_string)
            else:
                try:
                    while "user:" not in serial_string and time.time() <= timeout:
                        if self.serial_port.in_waiting > 0:
                            serial_byte += self.serial_port.read()
                            serial_string = serial_byte.decode(decoding_format)
                        if serial_string != '':
                            self.logger.debug(serial_string)
                    if "user:" not in serial_string:
                        self.serial_port.write("\n".encode())  
                        timeout = time.time() + 10
                        while "user:" not in serial_string and time.time() <= timeout:
                            if self.serial_port.in_waiting > 0:
                                serial_byte += self.serial_port.read()
                                serial_string = serial_byte.decode(decoding_format)
                            if serial_string != '':
                                self.logger.debug(serial_string)

                except Exception as err:
                    self.logger.error(err)
                    self.logger.error("serial read block error")
                    exit(1)
        self.logger.info(f"Data received - {serial_string}")
        return serial_string

    def serial_read_check(self, data, expected):
        return str(data).strip().casefold() == str(expected).strip().casefold()

    # def serial_comm(self, write_val, write_info="", ack_val=""):
    #     self.serial_write(write_val, write_info)
    #     ack_bool = True
    #     serial_rd = ""
    #     if ack_val != "":
    #         serial_rd = self.serial_read_block()
    #         ack_bool = self.serial_read_check(serial_rd, ack_val)
    #     if ack_bool:
    #         if ack_val != "":
    #             # self.logger.info(f"Serial communication succeed - sent {write_val} received {ack_val}")
    #             pass
    #     else:
    #         self.logger.error(f"Serial communication succeed - sent {write_val} received {serial_rd} expected {ack_val}")
    #         exit(1)


    '''
        -------------------- STA commands --------------------
    '''

    def print_fw_version(self):
        self.serial_write(data='wlan_get_fw_ver \n', info=f'cmd: wlan_get_fw_ver')
        self.serial_read_block()
    
    def set_date_time(self, year, month, day, hour, min, sec):
        date_time_str = year + "-" + month + "-" + day + 'T' + hour + ":" + min + ":" + sec
        self.serial_write(data=f'set_date_time -t {date_time_str}\n', info=f'cmd: set_date_time')
        self.serial_read_block(timeout=2)
    
    def wlan_start(self): 
        if self._is_started:
            return 
        self.serial_write(data='wlan_start \n', info=f'cmd: wlan_start')
        self._is_started = True
        self.serial_read_block()

    def wlan_sta_role_up(self,timeout = 1000):
        self.serial_write(data='wlan_sta_role_up \n', info=f'cmd: wlan_sta_role_up')
        self._is_wlan_sta_role_up = True
        self.serial_read_block(timeout=timeout)

    def wlan_sta_disconnect(self,timeout=5):
        self.serial_write(data='wlan_disconnect \n', info=f'cmd: wlan_sta_disconnect')
        self.serial_read_block(timeout=timeout)

    def wlan_sta_connect(self, ssid, sec_type, key=None):
        try:
            sec_type_val = SEC_TYPE_VAL[sec_type]
        except KeyError:
            raise KeyError("Unsupported Security Type")
        
        if key is None:
            key = ""
            if "PIN" in sec_type:
                raise KeyError("Should provide password")
                
        self.wlan_sta_disconnect()
        if sec_type_val == "WPS":
            if "PBC" in sec_type:
                self.serial_write(data=f'wlan_connect -t {sec_type_val}\n', info=f'cmd: wlan_connect')
            else:
                self.serial_write(data=f'wlan_connect -t {sec_type_val} -p "{key}"\n', info=f'cmd: wlan_connect')
            self.serial_read_block(timeout=15)
        else:
            self.serial_write(data=f'wlan_connect -s "{ssid}" -t {sec_type_val} -p "{key}"\n', info=f'cmd: wlan_connect')
            self.serial_read_block(timeout=10)

    def wlan_sta_get_mac(self, role_type='0'):
        self.serial_write(data=f'wlan_get_mac -i {role_type}\n', info=f'cmd: wlan_sta_get_mac')
        mac_str = self.serial_read_block()
        mac = mac_str.split(" : ")[1].replace("user:", "").rstrip('\n\r')
        print(f"MAC: {mac}")
        return mac

    def wlan_sta_set_static_ip(self, ip, netmask, gateway, role_type='0', ip_mode='1'): 
        '''
        role_type: 0 - STA , 2 - AP 
        ip_mode: 0 - DHCP. 1 - STATIC
        '''
        self.serial_read_block(timeout=5)
        self.serial_write(data=f'wlan_set_if_ip -i {role_type} -ip {ip_mode} -c {ip} -v {netmask} -gw {gateway}\n', info=f'cmd: wlan_set_static_ip')
        self.serial_read_block(timeout=5)
        self._is_wlan_sta_dynamic_ip = False

    def wlan_sta_set_dynamic_ip(self, role_type='0', ip_mode='0'):
        '''
        role_type: 0 - STA , 2 - AP 
        ip_mode: 0 - DHCP. 1 - STATIC
        '''
        self.serial_write(data=f'wlan_set_if_ip -i {role_type} -ip {ip_mode}\n', info=f'cmd: wlan_sta_set_dynamic_ip')
        self.serial_read_block(timeout=3)
        self._is_wlan_sta_dynamic_ip = True

    def wlan_sta_set_connection_policy(self, fast: bool, auto: bool, fast_persistent: bool):
        self.serial_write(data=f'wlan_set_con_policy -a {str(int(auto))} -f {str(int(fast))} -p {str(int(fast_persistent))}\n', info=f'cmd: wlan_set_con_policy')
        self.serial_read_block(timeout=5)
        
    def wlan_sta_del_profile(self, profile_index):
        self.serial_write(data=f'wlan_del_profile -i {profile_index}\n', info=f'cmd: wlan_del_profile')
        self.serial_read_block(timeout=5)
        
    def wlan_sta_add_profile(self, ssid, sec_type, key, priority='0', hiddenAP=False):
        try:
            sec_type_val = SEC_TYPE_VAL[sec_type]
        except KeyError:
            raise KeyError("Unsupported Security Type")
        
        if key is None:
            key = ""
            if "PIN" in sec_type:
                raise KeyError("Should provide password")
            
        if priority == None:
            priority = '0'
        
        self.serial_write(data=f'wlan_add_profile -s "{ssid}" -t {sec_type_val} -p "{key}" -pr {priority} -h {str(int(hiddenAP))}\n', info=f'cmd: wlan_add_profile')
        self.serial_read_block(timeout=10)

    def wlan_set_power_management_mode(self, mode='0'):
        '''
        mode: 0 - Always active mode
              1 - Power down mode (light / fast sleep)
              2 - ELP mode (Deep / Max sleep)
        '''
        self.serial_write(data=f'wlan_set_pm -m {mode}\n', info=f'cmd: wlan_set_pm')
        self.serial_read_block()
        
    def write_cert(self, type, filename: str):
        with open(filename, "rb") as source:
            buf = source.read() + b"\0"
            size = len(buf)
            self.serial_write(data=f'load_cert -t {type} -s {size}\n', info=f'cmd: load_cert')
            serial_rd = self.serial_read_block(timeout=2, user=False, print_flag=False)
            if 'ACK' in serial_rd:
                self.logger.info(f"CCS Serial sent 'ACK' successfully")
                try:
                    self.serial_write(data=buf.decode('utf-8'), print_flag=False)
                    # for byte in buf:
                    #     self.serial_write(data=chr(byte), print_flag=False)
                except Exception as ex:
                    self.logger.error(ex)
                    raise
            else:
                self.logger.error(f"Serial read error - expected 'ACK', exist '{serial_rd}'")
            
    def wlan_load_certi_ent(self, client_cert_filename: str, ca_cert_filename: str, private_key_filename: str):        
        if client_cert_filename is not None:
            self.write_cert(type='0', filename=client_cert_filename)
            serial_rd = self.serial_read_block(print_flag=False)
            if 'client certificate Load successfully' in serial_rd:
                self.logger.info(f"CCS Serial sent 'client certificate Load successfully' successfully")
            else:
                self.logger.error(f"Serial read error - expected 'client certificate Load successfully', exist '{serial_rd}'")                

        if ca_cert_filename is not None:
            self.write_cert(type='1', filename=ca_cert_filename)
            serial_rd = self.serial_read_block(print_flag=False)
            if 'ca certificate Load successfully' in serial_rd:
                self.logger.info(f"CCS Serial sent 'ca certificate Load successfully' successfully")
            else:
                self.logger.error(f"Serial read error - expected 'ca certificate Load successfully', exist '{serial_rd}'")
                
        if private_key_filename is not None:
            self.write_cert(type='2', filename=private_key_filename)
            serial_rd = self.serial_read_block(print_flag=False)
            if 'private key certificate Load successfully' in serial_rd:
                self.logger.info(f"CCS Serial sent 'private key certificate Load successfully' successfully")
            else:
                self.logger.error(f"Serial read error - expected 'private key certificate Load successfully', exist '{serial_rd}'")                     

    def wlan_sta_connect_ent(self, ssid, sec_type, key, bssid, eap_phase1, eap_phase2, identity, anonymous_user, client_cert_filename, ca_cert_filename, private_key_filename, timeout=10, flags=0):
        self.wlan_load_certi_ent(client_cert_filename, ca_cert_filename, private_key_filename)
        try:
            sec_type_val = SEC_TYPE_VAL[sec_type]
        except KeyError:
            raise KeyError("Unsupported Security Type")
        
        if key is None:
            key = ""
            if "PIN" in sec_type:
                raise KeyError("Should provide password")
        
        if eap_phase1 == "TTLS" or eap_phase1 == "PEAP0":
            enterprise = eap_phase1 + "_" + eap_phase2
        else:
            enterprise = eap_phase1
               
        self.wlan_sta_disconnect(timeout=2)
        if sec_type_val == "WPS":
            if "PBC" in sec_type:
                self.serial_write(data=f'wlan_connect -t {sec_type_val} -e {enterprise} -i "{identity}"\n', info=f'cmd: wlan_connect')
            else:
                self.serial_write(data=f'wlan_connect -t {sec_type_val} -p "{key}" -e {enterprise} -i "{identity}"\n', info=f'cmd: wlan_connect')
        else:
            if key:
                self.serial_write(data=f'wlan_connect -s "{ssid}" -t {sec_type_val} -p "{key}" -e {enterprise} -i "{identity}"\n', info=f'cmd: wlan_connect')
            else:
                self.serial_write(data=f'wlan_connect -s "{ssid}" -t {sec_type_val} -e {enterprise} -i "{identity}"\n', info=f'cmd: wlan_connect')
        self.serial_read_block(timeout=5)
        

    '''
        -------------------- STA commands --------------------
    '''


    '''
        -------------------- AP commands --------------------
    '''

    def wlan_ap_role_up(self, ssid, sec_type, key, channel, tx_power, sta_limit, regulatory_domain, hiddenAP="NO", sae_pwe='2', transition_disable='0'):
        try:
            sec_type_val = SEC_TYPE_VAL[sec_type]
        except KeyError:
            raise KeyError("Unsupported Security Type")
        
        if hiddenAP:
            hiddenAP = "YES"
        
        self.serial_read_block(timeout=5)
        #self.serial_write(data=f'wlan_ap_role_up -s "{ssid}" -t {sec_type_val} -p "{key}" -h {hiddenAP} -txp {tx_power} -c {channel} -l {sta_limit} -r {regulatory_domain} -w {sae_pwe} \n\n', info=f'cmd: wlan_ap_role_up')

        self.serial_write(data=f'wlan_ap_role_up -s "{ssid}"', info=f'cmd: wlan_ap_role_up ssid')
        self.serial_write(data=f' -t {sec_type_val}', info=f'cmd: wlan_ap_role_up sec type')
        self.serial_write(data=f' -p "{key}"', info=f'cmd: wlan_ap_role_up key')
        time.sleep(1)
        self.serial_write(data=f' -txp "{tx_power}"', info=f'cmd: wlan_ap_role_up txp')
        self.serial_write(data=f' -c {channel}', info=f'cmd: wlan_ap_role_up channel')
        self.serial_write(data=f' -l {sta_limit}', info=f'cmd: wlan_ap_role_up sta limit')
        self.serial_write(data=f' -w {sae_pwe}', info=f'cmd: wlan_ap_role_up pwe')
        if transition_disable == '1':
            self.serial_write(data=f' -b {transition_disable}', info=f'cmd: wlan_ap_role_up transition_disable')
        if (hiddenAP):
            self.serial_write(data=f' -h {hiddenAP}', info=f'cmd: wlan_ap_role_up hidden')
        self.serial_write(data=f' -r {regulatory_domain}\n\n', info=f'cmd: wlan_ap_role_up reg domain')

        time.sleep(0.1)
        self._is_wlan_ap_role_up = True
        self.serial_read_block(timeout=5)
        
    def wlan_ap_role_down(self):
        # self.serial_read_block(timeout=10)
        self.serial_write(data='wlan_ap_role_down \n', info=f'cmd: wlan_ap_role_down')
        self._is_wlan_ap_role_up = False
        self.serial_read_block(timeout=20)
    
    def wlan_ap_wps(self, method, key):
        if method == "PIN":
            wps_method = '1'
        else:
            wps_method = '0' 

        self.serial_read_block(timeout=1)
        if not key:
            self.serial_write(data=f'start_ap_wps -w {wps_method}\n', info=f'cmd: start_ap_wps') 
        else:   
            self.serial_write(data=f'start_ap_wps -w {wps_method} -p \"{key}\"\n', info=f'cmd: start_ap_wps')
        self.serial_read_block(timeout=5)
    
    def wlan_ap_get_mac(self, role_type='2'):
        self.serial_read_block(timeout=10)
        self.serial_write(data=f'wlan_get_mac -i {role_type}\n', info=f'cmd: wlan_ap_get_mac')
        mac_str = self.serial_read_block()
        if "No role up" in mac_str:
            return "00:00:00:00:00:00"
        if "No such command":
            self.serial_write(data=f'wlan_get_mac -i {role_type}\n', info=f'cmd: wlan_ap_get_mac')
            mac_str = self.serial_read_block()
            
        mac = mac_str.split(" : ")[1].replace("user:", "").rstrip('\n\r')
        print(f"MAC: {mac}")
        return mac
    
    def wlan_get_ip(self, role_type):
        import re
        ip_address = "0.0.0.0"
        self.serial_read_block(timeout=1)
        self.serial_write(data=f'wlan_get_if_ip -i {role_type}\n', info=f'cmd: wlan_get_if_ip')
        ip_str = self.serial_read_block()
        match = re.search(r'IP Address:\s*([\d\.]+)', ip_str)
        if match:
            ip_address = match.group(1)
            self.logger.info(f"IP: {ip_address}")
        return ip_address
    
    def wlan_ap_set_static_ip(self, ip, netmask, gateway, role_type='2', ip_mode='1'): 
        '''
        role_type: 0 - STA , 2 - AP 
        ip_mode: 0 - DHCP. 1 - STATIC
        '''
        self.serial_read_block(timeout=5)
        self.serial_write(data=f'wlan_set_if_ip -i {role_type} -ip {ip_mode} -c {ip} -v {netmask} -gw {gateway}\n', info=f'cmd: wlan_set_static_ip')
        self.serial_read_block(timeout=5)
    
    def wlan_ap_set_dhcp_server(self, lease_time, ip_start, ip_end):
        self.serial_write(data=f'wlan_set_dhcp -t {lease_time} -s {ip_start} -e {ip_end}\n', info=f'cmd: wlan_set_dhcp')
        self.serial_read_block(timeout=2)
        
    def wlan_ap_disconnect_sta(self, mac_address):
        self.serial_write(data=f'wlan_disconnect -m {mac_address}\n', info=f'cmd: wlan_disconnect')
        self.serial_read_block(timeout=5)

    def wlan_ap_set_wps_pin(self, pin="12345670", timeout=0):
        self.serial_write(data=f'set_wps_ap_pin -p \"{pin}\" -t {timeout}\n', info=f'cmd: set_wps_ap_pin')
        self.serial_read_block(timeout=5)
    
    '''
        -------------------- AP commands --------------------
    '''
        
        
    '''
        -------------------- P2P commands --------------------
    '''

    def wlan_p2p_role_up(self, regulatory_domain="00", channel="6", o_reg_class="81", listen_ch="11", l_reg_class="81", intent="5"):
        self.serial_write(data=f'p2p_role_up -r "{regulatory_domain}" -c {channel} -o {o_reg_class} -s {listen_ch} -m {l_reg_class} -i {intent}\n', info=f'cmd: p2p_role_up')
        self.serial_read_block()
        
    def wlan_p2p_find(self, ap_number='5'):
        self.serial_write(data=f'p2p_find -n {ap_number}\n', info=f'cmd: p2p_find')
        self.serial_read_block(timeout=15)
        # self.serial_write(data=f'p2p_stop_find \n', info=f'cmd: p2p_stop_find')
        # self.serial_read_block()

    def wlan_p2p_stop_find(self):
        self.serial_write(data=f'p2p_stop_find \n', info=f'cmd: p2p_stop_find')
        self.serial_read_block()

    def wlan_p2p_listen(self):
        self.serial_write(data='p2p_listen \n', info=f'cmd: p2p_listen')
        self.serial_read_block(timeout=2)
        #self.serial_write(data='p2p_cancel \n', info=f'cmd: p2p_cancel')
        #self.serial_read_block(timeout=2)
        
    def wlan_p2p_connect(self, mac_addr, wps_type, pin_code, intent):

        timeout = 30 #in seconds

        self.serial_write(data=f'p2p_cancel\n', info=f'cmd: p2p_cancel')
        self.serial_read_block(timeout=1)

        self.serial_write(data=f'p2p_stop_find\n', info=f'cmd: p2p_stop_find')
        self.serial_read_block(timeout=1)

        self.serial_write(data=f'p2p_set_channel -c 6 -o 81 -s 6 -m 81 -i {intent}\n', info=f'cmd: p2p_set_channel')
        self.serial_read_block(timeout=1)
        
        if pin_code is None:
            self.serial_write(data=f'p2p_connect -m {mac_addr} -w {wps_type} -t {timeout}\n', info=f'cmd: p2p_connect')
        else:
            self.serial_write(data=f'p2p_connect -m {mac_addr} -w {wps_type} -p \"{pin_code}\" -t {timeout}\n', info=f'cmd: p2p_connect')

        self.serial_read_block(timeout=2)
    
    def wlan_p2p_remove_group(self):
        self.serial_write(data='p2p_group_remove \n', info=f'cmd: p2p_group_remove')
        self.serial_read_block(timeout=5)
    
    def wlan_p2p_cancel(self):
        self.serial_write(data='p2p_cancel \n', info=f'cmd: p2p_cancel')
        self.serial_read_block(timeout=10)
        
    def wlan_p2p_get_mac(self, role_type='3'):
        self.serial_read_block(timeout=12)
        self.serial_write(data=f'wlan_get_mac -i {role_type}\n', info=f'cmd: wlan_ap_get_mac')
        mac_str = self.serial_read_block(timeout=2)
        mac = mac_str.split(" : ")[1].replace("user:", "").rstrip('\n\r')
        print(f"MAC: {mac}")
        return mac
    
    def wlan_p2p_set_params(self, channel="6", o_reg_class="81", listen_ch="6", l_reg_class="81", intent="5"):
        #self.serial_read_block()
        self.serial_write(data=f'p2p_set_channel -c {channel} -o {o_reg_class} -s {listen_ch} -m {l_reg_class} -i {intent}\n', info=f'cmd: p2p_set_channel')
        self.serial_read_block()

    def wlan_p2p_set_dynamic_ip(self, role_type='3', ip_mode='0'):
        '''
        role_type: 0 - STA , 2 - AP , 3 - P2P
        ip_mode: 0 - DHCP. 1 - STATIC
        '''
        self.serial_write(data=f'wlan_set_if_ip -i {role_type} -ip {ip_mode}\n', info=f'cmd: wlan_set_if_ip')
        self.serial_read_block()
        self._is_wlan_sta_dynamic_ip = True
    
    '''
        -------------------- P2P commands --------------------
    '''
    
    def init(self):
        pass

    def run(self):
        self.init()
        exit(0)


########################################################################################################################
class ColorHandler(logging.StreamHandler):
    # https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
    GRAY8 = "38;5;8"
    GRAY7 = "38;5;7"
    ORANGE = "33"
    RED = "31"
    WHITE = "0"

    def emit(self, record):
        # Don't use white for any logging, to help distinguish from user print statements
        level_color_map = {
            logging.DEBUG: self.GRAY7,
            logging.INFO: self.WHITE,
            logging.WARNING: self.ORANGE,
            logging.ERROR: self.RED,
        }

        csi = f"{chr(27)}["  # control sequence introducer
        color = level_color_map.get(record.levelno, self.WHITE)

        print(f"{csi}{color}m{record.msg}{csi}m")


########################################################################################################################
if __name__ == "__main__":
    script = DeviceBaseClass()
    script.run()