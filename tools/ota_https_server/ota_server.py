#!/usr/bin/env python3
"""
OTA HTTPS Server for CC35xx firmware updates.

Uses the SDK's playground certificates (rootCA + serverCert + serverPrivKey)
that match the ca_certificate.h already embedded in the device firmware.

Usage:
    1. Copy this entire 'ota_https_server' folder to the server PC
    2. Place your firmware .bin file(s) in the same directory
    3. Run:  python3 ota_server.py [--port 8443]
    4. On the device UART, enter the server base URL when prompted
       (e.g., https://<server-IP>:8443)

The server auto-classifies .bin files by filename. If a file can't be
identified, you'll be prompted to select the component type. Choices
are saved to ota_config.json so you won't be asked again.

Endpoints:
    GET /api/updates   — JSON manifest of available firmware images
    GET /<filename>    — Standard file download
"""

import argparse
import http.server
import json
import os
import re
import socket
import ssl
import struct
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SERVER_CERT = os.path.join(SCRIPT_DIR, "serverCert.pem")
SERVER_KEY = os.path.join(SCRIPT_DIR, "serverPrivKey.pem")
CONFIG_FILE = "ota_config.json"

# Manifest layout (PSA_FWU_GPESlot_t):
#   Manifest_Integrity: 16 bytes (4 x uint32_t)
#   GPE_Header:         20 bytes (magic(4) + load_addr(4) + hdr_size(2)
#                                 + protect_tlv_size(2) + img_size(4) + flags(4))
#   GPE_Version:         8 bytes (iv_major(1) + iv_minor(1) + iv_revision(2)
#                                 + iv_build_num(4))
VERSION_OFFSET = 36  # 16 + 20
VERSION_SIZE = 8     # major(1) + minor(1) + revision(2) + build(4)
MIN_FILE_SIZE = VERSION_OFFSET + VERSION_SIZE

# Component types: (name, slot1_id, slot2_id)
COMPONENT_TYPES = [
    ("Vendor_Image",  4, 5),
    ("BL2",           0, 1),
    ("Wireless_FW",   2, 3),
]

# Auto-classification: filename pattern -> component type index
COMPONENT_PATTERNS = [
    (re.compile(r"vendor.?image|vi_",                  re.IGNORECASE), 0),
    (re.compile(r"bl2|tbl|bootloader",                 re.IGNORECASE), 1),
    (re.compile(r"wsoc|rftool|wireless_fw|wireless",   re.IGNORECASE), 2),
]


def load_config():
    """Load saved component mappings from ota_config.json."""
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r") as f:
                return json.load(f)
        except Exception:
            pass
    return {}


def save_config(config):
    """Save component mappings to ota_config.json."""
    with open(CONFIG_FILE, "w") as f:
        json.dump(config, f, indent=2)


def classify_component(filename, config):
    """Classify a .bin file into a component type.

    Priority:
      1. Saved config (ota_config.json)
      2. Auto-classification by filename pattern
      3. Interactive prompt (saves choice to config)
    """
    # Check saved config first
    if filename in config:
        idx = config[filename]
        if 0 <= idx < len(COMPONENT_TYPES):
            name, s1, s2 = COMPONENT_TYPES[idx]
            return name, s1, s2

    # Try auto-classification
    for pattern, idx in COMPONENT_PATTERNS:
        if pattern.search(filename):
            config[filename] = idx
            save_config(config)
            name, s1, s2 = COMPONENT_TYPES[idx]
            print(f"[*] Auto-classified '{filename}' as {name}")
            return name, s1, s2

    # Interactive prompt
    print(f"\n[?] Unknown component type for '{filename}'. Select:")
    for i, (name, s1, s2) in enumerate(COMPONENT_TYPES):
        print(f"    [{i + 1}] {name}  (slots {s1}/{s2})")

    while True:
        try:
            choice = int(input("    Choice: ")) - 1
            if 0 <= choice < len(COMPONENT_TYPES):
                config[filename] = choice
                save_config(config)
                name, s1, s2 = COMPONENT_TYPES[choice]
                print(f"[*] Saved '{filename}' as {name}\n")
                return name, s1, s2
        except (ValueError, EOFError):
            pass
        print("    Invalid choice, try again")


def extract_version(filepath):
    """Extract version from the binary manifest header (PSA_FWU_GPESlot_t layout).

    Returns (major, minor, patch, build) or None on failure.
    """
    try:
        with open(filepath, "rb") as f:
            f.seek(VERSION_OFFSET)
            data = f.read(VERSION_SIZE)
            if len(data) < VERSION_SIZE:
                return None
            # iv_major(uint8), iv_minor(uint8), iv_revision(uint16 LE),
            # iv_build_num(uint32 LE)
            major, minor, revision, build = struct.unpack("<BBHi", data)
            return major, minor, revision, build
    except Exception:
        return None


