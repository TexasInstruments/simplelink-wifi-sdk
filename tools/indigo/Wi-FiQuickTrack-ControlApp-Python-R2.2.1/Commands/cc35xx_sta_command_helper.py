# Copyright (c) 2020 Wi-Fi Alliance

# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.

# THE SOFTWARE IS PROVIDED 'AS IS' AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
# CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
# OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
# SOFTWARE.

# Texss Instruments 2023-2024

try:
    from .cc35xx_command_helper import CC35xx_CommandHelper as CommandHelper
except ImportError:
    from .command_helper import CommandHelper
from dataclasses import dataclass
from .shared_enums import DebugLogLevel, P2PConnType, QuickTrackRequestTLV, WpsDeviceRole
from .command_interpreter import CommandInterpreter
from .command import Command
import os
from datetime import datetime
from Commands.dut_logger import DutLogger, LogCategory
from datetime import datetime


if not CommandHelper.device._is_uart_connect:
    CommandHelper.device.connect_serial(os.environ["CC35XX_PORT"])
    CommandHelper.device.wlan_start()
    CommandHelper.device.wlan_sta_role_up()
# CC35XX.start(os.environ["CC35XX_PORT"], os.environ["CC35XX_SERIAL_NUM"])
# CC35XX._wlan_client.enablePrintReportEvent(2)
#CC35XX.wlan_sta_role_up()
ACTIVE_ROLE = None

command_interpreter_obj = CommandInterpreter()
store_wpas_config_for_debug = False
sta_debug_log_level = DebugLogLevel.DISABLE
wpa_supplicant_log_folder_path = "/var/log/supplicant.log"
wpa_supplicant_config_file = "/etc/wpa_supplicant/wpa_supplicant.conf"
# Default DUT GO intent value
P2P_GO_INTENT = 7

wpa_config_header = (
    "sta_sae_groups", "mbo_cell_capa", "sae_pwe",
    "update_config",
)

def get_server_cert_hash(pem_file):
    # These were generated with: openssl x509 -outform der -in $pemname | openssl dgst -sha256
    entries = {
        "rsa_server1_w1_fi.pem": "a7407d995678712bb7adb4e7a75e89674aba363dea0b8308c63b006329b0de2d",
        "rsa_server1ALT_w1_fi.pem": "79a9d7273368bee41566f79ae9fc84119f7c963cf8cfac5984e2e0adaeafb112",
        "rsa_server2_w1_fi.pem": "8d0e00b924e30f4595ae7f5ef9f1346e2c3f343dfb1caf1429b3bb6b32a1bf03",
        "rsa_server4_w1_fi.pem": "2703264d2d06727be661752ff5b57e85f842dc74e18aaa03316e7b2d08db6260",
    }
    return "hash://server/sha256/{}".format(entries[pem_file])

@dataclass 
class _StaConfig:
    ssid: str = ""
    sec_type: str = ""
    key: str = ""
    bssid: str = ""
    eap_phase1: str = ""
    eap_phase2: str = ""
    identity: str = ""
    client_cert_filename: str = ""
    ca_cert_filename: str = ""
    private_key_filename: str = ""
    suiteb192 = 0        

_STA_CONFIG = _StaConfig()

