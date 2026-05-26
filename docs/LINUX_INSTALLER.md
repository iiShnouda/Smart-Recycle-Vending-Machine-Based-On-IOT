# Building the Linux installer (`.deb`) for Raspberry Pi

This guide turns your source tree into a single `rewingo_<version>_arm64.deb`
that any Pi OS user can install with one `apt install` command. Sister
guide to [`WINDOWS_INSTALLER.md`](WINDOWS_INSTALLER.md) — the two are
totally independent; pick one or build both.

> Tested on **Raspberry Pi OS Bookworm 64-bit**, Pi 4 and Pi 5.
> Build takes ≈15 min on a Pi 4, faster on a Pi 5.

---

## What end users will do

```bash
# Copy the .deb you built onto a fresh Pi (scp, USB stick, whatever):
sudo apt install ./rewingo_0.1.0_arm64.deb
```

That's it. `apt` pulls every Qt 6, OpenCV, and Python dependency
automatically, our `postinst` script creates the Python venv, the data
directory, and the editable `/etc/rewingo/.env`. The kiosk shows up in
the Start menu and on `$PATH` as `rewingo`.

The build steps below are what **you** do to produce the `.deb`.

---

## 1. One-time setup on the build Pi

You only need to run this on the machine where you're producing the
`.deb` — not on every kiosk that consumes it.

```bash
ssh pi@<build-pi-ip>
git clone https://github.com/YOUR_USER/rewingo.git
cd rewingo
sudo bash packaging/setup_pi.sh
```

`setup_pi.sh` installs the build toolchain (CMake, Ninja, Qt 6 dev
packages, OpenCV, Python venv). Idempotent — safe to re-run.

---

## 2. Build the `.deb`

```bash
bash packaging/build_deb.sh
```

What that does, step-by-step:

1. Re-applies `+x` on the maintainer scripts (git on Windows strips
   them, so a fresh clone from Windows would otherwise produce a `.deb`
   with non-executable scripts).
2. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DUSE_OPENCV=ON`
3. `cmake --build build -j` (parallel compile)
4. `cmake --build build --target package` (CPack reads the DEB block in
   `CMakeLists.txt`, stages the install tree, runs `dpkg-deb -b`)

Output:

```
build/rewingo_0.1.0_arm64.deb     ~80 MB
```

The file lists are correct because the install rules live in
`CMakeLists.txt` itself (`install(TARGETS …)`, `install(DIRECTORY
backend/ …)`, etc.) — there's no separate manifest to keep in sync.

---

## 3. Test the `.deb` on a clean Pi

Copy it to a fresh Pi (or a snapshot of one), then:

```bash
sudo apt install ./rewingo_0.1.0_arm64.deb
```

`apt` resolves every runtime dep listed in `CPACK_DEBIAN_PACKAGE_DEPENDS`
(libqt6core6, libopencv-dev, python3-venv, qml6-module-qtmultimedia, …)
and pulls them in automatically.

`postinst` then runs and prints what it does:

```
rewingo: post-install setup…
rewingo: copied .env.example to /etc/rewingo/.env (edit before running)
rewingo: creating Python virtualenv at /opt/rewingo/backend/venv …
rewingo: installing backend Python deps …
rewingo: install complete.

  Edit MongoDB creds:  /etc/rewingo
  Start the UI:         rewingo
  Start the backend:    rewingo-backend
