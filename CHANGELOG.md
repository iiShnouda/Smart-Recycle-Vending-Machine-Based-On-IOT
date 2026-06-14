# Changelog

All notable changes to ReWinGo are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.13.0] — 2026-06-14

### Added
- **Full cloud DB schema (multi-machine).** `DATABASE_SCHEMA.md` documents
  the complete MongoDB design — global collections (`users`, `product_catalog`,
  `machines`, `config`) and per-machine ones (`products`, `transactions`,
  `inventory`, `dispense_faults`), where "total across all machines" is just the
  same query without a `machineId` filter. Covers everything: full user record
  (name/email/mobile/password-hash/role/points/face/recycle counts/stats),
  product record (name/image/points/load-cell weight + calibration), the global
  product category, and per-/cross-machine transactions.
- **Point value in EGP (dev-only, admin read-only).** New `config` singleton holds
  `pointValueEGP`; `schema.js` seeds it and never overwrites it, so only the
  developer changes the EGP value. `GET /config` lets the admin panel *show*
  "1 point = X EGP" without being able to change it. Admins still change
  *points* (`products.pricePoints`, `config.recycleRewards`).
- **Backend now persists the full model.** `iot/kiosk-backend` ensures indexes,
  enriches each transaction with the frozen `egpValue` and rolls the totals into
  the `users` (points, recycle counts, stats) and `machines` (per-machine stats)
  docs; `status` updates the `machines` registry; new `GET /machines` returns the
  registry **plus a grand total** across all machines.

### Changed
- **Kiosk runs windowed (with frame) for now** — fullscreen disabled in `Main.qml`.

### Removed
- Dead `face_rec/scripts/machine_link_sidecar.py` (the old WebSocket relay,
  superseded by the MQTT `MachineLink`). No orphan QML/C++ remained otherwise.

## [0.12.0] — 2026-06-14

### Added
- **Connect-the-app account claim (optional, no password).** After registration,
  a new optional **"Connect the mobile app"** button on the complete page opens
  `ClaimQrPage`, which mints a one-time token, publishes the pending account to
  the backend (`rewingo/<machineId>/register` over MQTT), and shows a
  **`REWINGO-CLAIM:<token>`** QR. The phone app (logged in) scans it and the
  backend **auto-links** the kiosk account to that app user — they then set a
  password + email in the app. Skipping ("Maybe later") always finishes
  registration; it's never required.
  - Kiosk: `MachineLink.beginClaim(name, phone)` + `claimToken`.
  - Backend (`iot/kiosk-backend`): subscribes `rewingo/+/register` → upserts a
    `pending_accounts` doc; new **`POST /claim { token, appUser:{id,email} }`**
    finds the pending account, links it into `accounts`, marks it claimed.
  - *App side (your part):* scan `REWINGO-CLAIM:<token>` → `POST /claim`.

### Fixed
- **Registration details page was missing from the build.** `RegistrationDetailsPage.qml`
  (added in 0.9.0) was never listed in the QML module, so the build silently
  excluded it and "I agree" would have failed to open the name/mobile page at
  runtime. Registered it (and `ClaimQrPage.qml`) in CMake.

## [0.11.0] — 2026-06-14

