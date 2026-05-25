# SimpleLink™ WIFI SDK

This repository redistributes TI's [SimpleLink WIFI Software Development
Kit](https://www.ti.com/tool/download/SIMPLELINK-WIFI-SDK) (WIFI SDK), a
comprehensive software package for developing 2.4 GHz applications.

The WIFI SDK delivers components that enable engineers to develop applications
on the Texas Instruments CC35xx Wi-Fi and BLE combo MCU devices. 

This is version 10.10.01.08 of the SimpleLink Wi-Fi SDK.

## What's New

  - New Flash support

  - \[SCCM\] Add support for CBC encryption for protected storage

  - Correct flash enum definition for Auto Detection to match v9.22 SDK

  - Bug Fixes
## About this Repository

Although TI also offers this SDK via [a classic
installer](https://www.ti.com/tool/download/SIMPLELINK-WIFI-SDK), we made
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

One temporary, notable difference in this WIFI 10.10.01.08 SDK GitHub
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

[sdk release notes]: https://dev.ti.com/tirex/explore/node?a=ZlChU-m__10.10.01.08&isTheia=false&node=A__ADMjnimJ4C5BfFnmM3X-jg__com.ti.SIMPLELINK_WIFI_SDK__ZlChU-m__10.10.01.08
[sdk docs]: https://dev.ti.com/tirex/explore/node?a=ZlChU-m__10.10.01.08&isTheia=false&node=A__AHaph7YfvcrVy2cDlmb4sQ__com.ti.SIMPLELINK_WIFI_SDK__ZlChU-m__10.10.01.08
[sysconfig download]: https://www.ti.com/tool/SYSCONFIG
[ticlang download]: https://www.ti.com/tool/download/ARM-CGT-CLANG
[gcc download]: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
