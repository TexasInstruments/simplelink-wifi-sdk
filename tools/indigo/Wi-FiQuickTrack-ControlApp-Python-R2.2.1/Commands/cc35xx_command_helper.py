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

"""Utility module used in api commands."""
import subprocess
import os
from pathlib import Path
from .shared_enums import *

import socket
import struct

write_commands_into_file = False
# Set this flag to True if command and its output should be logged in a debug file
current_path = Path(os.path.realpath(""))
debug_file_path = (
    current_path / "QuickTrack-Tool/Test-Services/command_execution_debug_file.txt"
)
from time import sleep
from Commands.dut_logger import DutLogger, LogCategory
from .command import Command
from .command_interpreter import CommandInterpreter

import DeviceBaseClass


# import cc3xxx
# CC3XXX = cc3xxx.dev
# CC3XXX.start(os.environ["CC3XXX_PORT"], os.environ["CC3XXX_SERIAL_NUM"])
# CC3XXX._wlan_client.enablePrintReportEvent(2)
# print(CC3XXX.wlan_get_fw_phy_versions())
# print(CC3XXX.wlan_get_uppermac_version())

command_interpreter_obj = CommandInterpreter()

class CC35xx_CommandHelper:
    """Class that contains all the utility methods for processing api commands."""

    INTERFACE_LOGICAL_NAME = None
    STATIC_IP = None
    INTERFACE_LIST = []
    BSSID_COUNT = 0
    BRIDGE_WLANS = "br-wlans"
    DHCP_SERVER_IP = "192.168.65.1"
    
    device = DeviceBaseClass.DeviceBaseClass()
    
    if not device._is_uart_connect:
        device.connect_serial(os.environ["CC35XX_PORT"])
        device.wlan_start()
        device.print_fw_version()

    @staticmethod
    def add_tlvs(command_type, command_data):
        """Helper method for adding the TLV's(type-length-value) to the command_raw_data.

        Parameters
        ----------
        command_type : [byte]
            [Type of Command]
        command_data : [byte_array]
            [Commands value ]

        """
        single_tlv_data = []
        single_tlv_data.append(command_type)
        single_tlv_data.append(len(command_data))
        single_tlv_data.extend(command_data)
        return single_tlv_data

    @staticmethod
    def run_shell_command(shell_command: str, new_terminal=False):
        raise NotImplementedError

    @staticmethod
    def __write_commands_into_debug_file(shell_command: str, command_output: str):
        """Writes the shell command and its output into file for Debug purpose.

        Parameters
        ----------
        shell_command : str
            Command that is being executed.
        command_output : str
            Output of the command being executed.
        """
        if write_commands_into_file is True:
            try:
                file_obj = open(debug_file_path, "a")
                file_obj.write("###################\nCOMMAND: " + shell_command)
                file_obj.write("\n" + command_output)
            except Exception:
                pass

    @staticmethod
    def pause_execution(time: int):
        """Utility method for sleep/wait

        Parameters
        ----------
        time : [int]
            [sleep time specified in seconds.]
        """
        DutLogger.log(LogCategory.INFO, ("Execution paused for {} seconds").format(time))
        sleep(time)

    @staticmethod
    def check_if_root_user():
        return True

    @staticmethod
    def check_wlan_created(if_name):
        raise NotImplementedError

    @staticmethod
    def create_wlan_if(if_name):
        raise NotImplementedError

    @staticmethod
    def get_hw_addr(ifname):
        # https://stackoverflow.com/questions/159137/getting-mac-address
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        info = fcntl.ioctl(s.fileno(), 0x8927, struct.pack('256s', bytes(ifname, 'utf-8')[:15]))
        return ':'.join('%02x' % b for b in info[18:24])

    @staticmethod
    def create_new_interface_bridge_network():
        """Returns the new interface bridge network."""
        raise NotImplementedError

    @staticmethod
    def add_all_interfaces_to_bridge():
        """Creates a bridge network with the existing wireless interface and the
        wireless interface name sent from the tool
        """
        raise NotImplementedError

    @staticmethod
    def get_all_interface_ip():
        return "", ""

    @staticmethod
    def reset_bridge_network():
        """Clears all the bridge network if any and will reassign the ip that was
        assigned to the wireless interface before bridge was created
        """
        #raise NotImplementedError
        pass

    @staticmethod
    def reset_interface_ip(if_name):
        pass

    @staticmethod
    def assign_static_ip(dut_static_ip, input_interface_name=None):
        """Assigns the static IP for the DUT."""
        __class__.STATIC_IP = dut_static_ip
        if __class__.device._is_wlan_sta_role_up:
            __class__.device.wlan_sta_set_static_ip(dut_static_ip, "255.255.255.0", "0.0.0.0")
        elif __class__.device._is_wlan_ap_role_up:
            __class__.device.wlan_ap_set_static_ip(dut_static_ip, "255.255.255.0", "0.0.0.0")
        elif 0: #CC3XXX._is_wlan_p2p_role_up:
            status = CC3XXX.wlan_p2p_connection_status
            if not status.is_connected:
                raise Exception("p2p not connected")
            if not status.is_go:
                __class__.device.wlan_sta_set_static_ip(dut_static_ip, "255.255.255.0", "0.0.0.0")
            else:
                __class__.device.wlan_ap_set_static_ip(dut_static_ip, "255.255.255.0", "0.0.0.0")
        #else:
        #    raise Exception(f"STA/AP role is not up")

        return True

    @staticmethod
    def clear_bss_identifiers():
        # Reset bss_id in interface list
        for x in __class__.INTERFACE_LIST:
            x[2] = 0
        __class__.BSSID_COUNT = 0
        # Todo: Need to create not first WLAN again as hostapd will remove bss in appending conf case

    @staticmethod
    def set_interface_bss_id(band, bss_id):
        if __class__.INTERFACE_LIST:
            for if_info in __class__.INTERFACE_LIST:
                if if_info[0] == band and if_info[2] == 0:
                    if_info[2] = bss_id
                    __class__.BSSID_COUNT += 1
                    return if_info[1]
        DutLogger.log(LogCategory.INFO, "Can't set bss id to available interface based on band, Please check --interface argument")
        DutLogger.log(LogCategory.INFO, "use default wireless interface")
        return __class__.INTERFACE_LOGICAL_NAME

    @staticmethod
    def get_interface_name(bss_id = None):
        if bss_id is not None:
            for if_info in __class__.INTERFACE_LIST:
                if if_info[2] == bss_id:
                    return if_info[1]
            return None
        if __class__.INTERFACE_LOGICAL_NAME: 
            return __class__.INTERFACE_LOGICAL_NAME
        elif __class__.INTERFACE_LIST:
            for if_info in __class__.INTERFACE_LIST:
                if if_info[2] > 0:
                    return if_info[1]
            return __class__.INTERFACE_LIST[0][1]
        DutLogger.log(LogCategory.ERROR, "Can't get any valid interface, Please check --interface argument")
        return None

    # Return the list of all wireless interface name
    @staticmethod
    def get_all_wlan_name():
        return ["wlan0", "wlan1", "p2p0"]

    @staticmethod
    def get_if_ip_addr(if_name):
        if if_name == "wlan0":
            return __class__.device.wlan_get_ip("0")
        elif if_name == "wlan1":
            return __class__.device.wlan_get_ip("2")
        elif if_name == "p2p0":
            return __class__.device.wlan_get_ip("3")      
            status = CC3XXX.wlan_p2p_connection_status

            if status.is_go:
                #for _ in range(100):
                #    leases = DEVICE._get_dhcp_server_leases(DEVICE_AP.alias)
                #    if leases:
                #        break
                #    sleep(1)

                return CC3XXX.ip_v4 # used to be ip_v4_ap, but it doesn't work
            else:
                if __class__.device.is_wlan_sta_dynamic_ip:
                    for i in range(100):
                        ip = CC3XXX.ip_v4
                        if ip and ip != "0.0.0.0":
                            return ip
                        sleep(1)

        raise NotImplementedError

    @staticmethod
    def get_if_mac_addr(if_name):
        if if_name == "wlan0":
            __class__.device.wlan_sta_role_up(timeout=5) 
            return __class__.device.wlan_sta_get_mac()
        elif if_name == "wlan1":
            return __class__.device.wlan_ap_get_mac()
        elif if_name == "p2p0":
            raise NotImplementedError
        raise NotImplementedError

    @staticmethod
    def verify_band_from_freq(freq: str, band: str):
        """Utility method to return the operational band based on the frequency.
        Parameters
        ----------
        freq : str
            Frequency value to be verified if present in required band.
        band : OperationalBand
            Band value to be verified with
        """
        current_op_band = None
        _, _24G_frequencies = ChannelFreqConfig.get_24G_channels_frequencies()
        _, _5G_frequencies = ChannelFreqConfig.get_5G_channels_frequencies()
        if freq in _24G_frequencies:
            current_op_band = OperationalBand._24GHz.name
        elif freq in _5G_frequencies:
            current_op_band = OperationalBand._5GHz.name

        if current_op_band == band:
            return True
        else:
            return False

    # Return addr of P2P-device if there is no GO or client interface
    @staticmethod
    def get_p2p_mac_addr():
        return __class__.device.p2p_mac
        #return __class__.device.wlan_p2p_get_mac() #wlan_p2p_dev_get_mac()

    @staticmethod
    def get_p2p_group_interface():
        #raise NotImplementedError
        return "p2p0"

        DutLogger.log(LogCategory.INFO, "Can't get P2P Group Interface")
        return None

    @staticmethod
    def get_p2p_dev_interface():
        raise NotImplementedError
        p2p_dev = "p2p-dev-{}".format(__class__.INTERFACE_LOGICAL_NAME)
        return p2p_dev

    @staticmethod
    def start_dhcp_server(if_name, ip_addr: str):
        ip_sub, _, _ = ip_addr.rpartition(".")
        ip_start = ip_sub + ".50"
        ip_end = ip_sub + ".200"

        __class__.device.wlan_ap_set_dhcp_server(1, ip_start, ip_end)
            

    @staticmethod
    def start_dhcp_client(if_name):
        #__class__.device.wlan_ap_set_dhcp_server(2000,"10.0.0.4","10.0.0.6")
        #__class__.device.wlan_ap_set_dhcp_server(2000,"10.0.0.4","10.0.0.6")
        return


    @staticmethod
    def stop_dhcp_server():
        return #Or - Not relevant in our application
        CC3XXX.wlan_ap_set_dhcp_server(False)

    @staticmethod
    def stop_dhcp_client():
        return 
        #if not CC35xx_CommandHelper.STATIC_IP:
        #    CC35xx_CommandHelper.STATIC_IP = "10.0.0.10"
        #__class__.device.wlan_sta_set_static_ip(CC35xx_CommandHelper.STATIC_IP, "255.255.255.0", "0.0.0.0")
    
    @staticmethod
    def get_process_id(name):
        raise NotImplementedError

    @staticmethod
    def get_wps_settings(role):
        wps_config = []
        if role == WpsDeviceRole.WPS_AP:
            if os.path.exists("/tmp/wsc_settings_APUT"):
                with open("/tmp/wsc_settings_APUT", "r") as file:
                    for line in file:
                        config_list = line.split(":")
                        wps_config.append(config_list)
            else:
                DutLogger.log(LogCategory.ERROR, "APUT: WPS Erorr. Failed to get settings.")
        else:
            if os.path.exists("/tmp/wsc_settings_STAUT"):
                with open("/tmp/wsc_settings_STAUT", "r") as file:
                    for line in file:
                        config_list = line.split(":")
                        wps_config.append(config_list)
            else:
                DutLogger.log(LogCategory.ERROR, "STAUT: WPS Erorr. Failed to get settings.")
        return wps_config