### Changed
- **Renamed `SleepMode` → `StartPage`.** The attract/"press to start" screen is
  now `qml/StartPage.qml` (objectName `startPage`), and every reference is wired
  through: `Main.qml` (`startPageComponent` as the StackView's initialItem), the
  CMake QML module list, and the comments in `QrLoginPage`, `VendingPage`, and
  `IdleManager`. No behaviour change — clearer naming.

## [0.10.0] — 2026-06-14

### Fixed
- **QR sign-in timeout returns to the START screen.** When the 1-minute QR timer
  expired it only popped one page back; now it pops all the way to the start
  screen (the stack root), matching the idle-timeout behaviour.
- **Update button — bulletproof install + clear feedback.** The `.deb` now always
  downloads to `/tmp` (the only path the sudoers rule permits `dpkg -i` on, so a
  per-user temp dir can't cause a silent sudo denial). If the helper script is
  ever missing or won't launch, `UpdateChecker` now runs the proven install
  sequence **inline** via a detached shell as a fallback, and logs every step
  (helper launch OK/FAILED + fallback). The Check / Install / "Install now"
  buttons now animate on press so a tap is unmistakably registered.

### Added
- **"Back to Main Menu" on both receipts.** After recycling (`RecycleSummaryPage`)
  or vending (`VendingReceiptPage`), a new button returns to the user's Main page
  **without logging out**, so they can start another action; "Finish"/"Done"
  still go all the way home.

### Changed
- **Press animations across admin actions.** Every refresh / rescan / run / clear
  button in the admin panel (Analytics, Logs, Products, Inventory, Faults,
  Diagnostics) now scales + darkens on press, and the circular ↻ refresh icons
  spin on tap — clear "I pressed it" feedback.
- **Bigger QR-screen text.** The "Connecting… / Waiting to scan" status, the
  "scan this code" subtitle, and the privacy notice are all enlarged for
  readability across the room.

## [0.9.0] — 2026-06-13

### Added
- **Registration now asks for your name + mobile number.** A new
  `RegistrationDetailsPage` (on-screen keyboard) collects the name and mobile
  before the face scan, so login greets the real person ("Welcome <name>")
  instead of "New User", and the mobile number is stored with the face record
  (new `phone` column in `faces.db`) to key the phone-app account later. The
  name+mobile is handed to the enroller as one `name<TAB>mobile` stdin line, so
  the C++ `FaceRec.enroll()` signature is unchanged.

### Notes
- The face-rec **speed + voice** work (smooth preview, fast early-exit login,
  piper voice prompts) ships in the Python sidecars under `/opt/face_rec` and is
  already live on the kiosk — it isn't part of this `.deb`.

## [0.8.0] — 2026-06-13

### Face registration — rebuilt to actually work on the kiosk
- **Self-capture enrollment.** Registration used QtMultimedia (`Camera` +
  `feedFrame`) to show the face and capture frames — the exact path that stalls
  on the Pi, so the page showed nothing and never captured ("can't register").
  And it stored embeddings in the C++ DB, which **login never reads**, so even a
  successful enroll wouldn't be recognised. Registration now runs the new
  **`scripts.enroll_selfcam`** sidecar, which opens the camera itself (V4L2 +
  warm-up), **auto-advances 3 head poses with NO screen press**, writes the live
  preview, and stores the averaged ArcFace embedding in the **same `faces.db`
  that login reads** — so a newly registered face is recognised next visit.
  Verified end-to-end on the Pi (FRONT→LEFT→RIGHT → `enrolled`).
- **Round preview + outside ring + N/3.** `FaceEnrollPage` no longer uses
  QtMultimedia. The face shows in a **true circle** (a Canvas paints the page
  background over the square frame's corners — the old `radius + clip` only
  clipped to a square), the **progress ring sits outside** the face circle, and
  the counter reads **"N / 3"** (was "/ 5"). `FaceRec` gained `enrollCount` /
  `enrollTotal` + parses the sidecar's `stage`/`progress`/`enrolled` events.

## [0.7.0] — 2026-06-13

### Fixed
- **The update button now reliably installs and restarts.** The self-update
  helper (`rewingo-update-helper`) was aborting on a non-zero `dpkg` exit caused
  by an *unrelated* pending `initramfs-tools` trigger on this Pi (it can't write
  `/boot`) — our package installed fine, but `set -e` killed the script before
  it relaunched the kiosk, so the screen went black and the update "didn't
  work". The helper now: (1) drops `set -e`, (2) judges success by **reading the
  installed version back** from the package DB rather than trusting dpkg's exit
  code, (3) installs **before** killing the old process (replacing the on-disk
  binary while it runs is safe) and lets the autostart loop relaunch the new
  one, with a direct-launch fallback. `UpdateChecker` no longer self-quits 500 ms
  after spawning the helper — that race let the autostart loop relaunch the OLD
  binary mid-install. Proven end-to-end on the Pi (`RESULT:OK`).
- **QR sign-in: the kiosk no longer shows a stale QR.** `MachineLink` deletes the
  previous session's QR PNG *before* fetching the new one and retries the fetch,
  so a network blip can never leave an old code (with an old token) on screen —
  which the phone would echo back and the kiosk would silently reject. Every
  received login is now logged with the session/token state, so a rejected
  sign-in is diagnosable from the kiosk log instead of failing silently.

### Mobile app (separate Flutter project)
- **QR scanner no longer "approves" arbitrary codes.** It rejected nothing —
  any QR (Wi-Fi, URL, contact card) fell through to the session-started screen.
  Now only `REWINGO:<machineId>:<token>` codes are accepted; anything else keeps
  the camera running with a hint to scan the code on the kiosk.
- **Sign-in publish hardened.** The HiveMQ TLS publish now pins the system CA
  store (`SecurityContext.defaultContext`) so the handshake doesn't fail
  silently on some Android builds, with an 8 s connect timeout so it fails fast
  instead of hanging — the cause of "scanned the machine code but nothing
  happened on the LCD".

### Face recognition
- **Reliable camera open + live preview.** With the Logitech C270 reconnected,
  the self-capture sidecar now opens it via the **V4L2 backend** (the default
  backend sometimes selected GStreamer and delivered zero frames), **warms the
  camera up** for up to 2 s (the first UVC grabs after open fail while it powers
  on — the real cause of the `frames:0` stalls), and drops the Picamera2 path
  (that's for CSI cameras and isn't installed). Verified on the Pi: 69 frames
  grabbed in an 8 s window.
- **Live preview actually renders now.** The preview frame was written with
  `cv2.imwrite("…​.tmp", …)`, but OpenCV picks its codec from the file extension
  and silently failed on `.tmp`, so `FaceDetectionPage` never showed the camera.
  It now JPEG-encodes in memory and atomically renames a real image into place.
- The sidecar reports `grabbed` (raw frames) alongside `frames` (faces seen) and
  emits an explicit **`camera delivered no frames`** error, so a dead/unplugged
  camera is distinguishable from "face not recognised" instead of silently
  routing to registration.

  Note: if face login ever sits on the scan screen again, first check `lsusb`
  shows the `Logitech … C270` and that `/dev/video0` exists — a loose USB cable
  removes the camera entirely (there is no `/dev/video0` without it).

## [0.6.0] — 2026-06-12

### Changed
- **QR sign-in is now single-use (Discord-style).** Each time the QR screen
  opens, the kiosk mints a fresh random token (`REWINGO:<machineId>:<token>`)
  and only accepts a login that echoes *that* session's token, so an old or
  photographed QR can't sign anyone in. The QR page also **times out after
  1 minute**.
- **Admin → Diagnostics** realigned to the STM32 command set.

### Fixed
- **STM32 connection-log spam.** `Serial_Connection` logged "No matching
  device, retrying…" on every reconnect tick while no STM32 was attached;
  now it's said once and re-armed when the link connects.

## [0.5.0] — 2026-06-11

### Added
- **HiveMQ Cloud IoT + fixed-QR sign-in.** Picking "QR Code" on the auth
  screen now shows a **fixed machine QR** (`REWINGO:<kioskId>`) for the phone
  app to scan — previously that path showed a note and dropped you on MainPage
  with no QR at all. `MachineLink` was rewired off the WebSocket sidecar onto
  the shared **HiveMQ MQTT** client: it listens on `rewingo/<kioskId>/login`
  and signs the user in when the backend publishes them after a scan.
- **`iot/kiosk-backend/`** — a dedicated Node.js LCD backend (separate from the
  mobile-app backend). Bridges **HiveMQ ↔ MongoDB Atlas** (telemetry → Mongo)
  and exposes `POST /link` to relay a scanned QR to `rewingo/<machineId>/login`.
- `MqttClient` now subscribes to `<base>/login` as well as `<base>/cmd`.
- Cross-platform broker config: the kiosk reads `.env` from `C:\rewingo\.env`
  (or `%APPDATA%`) on Windows, `/etc/rewingo/.env` on the Pi.

### Changed
- **About → updates: Check → Install swap.** The "Check for updates" button now
  hides the moment an update is found, leaving just the green "Download &
  install vX.Y.Z" button — no more two-button confusion. Last-checked time
  unchanged.

## [0.4.0] — 2026-06-09

### Added
- **QR sign-in (Discord-style).** On the registration consent page, "Use QR"
  now opens a real scannable QR (`QrLoginPage`). A Python sidecar
  (`machine_link_sidecar.py`) holds a WebSocket to the backend's IoT endpoint
  `wss://…/iot?type=machine&machineId=<kiosk id>` — using the OS resolver, which
  works where Qt's network can't — and relays the user the phone app links. New
  `MachineLink` C++ singleton; the QR PNG is fetched via curl. The kiosk shows
  `REWINGO:<machineId>:<token>`; the phone app POSTs that to the backend's
  `POST /api/machines/link`, which `sendToMachine()`s the user back to this
  kiosk. machineId == the kiosk's id.

## [0.3.2] — 2026-06-07

### Added
- **Live camera preview is back on the face-login screen.** The self-capturing
  recognition sidecar now also writes each frame to `/tmp/rewingo_face.jpg`
  (atomic rename), and `FaceDetectionPage` shows it live — so you see yourself
  while the *separate* recognition process runs in parallel. No QtMultimedia,
  so none of the frame-stall problems. Recognition window bumped to 8 s.

## [0.3.1] — 2026-06-07

### Fixed
- **The in-app update button finally works on this network.** Root cause: on
  some kiosk networks (notably phone hotspots) **Qt's network stack can't
  resolve `api.github.com`** while the system resolver `curl` uses works fine
  (which is why `~/update.sh` always worked). So the check found nothing and
  the install never ran. The updater now **shells out to `curl`** for both the
  version check and the `.deb` download, so it works wherever the OS network
  does. Kept the 25 s watchdog + a 20 s curl timeout so it can't wedge.

## [0.3.0] — 2026-06-07

### Fixed / Changed
- **Face login now recognises people on the Pi.** The kiosk used to pipe
  QtMultimedia camera frames to the recogniser over stdin, but that frame
  delivery stalls on the Pi — the Python sidecar blocked forever on its stdin
  read, so face login *and* the "not recognised → registration" route never
  fired (you'd sit on the scan screen). The Pi sidecar now **opens the camera
  itself** (`scripts.sidecar_identify_selfcam`, Logitech cam) and emits the
  same JSON events; `FaceDetectionPage` no longer pipes frames and shows a
  scanning animation instead of a live mirror preview.
- **Safety-net timeout** on the face screen: if no result arrives in 12 s, it
  routes to the registration consent flow instead of hanging.
- Versioned the self-capture sidecar in the repo at
  `face_rec/scripts/sidecar_identify_selfcam.py` (deploy to
  `/opt/face_rec/scripts/` on the Pi).

## [0.2.9] — 2026-06-06

### Fixed
- **Self-update no longer wedges the "Check for updates" button.** The update
  check had no network timeout, so a hung DNS lookup (this Pi stalls on
  boot-time name resolution) left the in-flight flag stuck `true` forever —
  every subsequent tap early-returned and the About page froze on "Up to date".
  Added a 20 s transfer timeout on the request **plus** a 25 s watchdog that
  aborts and retries a wedged check, so the updater always recovers instead of
  bricking until reboot.

## [0.2.8] — 2026-06-06

### Added
- **App icon is now the ReWinGo coin** (`rwg-coin.png`, 256×256) instead of the
  generic vending graphic.
- **`iot/` — MQTT → MongoDB bridge (`rewingo-bridge`).** Completes the IoT
  pipe: the kiosk already publishes every DB write to Mosquitto; this Python
  service subscribes (`rewingo/#`) and persists each message to MongoDB Atlas.
  Topic-to-collection routing for `transactions`, `dispense_faults`,
  `inventory`, `products` (upsert), `product_catalog` (upsert), and kiosk
  presence (`status`/`heartbeat` → `kiosks`). Ships with a systemd unit and a
  one-shot `install.sh`. Reads the shared `/etc/rewingo/.env` (new keys:
  `MONGODB_URI`, `MONGODB_DB`).

### Fixed
- **In-app self-update now completes the install.** On 0.2.6 the
  `downloadAndInstall` path could return silently when the release asset URL
  hadn't been captured, so `rewingo-update-helper` never ran (its log stayed
  empty — confirmed on the Pi). The fallback asset URL + entry logging make the
  **Download & install** button reliably hand the `.deb` to `dpkg -i` and relaunch.

### Changed / Removed
- **Firmware: INA219 removed** to match the 2026-06-06 controller board rev.
  Its I²C lines no longer route to the MCU, so the `ina219` driver and the
  `POWER` command are gone and `I2C1` (PB6/PB7) is free. Every other MCU pin is
  unchanged from the previous revision.

## [0.2.0] — 2026-05-27

### Added
- `systemd` unit at `/etc/systemd/system/rewingo-backend.service` —
  Flask backend boots at startup and auto-restarts on crash. No more
  "open a second terminal" friction.
- In-app self-update flow on Admin → About:
  - **Check for updates** button hits the GitHub Releases API.
  - **Download & install vX.Y.Z** button fetches the .deb asset,
    spawns `/usr/local/bin/rewingo-update-helper`, and exits cleanly
    so dpkg can replace the running binary.
- Visible red **ADMIN** button on MainPage as a manual entry path
  while the reed switch isn't connected to the STM32 yet.
- Reed-switch handling moved from Pi GPIO to STM32 over USB-CDC:
  firmware now pushes unsolicited `DOOR:OPEN` / `DOOR:CLOSED` events.
- `docs/DATABASE.md` — Atlas setup walkthrough.
- `docs/RELEASING.md` — how to cut a tagged release with the .deb attached.
- `/etc/sudoers.d/rewingo` — scoped-down passwordless `dpkg -i` for the
  update helper, validated with `visudo -c` in postinst.

### Fixed
- **All Qt resource (qrc) paths.** `qt_add_qml_module`'s actual prefix is
  `:/Recycle_Vending_Machine_LCD/...` not `:/qt/qml/Recycle_Vending_Machine_LCD/...`.
  Fixing this restored: translations, the admin-page push, every PNG /
  GIF / JPG on MainPage and RecycleWaitingPage, and the asset references
  in `product_image_catalog.cpp`.
- `TranslationManager` also calls `engine.retranslate()` after
  `setUiLanguage()` — required in Qt 6.4 to update already-loaded pages.
- CPack `.deb` was installing files under `/usr/opt/...` and
  `/usr/usr/local/bin/...`. Fixed by setting
  `CPACK_PACKAGING_INSTALL_PREFIX "/"` so relative `DESTINATION`s
  resolve from filesystem root.

### Changed
- **License switched from MIT to PolyForm Noncommercial 1.0.0.**
  Source remains visible to everyone; commercial use is reserved.
- `MongoClient` now points at the local Flask backend (`127.0.0.1:5000`)
  instead of the deprecated Atlas Data API URL. `BACKEND_API_KEY` is
  read from `/etc/rewingo/.env`, shared with the Flask process.
- Window resized from 1080×1920 portrait → no change (still 1080×1920);
  experiments with 1360×768 landscape and 768×1360 were reverted.

## [Unreleased]

### Added
- **In-app update checker.** `UpdateChecker` C++ class hits the GitHub
  Releases API (`/repos/<owner>/<repo>/releases/latest`), exposes
  `currentVersion`, `latestVersion`, `updateAvailable` as Qt properties.
- **Admin → About page**: version, build date, kiosk ID, dependency
  list, license, "Check for updates" button.
- **Admin home banner**: when a newer release is detected, a yellow
  bar at the top of `AdminMainPage` links to the About page.
- Version baked into the binary via `REWINGO_VERSION` compile
  definition pulled from `project(... VERSION X.Y.Z ...)`.
- Windows installer built with **Advanced Installer** — see
  `docs/WINDOWS_INSTALLER.md` for the step-by-step.

### Changed
- Moved `Serial_Connection.h` from `src/` to `include/` for consistency.
- Moved `TEACHING.md` to `docs/`.
- Deleted unused `Resources.qrc` and `FILE_STRUCTURE.md` placeholder.

## [0.1.0] — 2026-05-25 — initial public release

### Added
- Qt 6 / QML kiosk UI: customer flow (recycle + vending), admin panel,
  registration, virtual keyboard.
- 8-slot vending dispense with weight-verified item drop, stall detection
  via TMC2209 StallGuard4, INDEX-pulse step verification.
- Continuous inventory tracking via HX711 bank scanner — counts auto-
  update on every restock without admin interaction.
- Per-slot weight calibration (one-time per product) stored in SQLite.
- Shared product catalog across kiosks via MongoDB.
- Open Food Facts integration for online product lookup (free, no API
  key) — auto-fills name, image, weight.
- Admin pages: Products (with calibration), Inventory (live counts +
  restock history), Faults (failed dispenses, auto-disable + re-enable),
  Diagnostics, Analytics, Logs.
- Bilingual UI (English + Arabic).
- Python Flask backend that bridges the kiosk to MongoDB Atlas.
- STM32F411 firmware: TMC2209 silent stepping, 8-cell HX711 parallel
  read, motor mux, EXTI dispatch, FreeRTOS task layout.