class CC35xx_StaCommandHelper:
    store_test_artifcats = False

    @staticmethod
    def sta_configure(params: dict):
        """Method to configure the wpa supplicant.

        Parameters
        ----------
        params : [dict]
            [parameters for sta wpa_supplicant configuration.]

        merge_config_file : bool
            Flag to indicate if new supplicant config file has to be create or the new config has to be merged into existing file
        """

        _STA_CONFIG.ssid = params.get("sta_ssid", "")
        _STA_CONFIG.key = params.get("psk", "")
        _STA_CONFIG.bssid = params.get("bssid", "")

        _STA_CONFIG.identity = ""
        _STA_CONFIG.eap_phase1 = ""
        _STA_CONFIG.eap_phase2 = ""
        _STA_CONFIG.private_key_filename = ""
        _STA_CONFIG.suiteb192 = 0
                                
        _STA_CONFIG.ca_cert_filename = ""
        _STA_CONFIG.client_cert_filename = ""

        _STA_CONFIG.sec_type = ""
        
        group = params.get("group") 
        val = params.get("key_mgmt")    
        if val == "NONE":
            _STA_CONFIG.sec_type = "OPEN"
        elif val == "WPA-PSK" and group == "TKIP":
            _STA_CONFIG.sec_type = "WPA-ONLY"
        elif val == "WPA-PSK" and group == "CCMP TKIP":
            _STA_CONFIG.sec_type = "WPA-PSK"
        elif val == "WPA-PSK":
            _STA_CONFIG.sec_type = "WPA2_PLUS"
        elif val == "WPA2-PSK":
            _STA_CONFIG.sec_type = "WPA2_PLUS"
        elif val == "WPA-PSK WPA-PSK-SHA256":
            _STA_CONFIG.sec_type = "WPA2_PLUS"
        elif val == "WEP":
            _STA_CONFIG.sec_type = val
        elif val == "SAE":
            _STA_CONFIG.sec_type = "WPA3"
        elif val == "SAE WPA-PSK":
            _STA_CONFIG.sec_type = "WPA2-WPA3"
        elif val == "WPA-EAP" or val == "WPA-EAP-SHA256" or val == "WPA-EAP-SUITE-B-192":
            if val == "WPA-EAP":
                _STA_CONFIG.sec_type = "WPA2-PSK"
                        
            elif val == "WPA-EAP-SHA256" or val == "WPA-EAP-SUITE-B-192":
                _STA_CONFIG.sec_type = "WPA3"
                if val == "WPA-EAP-SUITE-B-192":
                    _STA_CONFIG.suiteb192 = 1
                
            eap = params.get("eap", "")            
            eap_phase1= params.get("phase1", "") 

            if eap == "TLS":
                _STA_CONFIG.eap_phase1 = "TLS"
            elif eap == "TTLS":
                _STA_CONFIG.eap_phase1 = "TTLS"
            elif eap =="PEAP":
                if eap_phase1 == "peapver=0":
                    _STA_CONFIG.eap_phase1 = "PEAP0"
                elif not eap_phase1:
                    _STA_CONFIG.eap_phase1 = "PEAP1"
            
            _STA_CONFIG.eap_phase2 = "MSCHAP"#"WLAN_MSCHAPV2_TYPE"
            _STA_CONFIG.identity = params.get("identity", "")
            _STA_CONFIG.key = params.get("password", "")
            
            private_key = params.get("private_key", "")
            ca_cert = params.get("ca_cert", "")
            client_cert = params.get("client_cert", "")

            
            _STA_CONFIG.private_key_filename = _get_correct_filename(private_key)
            _STA_CONFIG.ca_cert_filename = _get_correct_filename(ca_cert)
            _STA_CONFIG.client_cert_filename = _get_correct_filename(client_cert)

        else:
            raise NotImplementedError

        # CommandHelper.device.wlan_set_power_management_mode(mode='0') for 10092
        
        is_active_from_env = os.environ["IS_RUN_ACTIVE_MODE"] in ('true', '1', 'True',"TRUE")
        if is_active_from_env:
            CommandHelper.device.wlan_set_power_management_mode(mode="0") #for 10092
        
        CommandHelper.device.wlan_sta_role_up() 
        CommandHelper.device.wlan_sta_del_profile(255) # delete all profiles
        CommandHelper.device.wlan_sta_set_connection_policy(False, True, False) # enable Auto

        return "Wpa supplicant successfully configured.", None

    @staticmethod
    def sta_associate(is_reassociate = False):
        """Method to start the wpa_supplicant service."""
        global ACTIVE_ROLE
        ACTIVE_ROLE = "STA"
        bssid = None if not _STA_CONFIG.bssid else _STA_CONFIG.bssid

        if _STA_CONFIG.eap_phase1:
            CommandHelper.device.set_date_time('2023', '11', '23', '14', '34', '00')

            print(_STA_CONFIG.sec_type)
            CommandHelper.device.wlan_sta_role_up(timeout=2) 
            CommandHelper.device.wlan_sta_connect_ent(_STA_CONFIG.ssid,_STA_CONFIG.sec_type,_STA_CONFIG.key, bssid,_STA_CONFIG.eap_phase1,_STA_CONFIG.eap_phase2,
                                        identity=_STA_CONFIG.identity,
                                        anonymous_user = "",
                                        client_cert_filename=_STA_CONFIG.client_cert_filename,
                                        ca_cert_filename=_STA_CONFIG.ca_cert_filename,
                                        private_key_filename=_STA_CONFIG.private_key_filename,
                                        timeout=10,
                                        flags=0,
                                        suiteb192 = _STA_CONFIG.suiteb192
                                        )
        else:
            if not is_reassociate: # skip on reconnection
                
                #CommandHelper.device.wlan_sta_role_up() 
                #CommandHelper.device.wlan_sta_del_profile(255) # delete all profiles
                #CommandHelper.device.wlan_sta_set_connection_policy(False, True, False) # enable Auto
                #CommandHelper.pause_execution(1)
                CommandHelper.device.wlan_sta_disconnect() 
                #CommandHelper.pause_execution(1)
                CommandHelper.device.wlan_sta_add_profile(_STA_CONFIG.ssid,_STA_CONFIG.sec_type, _STA_CONFIG.key,bssid)
                #CommandHelper.pause_execution(3)

                #no profiles
                """CommandHelper.device.wlan_sta_role_up() 
                CommandHelper.device.wlan_sta_disconnect()
                CommandHelper.pause_execution(1)
                CommandHelper.device.wlan_sta_set_connection_policy(False,False,False) # enable Auto
                CommandHelper.device.wlan_sta_connect(_STA_CONFIG.ssid, _STA_CONFIG.sec_type, _STA_CONFIG.key,bssid, 1)"""
 
        #CommandHelper.pause_execution(3)

        return None

    @staticmethod
    def sta_disconnect():
        """Method to stop the wpa_supplicant service.

        Parameters
        ----------
        interface_name : [str]
            [interface name on which the supplicant needs to be stopped.]
        """
        global ACTIVE_ROLE
        if ACTIVE_ROLE == "STA":
            #device.wlan_sta_role_up()
            CommandHelper.device.wlan_sta_del_profile(255) 
            CommandHelper.device.wlan_sta_set_connection_policy(False, False, False) # disable policies between testes
            CommandHelper.device.wlan_sta_disconnect(timeout=5)
        elif ACTIVE_ROLE == "P2P":
            #CC35XX.wlan_p2p_role_up()
            if 1: # issue with cancel/remove while connect is going on
                CommandHelper.device.wlan_p2p_cancel()
                CommandHelper.device.wlan_sta_disconnect()
                CommandHelper.device.wlan_p2p_remove_group()
            else: # fix this ^
                CC35XX.stop()
                CC35XX.start(os.environ["CC35XX_PORT"], os.environ["CC35XX_SERIAL_NUM"])
                CC35XX.enable_print_report_events(0)
                CommandHelper.device.wlan_p2p_role_up()

        return None, None

    @staticmethod
    def set_sta_debug_log_level(log_level):
        pass

    @staticmethod
    def get_sta_if_status(if_name):
        global ACTIVE_ROLE
        if ACTIVE_ROLE == "STA":
            CommandHelper.device.wlan_sta_role_up(timeout=5) 
            mac_addr = CommandHelper.device.wlan_sta_get_mac()
            status = CC35XX.wlan_connection_status
        elif ACTIVE_ROLE == "P2P":
            status = CC35XX.wlan_p2p_connection_status
            if status is not None:
                if not status.is_go:
                    mac_addr = CommandHelper.device.wlan_sta_get_mac()
                else:
                    mac_addr = CommandHelper.device.wlan_ap_get_mac()

        if status is not None:
            interface_freq = _calc_freq_from_channel(status.channel)
            interface_ssid = status.ssid
        else:
            interface_ssid = ""
            interface_freq = -1
        return interface_freq, interface_ssid, mac_addr

    @staticmethod
    def start_up_p2p():
        """Method to start the wpa_supplicant service."""
        global ACTIVE_ROLE
        CommandHelper.device.wlan_p2p_role_up()
        CommandHelper.device.p2p_mac = CommandHelper.device.wlan_p2p_get_mac()
        ACTIVE_ROLE = "P2P"

        return None
    
    @staticmethod
    def send_sta_btm_query(reason_code, cand_list):
        raise NotImplementedError

    @staticmethod
    def send_sta_anqp_query(bssid, id_param):
        raise NotImplementedError

    @staticmethod
    def send_sta_disconnect():
        CommandHelper.device.wlan_sta_disconnect(timeout=1)

        return None, None

    @staticmethod
    def sta_reassociate():
        #if not CC35XX.is_wlan_connected: 
        __class__.sta_associate(True)

        return None, None

    @staticmethod
    def set_sta_param(param_str, value):
        raise NotImplementedError
        if_name = CommandHelper.get_interface_name()

        return command_interpreter_obj.execute(
            Command.SET_STA_PARAM.value,
            [if_name, param_str, value],
        )

    @staticmethod
    def p2p_find():
        CommandHelper.device.wlan_p2p_find()
        return None, None

    @staticmethod
    def p2p_listen():
        CommandHelper.device.wlan_p2p_listen()
        return None, None

    @staticmethod
    def add_p2p_group(freq):
        raise NotImplementedError
        return None, None

    @staticmethod
    def p2p_start_wps(pin_code):
        raise NotImplementedError

    @staticmethod
    def get_go_intent_value():
        return P2P_GO_INTENT

    @staticmethod
    def p2p_connect(addr, pin_code, method, conn_type, intent_value, he, persist):
        if he:
            pass
        if persist:
            pass

        if method == "pbc": #PBC
            wps_type = "0"
        elif method == "display": #PIN-DISPLAY
            wps_type = "1"
        elif method == "keypad": #PIN
            wps_type = "2"
        else:
            raise Exception("...")
        
        initiate = True
        join_auto_go = False
        
        if conn_type:
            if conn_type == P2PConnType.JOIN.value:
                join_auto_go = True
            if conn_type == P2PConnType.AUTH.value: # Wait Peer GO NEG Req
                initiate = False
        
        CommandHelper.device.wlan_p2p_connect(addr, wps_type, pin_code, intent_value) 
        return None, None

    @staticmethod
    def p2p_invite(mac, persist, freq):
        raise NotImplementedError
        return None, None

    @staticmethod
    def stop_p2p_group(persist):
        raise NotImplementedError
        
        if persist:
            # Clear the persistent group with id 0
            pass

        return None, None

    @staticmethod
    def set_p2p_serv_disc(addr):
        raise NotImplementedError

    @staticmethod
    def set_p2p_ext_listen():
        raise NotImplementedError

    @staticmethod
    def get_wsc_pin():
        return "12345670", None
        raise NotImplementedError

    @staticmethod
    def get_wsc_cred():
        raise NotImplementedError
        # Get SSID, key_mgmt and Passphrase from config file
        key_list = ["ssid=", "psk=", "key_mgmt="]
        config_list = []
        with open(wpa_supplicant_config_file, "r") as config_f:
            config =config_f.read()
            for key in key_list:
                index = config.find(key)
                if index == -1:
                    DutLogger.log(LogCategory.ERROR,
                                "Cannot find the setting: {}".format(key))
                    return config_list, "Cannot find the setting"
                else:
                    index += len(key)
                    if config[index] == '\"':
                        #  the format aaaaa="xxxxxxxx"
                        index += 1
                        end = config[index:].find('\"')
                    else:
                        # the format bbbbb=yyyyyyyy
                        end = config[index:].find('\n')
                    config_list.append(config[index:index+end])
        return config_list, None


    
    @staticmethod
    def sta_start_wps(pin_code):
        CommandHelper.device.wlan_sta_role_up()
        if pin_code: 
            wps_type = "WPS-PIN"
        else:
            pin_code = ""
            wps_type = "WPS-PBC"
        
        #CC35XX.wlan_sta_connect("", wps_type, pin_code, "", timeout=50) original
        CommandHelper.pause_execution(1)
        print(CommandHelper.device.wlan_sta_get_mac())
        if wps_type == "WPS-PIN":
            pin = "12345670"
            CommandHelper.device.wlan_sta_connect("", wps_type, str(pin)) 
            return pin, None
        #print(CC35XX.wlan_sta_get_mac())
        CommandHelper.device.wlan_sta_connect("", wps_type, pin_code) 
        return "", ""
        raise NotImplementedError
        if_name = CommandHelper.get_interface_name()
        if pin_code is not None:
            if len(pin_code) == 4 or len(pin_code) == 8:
                return CommandHelper.run_shell_command(
                    ("sudo wpa_cli -i {} wps_pin any {}").format(if_name, pin_code)
                )
            elif len(pin_code) == 1 and pin_code == '0':
                return CommandHelper.run_shell_command(
                    ("sudo wpa_cli -i {} wps_pin any").format(if_name)
                )
            else:
                DutLogger.log(LogCategory.ERROR, "Unrecognized PIN :" + pin_code)
                return "Failed", "Unrecognized PIN"
        else:    
            return CommandHelper.run_shell_command(
                ("sudo wpa_cli -i {} wps_pbc").format(if_name)
            )

    @staticmethod
    def sta_enable_wsc(config_enums: dict):
        return
        raise NotImplementedError
        """Method to enable WSC."""
        interface_name = CommandHelper.get_interface_name()
        CC35xx_StaCommandHelper.clear_supplicant_logs()

        CommandHelper.run_shell_command("sudo rfkill unblock wlan")
        CommandHelper.run_shell_command("sudo killall wpa_supplicant")
        CommandHelper.pause_execution(3)

        wpa_supplicant_config = "ctrl_interface=/var/run/wpa_supplicant\nap_scan=1\npmf=1\n"
 
        # Global settings
        for each_config_enum in config_enums:
            if each_config_enum in wpa_config_header:
                field_value = config_enums.get(each_config_enum)
                wpa_supplicant_config += each_config_enum + "=" + field_value + "\n"
        
        # WPS settings
        wps_enable = config_enums.get("wps_enable")
        if wps_enable is None:
                error = "No WPS_ENABLE TLV found. Failed to append STA WSC data"
                DutLogger.log(LogCategory.ERROR, error)
                return error
        else:
            wps_setting = CommandHelper.get_wps_settings(WpsDeviceRole.WPS_STA)
            if not wps_setting:
                error = "Failed to get STAUT WPS settings"
                DutLogger.log(LogCategory.ERROR, error)
                return error
            elif wps_enable == "1":
                # Enable Normal
                for cfg_item in wps_setting:
                    wpa_supplicant_config += cfg_item[0] + "=" + cfg_item[1] + "\n"
            else:
                error = "Invalid WPS TLV value: {}".format(wps_enable)
                DutLogger.log(LogCategory.ERROR, error)
                return error
            with open(wpa_supplicant_config_file, "w") as file:
                file.write(wpa_supplicant_config)

        supplicant_start_command = "sudo /usr/local/bin/WFA-Hostapd-Supplicant/wpa_supplicant -B -t -c {} -i {}".format(wpa_supplicant_config_file, interface_name)
        log_level = CC35xx_StaCommandHelper.__get_sta_debug_log_level()
        if log_level:
            supplicant_start_command += " {} -f {}".format(log_level, wpa_supplicant_log_folder_path)
        CommandHelper.run_shell_command(supplicant_start_command)

        return None


