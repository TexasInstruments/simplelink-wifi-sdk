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
from .shared_enums import CommandOperation, DebugLogLevel, BssIdentifierBand, QuickTrackRequestTLV, WpsDeviceRole
from .command_interpreter import CommandInterpreter
from .command import Command
from datetime import datetime
from Commands.dut_logger import DutLogger, LogCategory
import os
from datetime import datetime

if not CommandHelper.device._is_uart_connect:
    CommandHelper.device.connect_serial(os.environ["CC35XX_PORT"])
    CommandHelper.device.wlan_start()
    CommandHelper.device.wlan_ap_role_up(ssid="test", sec_type="OPEN", 
                                         key="", channel='1', sta_limit="2", regulatory_domain="US", 
                                         hiddenAP=False, sae_pwe='0', transition_disable='0')

# import cc3xxx
# CC3XXX = cc3xxx.dev
# CC3XXX.start(os.environ["CC3XXX_PORT"], os.environ["CC3XXX_SERIAL_NUM"])
# CC3XXX._wlan_client.enablePrintReportEvent(2)
# #print(CC3XXX.wlan_get_fw_phy_versions())
# #print(CC3XXX.wlan_get_uppermac_version())

command_interpreter_obj = CommandInterpreter()

chosen_ssid = ""

class CC35xx_ApCommandHelper:
    store_test_artifcats = False

    @staticmethod
    def ap_start_up():
        """Starts the hostapd service on the AP."""

        return None, None

    @staticmethod
    def ap_stop():
        CommandHelper.device.wlan_ap_role_down()
        return None, None

    @staticmethod
    def set_ap_debug_log_level(log_level):
        pass

    @staticmethod
    def get_ap_if_status(if_name):
        hostapd_cli_status, _ = CommandHelper.run_shell_command(
            "sudo hostapd_cli -i {} status".format(if_name)
        )
        interface_freq = command_interpreter_obj.apply_cmd_regex(
            Command.GET_FREQ, hostapd_cli_status
        )
        interface_ssid = command_interpreter_obj.apply_cmd_regex(
            Command.GET_AP_SSID, hostapd_cli_status
        )
        mac_addr = command_interpreter_obj.apply_cmd_regex(
            Command.GET_AP_DUT_MAC_ADDR, hostapd_cli_status
        )
        return interface_freq, interface_ssid, mac_addr

    @staticmethod
    def get_center_freq_idx(chan, width: int = 1):
        if (width == 1):
            if chan >= 36 and chan <= 48:
                return "42"
            elif chan <= 64:
                return "58"
            elif chan >= 100 and chan <= 112:
                return "106"
            elif chan <= 128:
                return "122"
            elif chan <= 144:
                return "138"
            elif chan >= 149 and chan <= 161:
                return  "155"
        elif (width == 2):
            if chan >= 36 and chan <= 64:
                return "50"
            elif chan >= 100 and chan <= 128:
                return "114"

    @staticmethod
    def is_ht40plus_chan(chan):
        if (chan == 36 or chan == 44 or chan == 52 or chan == 60 or
            chan == 100 or chan == 108 or chan == 116 or chan == 124 or
            chan == 132 or chan == 140 or chan == 149 or chan == 157):
            return True
        else:
            return False

    @staticmethod
    def is_ht40minus_chan(chan):
        if (chan == 40 or chan == 48 or chan == 56 or chan == 64 or
            chan == 104 or chan == 112 or chan == 120 or chan == 128 or
            chan == 136 or chan == 144 or chan == 153 or chan == 161):
            return True
        else:
            return False

    '''
    @staticmethod
    def store_bss_identifiers(bss_identifier: int, ssid: str, interface_name):
        global bss_identifiers
        bss_identifiers[bss_identifier] = (ssid, interface_name)

    @staticmethod
    def get_bss_identifier_details(identifier: int):
        global bss_identifiers
        if identifier in bss_identifiers:
            return bss_identifiers[identifier]
        return None, None

    @staticmethod
    def get_bss_identifiers():
        global bss_identifiers
        return bss_identifiers
    '''

    @staticmethod
    def send_ap_disconnect(address):
        CommandHelper.device.wlan_ap_disconnect_sta(address)
        return None,None
    @staticmethod
    def ap_chan_switch(channel, freq):
        raise NotImplementedError

    @staticmethod
    def send_ap_btm_req(bssid, disassoc_immi, cand_list, disassoc_timer, retry_delay, bss_term_bit, bss_term_tsf, bss_term_duration):
        raise NotImplementedError

    @staticmethod
    def set_ap_param(param_str, value):
        raise NotImplementedError
        if_name = CommandHelper.get_interface_name()

        return  command_interpreter_obj.execute(
            Command.SET_AP_PARAM.value,
            [if_name, param_str, value],
        )

    @staticmethod
    def get_wsc_cred():
        global chosen_ssid
        # Get SSID, key_mgmt and Passphrase from config file
        key_list = ["ssid=", "wpa_passphrase=", "wpa_key_mgmt="]
        return [chosen_ssid,"12345678","WPA-PSK"],None
        config_list = []
        file_path = "/etc/hostapd/{}".format(hostapd_config_files[0])
        with open(file_path, "r") as config_f:
            config =config_f.read()
            for key in key_list:
                index = config.find(key)
                if index == -1:
                    DutLogger.log(LogCategory.INFO, "Cannot find the setting: {}".format(key))
                    config_list.append("")
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
    def ap_start_wps(pin_code):
        method = ""
        if pin_code:
            method = "PIN"
        else:
            method = "PBC"
        CommandHelper.device.wlan_ap_wps(method, pin_code)
        return None,None

    @staticmethod
    def ap_configure_wsc(config_enums: dict):
        global chosen_ssid 

        CommandHelper.device.wlan_ap_role_down()
        channel = config_enums["channel"] #int(config_enums["channel"])

        if "ssid" in config_enums:
            chosen_ssid = config_enums["ssid"]
        else:
            chosen_ssid = "ap-wps"

        wps_er_support = config_enums.get("wps_er_support")

        # CC3XXX.wlan_ap_set_config(chosen_ssid,"WPA-PSK","12345678",channel,0,2,"US",False)
        CommandHelper.device.wlan_ap_role_up(ssid=chosen_ssid, sec_type="WPA-PSK", key="12345678",
                                             channel=channel, sta_limit="2", regulatory_domain="US", hiddenAP=False)

        if (wps_er_support == '1'):
            CommandHelper.device.wlan_ap_set_wps_pin("12345670", 0)
        return None, None
        __class__.ap_stop()
        config_only = config_enums.pop("wsc_config_only", None)
        ApCommandHelper.assign_interface_and_config_file_name(config_enums)
        __class__.create_hostapd_config(config_enums, False)
        if config_only is not None:
            return "Configure wsc ap successfully. (Configure only)", None
        
        __class__.ap_start_up()
        return "Confiugre and start wsc ap successfully. (Configure and start)", None

    @staticmethod
    def get_wsc_pin():
        CommandHelper.device.wlan_ap_wps("PIN", "12345670")
        return "12345670",None
 

    @staticmethod
    def ap_configure(config: dict):
        global chosen_ssid 

        chosen_ssid = config["ssid"]
        key = ""

        sec_type = config.get("wpa_key_mgmt")
        wep_key0 = config.get("wep_key0")
        sae_pwe = config.get("sae_pwe")

        transition_disable = config.get("transition_disable")
        if transition_disable is None:
            transition_disable = '0'

        if sec_type is not None and wep_key0 is not None:
            raise Exception("Invalid security")

        if sec_type is not None:
            # if sec_type == "NONE":
            #     sec_type = "OPEN"
            if sec_type == "WPA-PSK" and config.get("wpa", "") != "2":
                sec_type = "WPA-PSK"
            elif sec_type == "WPA-PSK" and config.get("wpa", "") == "2":
                sec_type = "WPA2-PSK"
            elif sec_type == "WPA-PSK WPA-PSK-SHA256" and config.get("wpa", "") == "2":
                sec_type = "WPA2-PSK"
            elif sec_type == "SAE":
                sec_type = "WPA3"
            elif sec_type == "SAE WPA-PSK" and config.get("wpa", "") == "2":
                sec_type = "WPA2-WPA3"
            else:
                raise NotImplementedError

            key = config.get("wpa_passphrase", "")
            
        if sec_type == "NONE" or not sec_type:
                sec_type = "OPEN"

        if wep_key0 is not None:
            sec_type = "WEP"
            key = wep_key0

        channel = int(config["channel"])
        country_code = config.get("country_code", "US")

        if sae_pwe:
            sae_pwe = sae_pwe
        else:
            sae_pwe = '2' #default is both hunting-and-pecking and h2e

        CommandHelper.device.wlan_ap_role_down()
        CommandHelper.device.wlan_ap_role_up(ssid=chosen_ssid, 
                                             sec_type=sec_type, 
                                             key=key, 
                                             channel=channel, 
                                             sta_limit="2", 
                                             regulatory_domain=country_code, 
                                             hiddenAP=False, 
                                             sae_pwe=sae_pwe, 
                                             transition_disable=transition_disable)
        return None, None