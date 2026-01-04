# SimpleLink™ WIFI SDK

This repository redistributes TI's [SimpleLink WIFI Software Development
Kit](https://www.ti.com/tool/download/SIMPLELINK-WIFI-SDK) (WIFI SDK), a
comprehensive software package for developing 2.4 GHz applications.

The WIFI SDK delivers components that enable engineers to develop applications
on the Texas Instruments CC35xx Wi-Fi and BLE combo MCU devices. 

This is version 9.21.00.15 of the SimpleLink Wi-Fi SDK.

## What's New

  - Wi-Fi
      - IEEE802.11 a,b,g,n,ac,ax
      - 2.4GHz & 5GHz bands
      - Supported roles: Station role, AP role (up to 4 connections,
        Wi-Fi4 only) , MultiRole (STA-AP), P2P (Client and Go)
      - Support Security types
          - Personal - WPA3 (STA role only), WPA2/WPA3 (Transition
            mode), WPA2 Personal, WPA/WPA2 (mixed mode, STA role only),
            WPS, OPEN
          - Enterprise - WPA3, WPA2 (TLS,TTLS, PEAP0)
      - Supported Profiles and connection policies
      - Support OTA lib
      - Date and time module
      - Agile Multiband
      - DFS support and channel switch
      - Option to disable WI-Fi6 via eFuse / INI
  - BLE
      - Implementing BLE 5.4 specification
      - Roles: Broadcaster, Peripheral, Observer, Central, Multirole
      - Legacy & Extended Advertisement
      - Legacy & Extended Scan
      - Multiple BLE Connections (up to 16)
      - Supported PHYs: 1M, 2M, Coded
      - Privacy
      - Secured connection
      - BLE store bond information support
  - Support Drivers - I2C Controller, DMA, GPIO, GPTIMER, PWM, SPI,
    UART2, SHA2, AES, POWER, ADC, Watchdog, SDIO, I2S, SDMMC, TRNG, ECC
    (for ecdsa/ecdh/eddsa)
  - SDK content
      - FreeRTOS
      - WPA Supplicant
      - lwIP
      - NimBLE
      - Driver examples
      - MbedTLS
      - Support OTA lib
      - CC35xx network terminal example
      - CC35xx BLE Wi-Fi provisioning example
      - CC35xx Mqtt client example
      - CC35xx AT-Commands example
      - CC35XX Indigo example
      - NVOCMP package
      - Dynamic support for different flash types
          - IS25WJ032F
          - IS25WJ064F
          - W25Q64JW
          - W25Q32JW
          - PY25Q32LB
          - PY25Q64LB
          - PY25Q128LA
          - PY25Q256LC
          - GD25LF32ETJGY
          - GD25LF64ETJGY
          - MX25U3235F\_EXPERIMENTAL
          - MX25U6435F\_EXPERIMENTAL
          - IS25WJ128F\_EXPERIMENTAL
      - Power management
      - PSA APIs for selected Crypto drivers
      - PSRAM support
          - APS6404L-SQH - 8MB - Stacked
## About this Repository

Although TI also offers this SDK via [a classic
installer](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK), we made
it available as a Git repository to cater to various use cases:

* **Smaller download size**: By omitting documentation (in the **docs/**
  directory) and examples (in the **examples/** directory), the size of the SDK
  can be reduced.
  * The documentation is [available online][sdk docs] in the TI Developers Zone;
    just be sure to browse the appropriate version.
  * Examples are provided in separate repositories, as noted below.
* **CI/CD tooling integration**: Using Git instead of installers can ease
  integration with CI/CD tooling pipelines.
* **Increased visibility and change tracking**: Git improves the ability to
  identify changes that may affect (or not!) your application.
* **Simplified workflow for rebasing**: Git makes it simpler for you to rebase
  your changes to new releases.

One temporary, notable difference in this WIFI 9.21.00.15 SDK GitHub
distribution when compared to the classic installer above, is the exclusion of
the **tools/ble/ble_agent/ble_toolbox/** directory. This has been temporarily
done as the ble_toolbox executable currently exceeds GitHub's file size limit.
Users can obtain the ble_toolbox executable using the classic installer. This
GitHub-specific difference will be resolved in the near future.

## Example Repos
* [Core SDK, including
  drivers](https://github.com/TexasInstruments/simplelink-coresdk_wifi-examples)
* [WI-FI Examples](https://github.com/TexasInstruments/simplelink-wifi-examples)

## Setup Instructions

### Edit **imports.mak**

At the base of the SDK, you will find **imports.mak**; this file is used by the
build system to find your installed dependencies. Please update **imports.mak**
with full paths to where you have these dependencies installed on your system.

Notably for Windows users, the _Windows_ variant of some tools may be required.
For example, the `SYSCONFIG_TOOL` will need to have a **.bat** extension.

Refer to the comments in **imports.mak** for details on setting variables, and
the [release notes][sdk release notes] for recommended versions. Also see the
[Resources](#resources) section below for download locations.

### Build SDK Libraries

With a few exceptions (libraries without sources), the SDK is provided without
prebuilt libraries. To build the SDK libraries (after editing **imports.mak** as
described above!), navigate to the root of the SDK (the same directory where
**imports.mak** resides) and run GNU `make`.

Assuming it is on your path, you can simply:

```bash
make
```

To clean/rebuild:

```bash
make clean
make
```

At this point, the libraries will be built and installed into the appropriate
locations throughout the SDK.

## Resources

Dependency download locations:

* [SysConfig (SYSCONFIG_TOOL)][sysconfig download]
  * This is also included with CCS
* [TI CLANG Compiler (TICLANG_ARMCOMPILER)][ticlang download]
* [ARM Gnu Toolchain (GCC_ARMCOMPILER)][gcc download]
* [CMake](https://cmake.org/)
* [GNU make](https://www.gnu.org/software/make/)
  * This is also included with CCS in the **ccs/utils/bin/** directory (and
    called `gmake`).

## Technical Support

Please consider creating a post on [TI's E2E forum](https://e2e.ti.com).

[sdk release notes]: https://software-dl.ti.com/simplelink/esd/simplelink_wifi_sdk/9.21.00.15/exports/release_notes_simplelink_wifi_sdk_9_21_00_15.html
[sdk docs]: https://software-dl.ti.com/simplelink/esd/simplelink_wifi_sdk/9.21.00.15/exports/docs/Documentation_Overview.html
[sysconfig download]: https://www.ti.com/tool/SYSCONFIG
[ticlang download]: https://www.ti.com/tool/download/ARM-CGT-CLANG
[gcc download]: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