def _calc_freq_from_channel(self, channel):
    if channel >= 149:
        return ((channel - 149)*5 + 5745)
    if channel >= 36:
        return ((channel - 36)*5 + 5180)
    if channel >= 1:
        return ((channel - 1)*5 + 2412)

def _calc_channel_from_freq(self, freq):
    freq = int(freq)
    if freq >= 5745:
        return ((freq - 5745) / 5 + 149)
    if freq >= 5180:
        return ((freq - 5180) / 5 + 36)
    if freq >= 2412:
        return ((freq - 2412) / 5 + 1)
    
def _get_correct_filename(filename):  #TODO: Change path to global
    relative_path = os.path.join(os.getcwd(), "Certificates")
    if "wifiuser.pem" in filename:
        return os.path.join(relative_path, "wifiuser.pem") #r"C:\Osprey\mx\Certificates\wifiuser.pem"
    elif "cas.pem" in filename:
        return os.path.join(relative_path, "cas.pem") #r"C:\Osprey\mx\Certificates\cas.pem"
    elif "rsa_ca1_w1_fi" in filename:
        return os.path.join(relative_path, "rsa_ca1_w1_fi.pem") #r"C:\Osprey\mx\Certificates\rsa_ca1_w1_fi.pem"
    elif "rsa_user1_w1_fi" in filename:
        return os.path.join(relative_path, "rsa_user1_w1_fi.pem") #r"C:\Osprey\mx\Certificates\rsa_user1_w1_fi.pem"
    elif "rsa_ca1_ca1BAD_w1_fi" in filename:
        return os.path.join(relative_path, "rsa_ca1_ca1BAD_w1_fi.pem") #r"C:\Osprey\mx\Certificates\rsa_ca1_ca1BAD_w1_fi.pem"
    elif "rsa_ca1BAD_w1_fi" in filename:
        return os.path.join(relative_path, "rsa_ca1BAD_w1_fi.pem") #r"C:\Osprey\mx\Certificates\rsa_ca1BAD_w1_fi.pem"
    elif "rsa_ca2_w1_fi" in filename:
        return os.path.join(relative_path, "rsa_ca2_w1_fi.pem") #r"C:\Osprey\mx\Certificates\rsa_ca2_w1_fi.pem"
    else:
        return None