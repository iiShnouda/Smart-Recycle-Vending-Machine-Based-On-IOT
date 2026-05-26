# Building the Windows installer with Advanced Installer

This guide turns your Qt build into a polished `rewingo-setup.exe` that
double-clicks like any other Windows app installer.

> Tested with Advanced Installer 22.x (Professional edition). Free
> edition works for basic MSI; you'll need Professional or higher for
> a single-file `.exe` and updater support.

---

## 0. What you'll end up with

```
ReWinGo-Installer.exe       ← what your users download
   ↓ runs through the standard Windows installer wizard
   ↓ installs to  C:\Program Files\ReWinGo\
      ├── rewingo.exe       ← the Qt app
      ├── Qt6Core.dll, Qt6Quick.dll, …  (windeployqt-bundled)
      ├── qml\, plugins\    (Qt runtime modules)
      ├── opencv_*.dll
      └── backend\          (Python Flask + bundled venv)
   ↓ creates Start Menu + Desktop shortcuts
   ↓ optional: starts the kiosk after install
```

---

## 1. Build the Qt app for Windows

Open **Qt Creator** (or your preferred CMake IDE) with the
**Qt 6.x MinGW 64-bit** kit selected, then:

```powershell
cd E:\Projects\qt\Grad_Project\Recycle_Vending_Machine_LCD
cmake -B build-win -DCMAKE_BUILD_TYPE=Release -DUSE_OPENCV=ON -G Ninja
cmake --build build-win
```

You should now have `build-win\appRecycle_Vending_Machine_LCD.exe`.

---

## 2. Bundle the Qt runtime with `windeployqt`

`rewingo.exe` won't run without its DLLs. The Qt deployment tool copies
every required DLL + QML plugin into one folder.

```powershell
mkdir deploy
copy build-win\appRecycle_Vending_Machine_LCD.exe deploy\rewingo.exe

# Find windeployqt in your Qt install:
C:\Qt\6.11.0\mingw_64\bin\windeployqt.exe ^
    --release ^
    --qmldir qml ^
    --qmldir components ^
    deploy\rewingo.exe
```

After this `deploy\` contains:

```
deploy\
├── rewingo.exe
├── Qt6Core.dll
├── Qt6Gui.dll
├── Qt6Quick.dll
├── …
├── platforms\qwindows.dll
├── styles\qwindowsvistastyle.dll
└── qml\               (every QML module the app references)
```

Test it before going further — `cd deploy && rewingo.exe`. The app
should launch with no missing-DLL dialog.

---

## 3. Bundle OpenCV + the Python backend

Copy the OpenCV runtime DLLs into `deploy\`:

```powershell
copy C:\OpenCV\build\x64\vc16\bin\opencv_world4*.dll deploy\
```

For the Python backend, two options:

### Option A — bundle a portable Python (recommended)
1. Download Python 3.12 embeddable from python.org.
2. Extract to `deploy\backend\python\`.
3. `deploy\backend\python\python.exe -m pip install -r backend\requirements.txt`
4. Copy `backend\server.py`, `backend\.env.example` to `deploy\backend\`.

### Option B — require Python already installed
Ship `backend\` as-is and document the prerequisite. Smaller installer
but a worse first-run experience.

---

## 4. Create the Advanced Installer project

1. Open **Advanced Installer**.
2. **New project** → choose the **Installer** category → **Professional**
   template (or **Simple** if you only have the Free edition).
3. **Product Details** panel:
   - Product Name: `ReWinGo`
   - Product Version: `0.1.0` (must match `project(... VERSION ...)`
     in `CMakeLists.txt` — the auto-updater compares against this)
   - Manufacturer: `ReWinGo`
   - Application URL: `https://github.com/YOUR_USER/rewingo`
   - Help/Support links: same URL
4. Save as `packaging\AdvancedInstaller\ReWinGo.aip`.

---

## 5. Add the deploy folder

1. Open the **Files and Folders** panel.
2. Right-click **Application Folder** → **Add Folder…**
3. Pick `deploy\` from step 2.
4. Advanced Installer recursively adds every file. Click **Yes** when
   it asks "include subdirectories".

The tree now mirrors:

```
[ProgramFilesFolder]\ReWinGo\
   rewingo.exe
   Qt6*.dll, opencv_*.dll, …
   qml\
   backend\