```

Fill in `/etc/rewingo/.env` with the real MongoDB URI + API key, then
run the two binaries from any terminal (or click the menu icon):

```bash
rewingo            # UI
rewingo-backend    # Flask, in a second terminal
```

---

## 4. What gets installed where

| Path | Contents | Survives `apt remove` ? |
|---|---|---|
| `/opt/rewingo/bin/rewingo` | Qt binary | no |
| `/opt/rewingo/backend/` | `server.py`, `requirements.txt`, `.env.example` | no |
| `/opt/rewingo/backend/venv/` | Python virtualenv (created by `postinst`) | no |
| `/usr/local/bin/rewingo` | shell launcher (sets `QT_IM_MODULE`) | no |
| `/usr/local/bin/rewingo-backend` | activates venv, runs Flask | no |
| `/usr/share/applications/rewingo.desktop` | XDG menu entry | no |
| `/usr/share/icons/hicolor/256x256/apps/rewingo.png` | menu icon | no |
| `/etc/rewingo/.env` | **editable** credentials (postinst preserves on upgrade) | **yes** |
| `~/.local/share/ReWinGo/ReWinGoKiosk/` | SQLite DB, logs, cached images | **yes** |

`apt purge rewingo` wipes `/etc/rewingo/`. User data in `~/.local/share/`
is owned by the user and stays put even after purge — `rm -rf` it
manually if you want a truly clean state.

---

## 5. Bumping the version

```bash
# 1. Edit the version line in CMakeLists.txt:
#       project(... VERSION 0.1.0 → 0.2.0 LANGUAGES CXX)
# 2. Build the .deb:
bash packaging/build_deb.sh
# 3. Tag + push:
git tag v0.2.0
git push origin main v0.2.0
# 4. Create a GitHub release and attach build/rewingo_0.2.0_arm64.deb
```

Every kiosk in the field that runs the **Admin → About → Check for
updates** flow will see the new tag and surface the banner on
`AdminMainPage`. The admin downloads the new `.deb` and re-runs
`apt install ./rewingo_*_arm64.deb`. apt knows it's an upgrade because
the package name matches — `prerm` stops the running backend, `postinst`
tops up the Python venv if `requirements.txt` changed, the user's
credentials and data are untouched.

---

## 6. Distributing the `.deb`

A few options, ranked by user friction:

1. **GitHub Release attachment** *(easiest)*. Drop the `.deb` into the
   Releases page → users `wget` / scp it onto their Pi → `apt install`.
2. **Public apt repo** *(nicer)*. Host the `.deb` in an apt-compatible
   layout on any HTTP server. End users add a single line to
   `/etc/apt/sources.list.d/rewingo.list` and then `apt install
   rewingo` like any official package. Tools like
   [`apt-cacher-ng`](https://www.unix-ag.uni-kl.de/~bloch/acng/) or
   [`reprepro`](https://salsa.debian.org/debian/reprepro) make this
   pretty easy.
3. **PPA on Launchpad** *(if you want to look very official)*. Free,
   integrated into `apt-add-repository`. Requires signing the package
   with a GPG key and conforming to Debian Policy.

Pick (1) until you're shipping enough kiosks that the manual download
becomes annoying.

---

## 7. Troubleshooting

| Symptom | Fix |
|---|---|
| `apt install ./rewingo_*.deb` says "depends on … which is not installed" | `sudo apt update` first; outdated package index. |
| Build fails on `qt_standard_project_setup REQUIRES 6.11` | Pi OS Bookworm ships Qt 6.4. The require line is a soft check — re-run `cmake` and the build proceeds. If it doesn't, edit `CMakeLists.txt` line 22 to `REQUIRES 6.4`. |
| Backend crashes with `MongoServerError` | `/etc/rewingo/.env` has wrong/empty credentials. Edit and re-run. |
| Could not load Qt platform plugin "xcb" | You're over SSH without `-X`. Run from the Pi's local desktop, or set `QT_QPA_PLATFORM=eglfs` in `/usr/local/bin/rewingo`. |
| STM32 doesn't respond | `sudo usermod -aG dialout $USER`, log out and back in so the group membership takes effect. |
| `postinst` fails with `pip install`: SSL error | The Pi can't reach pypi. Check network. Re-run install: `sudo dpkg --configure rewingo` re-runs the maintainer scripts. |

---

## 8. (Future) systemd auto-start

When you're ready to make the kiosk launch at boot, drop these two
units into `/etc/systemd/system/`:

```ini
# /etc/systemd/system/rewingo-backend.service
[Unit]
Description=ReWinGo Flask backend
After=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/rewingo-backend
Restart=on-failure
User=pi

[Install]
WantedBy=multi-user.target
```

```ini
# /etc/systemd/system/rewingo.service
[Unit]
Description=ReWinGo kiosk UI
After=graphical.target rewingo-backend.service
Requires=rewingo-backend.service

[Service]
Type=simple
ExecStart=/usr/local/bin/rewingo
Restart=on-failure
User=pi
Environment=DISPLAY=:0

[Install]
WantedBy=graphical.target
```

Then:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now rewingo-backend rewingo
```

Pi reboots → backend + UI both launch automatically.

---

## Summary

```bash
# On the Pi where you build releases:
sudo bash packaging/setup_pi.sh        # one-time
bash      packaging/build_deb.sh        # every release
# → build/rewingo_<version>_arm64.deb

# On every Pi that runs the kiosk:
sudo apt install ./rewingo_<version>_arm64.deb
sudo nano /etc/rewingo/.env
rewingo            # UI
rewingo-backend    # backend
```

That's the whole flow.
