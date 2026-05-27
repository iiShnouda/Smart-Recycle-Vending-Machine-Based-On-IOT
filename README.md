# ReWinGo

> Touch-screen kiosk software for a combined recycling + vending machine.
> Runs on a Raspberry Pi with a 15.6" portrait LCD; talks to an STM32F411
> over USB-CDC for all motor, sensor, and load-cell hardware.

[![Build status](https://img.shields.io/github/actions/workflow/status/YOUR_USER/rewingo/build.yml?branch=main)](https://github.com/YOUR_USER/rewingo/actions)
[![License: PolyForm NC](https://img.shields.io/badge/license-PolyForm%20Noncommercial%201.0.0-orange.svg)](LICENSE)
[![Qt 6.4+](https://img.shields.io/badge/Qt-6.4%2B-41cd52)](https://www.qt.io/)
[![Platform: Pi 4 / Pi 5](https://img.shields.io/badge/platform-Raspberry%20Pi-c51a4a)](https://www.raspberrypi.com/)

---

## What it does

ReWinGo runs a public-facing kiosk that:

1. **Recycles** — customer drops a bottle/can in the chute. A YOLO model
   on the Pi identifies the material and credits the customer's account.
2. **Vends** — customer browses 8 product slots, taps to buy, the STM32
   rotates the correct auger one full revolution, weight cell confirms
   the item dropped.
3. **Tracks inventory automatically** — each shelf has a load cell. The
   Pi computes counts from weight using one-time per-product calibration,
   so restocks need no admin interaction beyond opening the door.
4. **Logs faults** — failed dispenses, weight mismatches, and stall events
   land in an admin "Faults" page; problem slots auto-disable until
   serviced.

## Hardware

| Component | Role |
|---|---|
| Raspberry Pi 4 / 5 (4 GB+) | Runs the Qt UI, OpenCV YOLO inference, Flask MongoDB backend |
| 15.6" portrait LCD (1080×1920) | Customer touchscreen |
| Pi camera | YOLO input |
| STM32F411 BlackPill (96 MHz) | All real-time hardware control via USB-CDC |
| TMC2209 stepper driver (1×) | Drives 1 of 8 motors via a 74HC595 multiplexer |
| 8× HX711 load cells | Read in parallel via a 74HC165 input shift register |
| INA219, RCWL-0516, reed switch, 5× IR sensors | Power monitor, motion, admin door, IR break-beams |

Full schematic in [`docs/schematic-v2.pdf`](docs/) (drop your file there).

## Install

Two install paths depending on where the kiosk runs:

| Target | Installer | Guide |
|---|---|---|
| **Raspberry Pi 4 / 5** (production) | `rewingo_<version>_arm64.deb` | [`docs/LINUX_INSTALLER.md`](docs/LINUX_INSTALLER.md) |
| **Windows** (development / lab) | `rewingo-setup.exe` (Advanced Installer) | [`docs/WINDOWS_INSTALLER.md`](docs/WINDOWS_INSTALLER.md) |

Grab the latest installer for your platform from the
[Releases page][rel] and run it — both bundle the Qt runtime, OpenCV,
and the Python Flask backend.

The in-app **Admin → About** page tells you what version is installed,
and the **Check for updates** button hits the same Releases endpoint to
see if a newer build is available.

[rel]: https://github.com/YOUR_USER/rewingo/releases

## Project layout

```
.
├── CMakeLists.txt            # Qt build (includes CPack DEB block for Linux)
├── packaging/                # Linux .deb metadata + scripts
│   ├── build_deb.sh          # produces build/rewingo_<v>_arm64.deb
│   ├── setup_pi.sh           # installs build deps on a fresh Pi
│   ├── rewingo.sh            # launcher: /usr/local/bin/rewingo
│   ├── rewingo-backend.sh    # launcher: starts Flask in its venv
│   ├── rewingo.desktop       # XDG menu entry
│   └── debian/{postinst,prerm}
├── packaging/AdvancedInstaller/   # .aip project file for the Windows installer
├── backend/                  # Python Flask MongoDB bridge
├── src/  include/            # C++ application code (UpdateChecker, etc.)
├── qml/  components/         # QML UI (Main, Admin, Registration pages)
├── resources/                # Assets, translations (.qm)
└── docs/                     # Architecture notes + installer guides
```

The companion STM32 firmware lives in a [separate repo](#) — flash it with
STM32CubeIDE or `st-flash`. See `docs/STM32.md` for the firmware overview.

## Development

```bash
# Linux / Pi
cd Recycle_Vending_Machine_LCD
mkdir build && cd build
cmake .. -DUSE_OPENCV=ON -GNinja
ninja
./appRecycle_Vending_Machine_LCD
```

```powershell
# Windows (Qt 6.11 MinGW kit recommended)
cmake -B build -G Ninja
cmake --build build
.\build\appRecycle_Vending_Machine_LCD.exe
```

### Architecture overview

[`docs/TEACHING.md`](docs/TEACHING.md) is a long-form walkthrough of every
module, signal/slot path, and design choice.

Highlights:

- `ApplicationManager` (singleton) owns the `Serial_Connection` worker
  thread, the `Database`, the `InventoryScanner`, and every other service.
  Exposed to QML as `appManager`.
- `Serial_Connection` runs on its own QThread. Line-based protocol with
  per-command ack timeouts + auto-reconnect.
- `ProductsModel` (singleton) drives the 8-slot grid. Per-slot rows
  expose `count`, `lastRaw`, `calibrated` to QML.
- `ProductCatalog` (singleton) is the SKU master list — synced across
  kiosks via MongoDB. New products are added either manually or via
  Open Food Facts lookup (free, no API key).
- `InventoryScanner` polls `WEIGH_ALL` every 3 s, plus on boot, plus on
  reed-close (door closed), plus immediately after every dispense. Logs
  every count change to `restock_events` with a source tag.

### Wire protocol

Line-based ASCII over USB-CDC (115200 8N1).

| Command | Reply | Notes |
|---|---|---|
| `PING` | `PONG` | Health check |
| `STATUS` | `STATUS:uptime=…ms,ir=0xNN,door=open/closed,…` | Diagnostics |
| `DISPENSE:N[:min_drop]` | `DISPENSE:N:START:before=…`<br>`Done DISPENSE:N:…` or `Error DISPENSE:N:STALL:…` | Async — drops 1 item from slot N |
| `WEIGH_ALL` | `Done WEIGH_ALL:r0,r1,…,r7` | 8 raw HX711 readings |
| `SET_TIME:<epoch>` / `GET_TIME` | `SET_TIME:OK:…` / `<epoch>` | RTC sync |

Full list in `STM32/Core/Src/protocol.c`.

## Contributing

Pull requests welcome. See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the
code-style + PR-process basics.

## License

**[PolyForm Noncommercial 1.0.0](LICENSE)** — source-available, non-commercial.

You're welcome to:
- ✅ Read, fork, modify, and run the code for **personal**, **educational**, or **research** purposes
- ✅ Use it in school / university / hackathon projects
- ✅ Share modified copies as long as you keep the copyright notice and this license

You **may not**:
- ❌ Sell this software, build a paid product around it, or use it in any commercial offering
- ❌ Remove the copyright notice or pass the work off as your own
- ❌ Sub-license it to others

For commercial licensing terms, contact the copyright holder via the
GitHub repo issues page.