```

---

## 6. Set the install location + main executable

1. **Install Parameters** panel → set **Application Folder** to
   `[ProgramFilesFolder]ReWinGo\` (default is usually right).
2. **Shortcuts** panel:
   - Add a **Start Menu** shortcut: `ReWinGo` → points at `rewingo.exe`,
     icon from `resources\assets\vending.png` (convert to .ico first
     with any online converter).
   - Optional: **Desktop** shortcut with the same target.

---

## 7. Prerequisites (so installs don't fail half-way)

**Prerequisites** panel → click **Predefined → Microsoft** and check:

- **Microsoft Visual C++ Redistributable 2015–2022 (x64)** — Qt MinGW
  builds usually don't need this, but Qt MSVC builds do. Tick it for
  safety; the installer skips silently if it's already present.

If you bundled the embeddable Python in step 3, you don't need any
Python prereq.

---

## 8. Launch options (optional)

**Custom Actions / Files Operations / Installer Behavior**:

- After install, run `rewingo.exe` automatically: **Run Application
  After Install** → tick **Run application as current user** → target
  `[#rewingo.exe]`.
- Run the Flask backend at logon: **Tasks** panel → add **Logon
  Trigger** → action runs `python.exe backend\server.py` with
  `[APPDIR]backend` as working directory.

The kiosk is fine without either — the user can just double-click the
desktop shortcut.

---

## 9. Build the installer

1. Hit **Build** on the toolbar (or **F7**).
2. Advanced Installer compiles the `.msi` and (Professional edition)
   wraps it in `rewingo-setup.exe`.
3. Output lands in `packaging\AdvancedInstaller\<ProjectName>-SetupFiles\`.

Test on a clean Windows VM if you can — install, verify shortcuts,
launch, uninstall, confirm everything cleaned up.

---

## 10. Auto-update workflow (how the in-app checker fits in)

The kiosk's **Admin → About** page hits the GitHub Releases API to see
if a newer `tag_name` than its baked-in `REWINGO_VERSION` exists.

To cut a new release that users can update to:

1. Bump `project(... VERSION X.Y.Z ...)` in `CMakeLists.txt`.
2. Build the Qt app + bundle the Windows folder (steps 1-3).
3. In Advanced Installer:
   - Update **Product Version** to match.
   - Re-build the installer.
4. Push your commits and tag:
   ```powershell
   git commit -am "release: v0.2.0"
   git tag v0.2.0
   git push origin main v0.2.0
   ```
5. Go to **github.com/YOUR_USER/rewingo/releases/new**:
   - Tag: `v0.2.0`
   - Title: `ReWinGo v0.2.0`
   - Body: changelog highlights
   - Attach the new `rewingo-setup.exe`
   - Publish

The next time any installed kiosk runs its 6-hourly update check, its
`UpdateChecker` will see `v0.2.0`, set `updateAvailable = true`, and
the admin's `AdminMainPage` will show the yellow "New version
available" banner. Tapping it opens **AdminAboutPage** where the admin
can read release notes and follow the link to download the new
installer.

We deliberately do **not** auto-download or auto-install the new
build — installers on Windows almost always need a UAC prompt, and
silently bouncing the kiosk while a customer is in the middle of a
purchase would be terrible. The admin downloads the new `.exe` on
their laptop, walks it over to the kiosk on a USB stick (or pushes
it via RDP), and double-clicks.

If you ever want truly silent updates, Advanced Installer's
**Updater** panel can configure a side-by-side updater service. Wire
it to the same `tag_name` you're publishing and it'll handle the
download + MSI re-run on a schedule.

---

## 11. Set the GitHub repo

Two places need your real `owner/repo` string before the update checker
works:

1. `src/applicationmanager.cpp` — search for `YOUR_USER/rewingo`. Either
   edit the string here, or leave it and set the runtime override:
   ```ini
   ; %APPDATA%\ReWinGo\ReWinGoKiosk.ini
   [updater]
   repo=youruser/rewingo
   ```
   (Advanced Installer can ship this file pre-filled — **Files and
   Folders → Application Data Folder → Add File** with your `.ini`.)

2. `qml/admin/AdminAboutPage.qml` — the footer line `github.com/...`,
   purely cosmetic.

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Installer succeeds but `rewingo.exe` fails on launch | Missing DLL — re-run `windeployqt`, check `deploy\` includes `platforms\qwindows.dll` |
| QML errors "module \"X\" is not installed" | windeployqt missed a QML module — pass `--qmldir` for every folder that contains `.qml` files (qml, components) |
| OpenCV fails to load | Missing `opencv_videoio_*.dll` for camera support — copy all `opencv_*` DLLs from your OpenCV install, not just `opencv_world` |
| Update check always says "Up to date" even with new release | Check the Logger output for an HTTP code. Common cause: repo is private (returns 404) — the API endpoint only works on public repos without auth |
| Backend can't reach MongoDB | `%APPDATA%\ReWinGo\.env` not written. Set credentials there or via the Advanced Installer **App Data** include |

---

## Summary checklist

- [ ] Built `rewingo.exe` in Release mode
- [ ] Ran `windeployqt --qmldir qml --qmldir components`
- [ ] Copied OpenCV DLLs into `deploy\`
- [ ] Bundled (or required) Python + backend
- [ ] Advanced Installer project created, version matches `CMakeLists.txt`
- [ ] Files added, shortcut configured, prerequisites set
- [ ] Built `.msi` / `.exe`, tested on a clean machine
- [ ] Pushed tag + uploaded installer to GitHub Release
- [ ] Verified in-app **Admin → About** shows correct current version
- [ ] Verified **Check for updates** finds the new release after publishing