# Global config loaded at startup, updated as files are classified
g_config = {}


def build_updates_list():
    """Scan the current directory for .bin files and build the update manifest."""
    global g_config
    updates = []
    bin_files = sorted(f for f in os.listdir(".") if f.endswith(".bin"))
    for bf in bin_files:
        size = os.path.getsize(bf)
        if size < MIN_FILE_SIZE:
            continue
        component, slot1, slot2 = classify_component(bf, g_config)
        ver = extract_version(bf)
        version_str = f"{ver[0]}.{ver[1]}.{ver[2]}.{ver[3]}" if ver else "0.0.0.0"
        updates.append({
            "file": bf,
            "component": component,
            "slot1_id": slot1,
            "slot2_id": slot2,
            "version": version_str,
            "size": size,
            "url": f"/{bf}",
        })
    return updates


class OTARequestHandler(http.server.SimpleHTTPRequestHandler):
    """HTTP handler with /api/updates endpoint; all other paths serve files."""

    def do_GET(self):
        if self.path == "/api/updates":
            updates = build_updates_list()
            body = json.dumps({"updates": updates}).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            # Fall through to standard file serving
            super().do_GET()


def get_all_ips():
    """Get all IPv4 addresses on this machine."""
    ips = []
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            ip = info[4][0]
            if ip not in ips and ip != "127.0.0.1":
                ips.append(ip)
    except Exception:
        pass
    if not ips:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ips.append(s.getsockname()[0])
            s.close()
        except Exception:
            ips.append("127.0.0.1")
    return ips


def run_server(port, serve_dir):
    """Run HTTPS server."""
    global g_config

    # Verify certificate files exist
    if not os.path.exists(SERVER_CERT):
        print(f"[!] ERROR: Server certificate not found: {SERVER_CERT}")
        print(f"    Make sure serverCert.pem is in the same directory as this script")
        sys.exit(1)
    if not os.path.exists(SERVER_KEY):
        print(f"[!] ERROR: Server private key not found: {SERVER_KEY}")
        print(f"    Make sure serverPrivKey.pem is in the same directory as this script")
        sys.exit(1)

    os.chdir(serve_dir)
    g_config = load_config()

    # Classify all .bin files at startup (prompts user if needed)
    bin_files = sorted(f for f in os.listdir(".") if f.endswith(".bin"))
    if bin_files:
        print(f"\n[*] Classifying firmware files...")
        for bf in bin_files:
            if os.path.getsize(bf) < MIN_FILE_SIZE:
                continue
            classify_component(bf, g_config)

        # Show summary
        print(f"\n[*] Firmware files available for download:")
        ips = get_all_ips()
        for bf in bin_files:
            if os.path.getsize(bf) < MIN_FILE_SIZE:
                continue
            name, _, _ = classify_component(bf, g_config)
            ver = extract_version(bf)
            ver_str = f"{ver[0]}.{ver[1]}.{ver[2]}.{ver[3]}" if ver else "0.0.0.0"
            size = os.path.getsize(bf)
            print(f"    {bf}  [{name}]  v{ver_str}  ({size:,} bytes)")
    else:
        print(f"\n[!] No .bin files found in {os.path.abspath(serve_dir)}")
        print(f"    Place your firmware .bin file there and restart")
        ips = get_all_ips()

    # Show auto-discovery endpoint
    for ip in ips:
        print(f"\n[*] Auto-discovery endpoint: https://{ip}:{port}/api/updates")

    # Create SSL context — use TLS 1.2 (CC35xx mbedTLS does not support TLS 1.3)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(SERVER_CERT, SERVER_KEY)

    # Create HTTP server with custom handler
    server = http.server.HTTPServer(("0.0.0.0", port), OTARequestHandler)
    server.socket = context.wrap_socket(server.socket, server_side=True)

    print(f"\n[*] HTTPS server listening on port {port}")
    print(f"    Serving files from: {os.path.abspath(serve_dir)}")
    for ip in ips:
        print(f"    https://{ip}:{port}/")
    print(f"\n    Press Ctrl+C to stop\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[*] Server stopped")
        server.server_close()


def main():
    parser = argparse.ArgumentParser(
        description="OTA HTTPS Server for CC35xx (uses SDK playground certificates)")
    parser.add_argument("--port", type=int, default=8443,
                        help="HTTPS port (default: 8443)")
    parser.add_argument("--dir", type=str, default=".",
                        help="Directory to serve files from (default: current dir)")
    args = parser.parse_args()

    print("[*] CC35xx OTA HTTPS Server")
    print(f"[*] Using certificates: {SERVER_CERT}")
    run_server(args.port, args.dir)


if __name__ == "__main__":
    main()
