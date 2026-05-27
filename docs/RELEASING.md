# Cutting a release

The kiosk's in-app update checker compares its baked-in `REWINGO_VERSION`
against the latest GitHub Release `tag_name`. So "shipping a new version"
is really four steps:

1. Bump the version in `CMakeLists.txt`
2. Build the new `.deb`
3. Commit + push + tag
4. Create a GitHub Release with the tag, attach the `.deb`

The Pi auto-checks every 6 hours; the admin can also force a check from
**About → Check for updates** and then tap **Download & install vX.Y.Z**.

---

## 1. Bump the version

Open `CMakeLists.txt`, find the `project(...)` line near the top, change
the `VERSION`:

```cmake
project(Recycle_Vending_Machine_LCD VERSION 0.2.0 LANGUAGES CXX)
                                     ^^^^^^^   bump this
```

Semver convention:
- `0.2.1` for a bugfix
- `0.3.0` for a new feature
- `1.0.0` for the first stable release

That number gets compiled into the binary as `REWINGO_VERSION`. The
UpdateChecker reads it on startup.

## 2. Build the .deb

Three places you can build:

**A. On the Pi** *(production)*
```bash
ssh rewingo@<pi-ip>
cd ~/Recycle_Vending_Machine_LCD
git pull
bash packaging/build_deb.sh
# → build/rewingo_X.Y.Z_arm64.deb
```

**B. Cross-compile on a Linux x86 box** — out of scope for now.

**C. On Windows with WSL** — possible, but easier to just SSH to the Pi.

The output filename includes the version: `rewingo_0.2.1_arm64.deb`.

Pull it back to Windows so you can attach it to the Release page:

```bash
# On Windows
scp rewingo@<pi-ip>:~/Recycle_Vending_Machine_LCD/build/rewingo_*.deb .
```

## 3. Tag the commit

Tags are how git marks "this is version X.Y.Z". They're lightweight
labels on a specific commit, not branches.

```bash
cd E:\Projects\qt\Grad_Project\Recycle_Vending_Machine_LCD
git add CMakeLists.txt
git commit -m "release: v0.2.1"
git push

# Then tag the commit you just pushed:
git tag v0.2.1
git push origin v0.2.1
```

After this, `git log --oneline --decorate` shows:

```
abc1234 (HEAD -> main, tag: v0.2.1, origin/main) release: v0.2.1
```

The tag is also visible on GitHub under **Releases** → **Tags**.

## 4. Create the GitHub Release

A *Release* on GitHub is the tag PLUS a UI-facing page with changelog
notes and **binary attachments**. The kiosk's UpdateChecker fetches the
latest release via the API; the attached `.deb` is what it downloads.

1. Open your repo on github.com.
2. Click **Releases** in the right sidebar (or `<repo-url>/releases`).
3. Click **Draft a new release**.
4. **Choose a tag** → type `v0.2.1`. GitHub picks the existing one if
   you pushed it in step 3, or offers to create one.
5. **Release title**: `v0.2.1 — short description`.
6. **Describe this release**: paste highlights from `CHANGELOG.md`.
7. **Attach binaries**: drag the `rewingo_0.2.1_arm64.deb` file into
   the upload box.
8. ☑ **Set as the latest release** (default for the newest tag).
9. Click **Publish release**.

That's it. The kiosks in the field will see the new tag on their next
check.

## 5. Test the update

Force the kiosk to check now:

```
Pi desktop
  → Tap red ADMIN button on MainPage
  → AdminGatePage → enter
  → Tap "About" tile
  → Yellow card: "Update available: v0.2.1"
  → Tap "Check for updates" if no banner — forces refresh
  → Tap "⤓ Download & install v0.2.1"
  → progress bar fills
  → "Installing — the app will restart…"
  → kiosk closes, comes back at v0.2.1
```

You can watch the helper script do its work via:

```bash
tail -f /var/log/rewingo-update.log
```

You should see:
```
=== 2026-… rewingo-update-helper start: /tmp/rewingo_0.2.1_arm64.deb ===
✔ dpkg install complete
✔ rewingo relaunched (pid=…)
```

---

## One repo, many releases

Common question: **Do I need a separate repo for releases?** No.

```
github.com/iiShnouda/Smart-Recycle-Vending-Machine-Based-On-IOT
  ├── source code (visible on the Code tab)
  ├── Releases tab
  │     ├── v0.1.0  ← rewingo_0.1.0_arm64.deb attached
  │     ├── v0.2.0  ← rewingo_0.2.0_arm64.deb attached
  │     ├── v0.2.1  ← rewingo_0.2.1_arm64.deb attached
  │     └── …
  └── Tags tab     (the same v0.X.Y markers, one for every release)
```

All on the **same** repo. The Releases tab is just a GitHub feature for
attaching files + changelog to a tag. The `.deb` files don't bloat your
git history because they're stored in GitHub's release-asset storage,
not in `git`. Your repo's clone size stays small.

If you ever want to publish *only the binaries* publicly while keeping
source private, that's when you'd split into two repos — but that's not
what you're asking for here.

## Removing a release

If you publish a bad release, you can either:

- **Edit** the release page (replace the `.deb`, edit notes) — preserves the tag.
- **Delete** the release AND the tag. Force the tag delete locally + remotely:
  ```bash
  git tag -d v0.2.1
  git push --delete origin v0.2.1
  ```
  Then on GitHub → Releases → the bad release → **Delete**.

Don't delete a release once the kiosks in the field might have
downloaded the `.deb` from it — it'd 404 their downloads.

## What to put in CHANGELOG.md

Per [Keep a Changelog](https://keepachangelog.com/):

```markdown
## [0.2.1] — 2026-05-27

### Added
- Auto-start of the Flask backend via systemd.

### Fixed
- Reed switch events are no longer required from Pi GPIO.

### Changed
- License switched to PolyForm Noncommercial 1.0.0.
```

Add a section for every release. Past sections are *immutable* —
you never edit them, just add new ones at the top.
