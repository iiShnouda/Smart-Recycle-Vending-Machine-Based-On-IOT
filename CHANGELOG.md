# Changelog

All notable changes to ReWinGo are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
