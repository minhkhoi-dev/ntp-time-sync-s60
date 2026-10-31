<p align="center"><img src="gfx/ntpsyncui.svg" width="88" height="88" alt="NTP Time Sync"></p>

<h1 align="center">NTP Time Sync</h1>

<p align="center">
Keeps the Nokia E72's clock right with NTP over Wi-Fi (Symbian S60 3rd Edition FP2).<br>
<a href="https://minhkhoi-dev.github.io/ntp-time-sync-s60/en/">Website</a> ·
<a href="https://github.com/minhkhoi-dev/ntp-time-sync-s60/releases/latest">Latest release</a> ·
<a href="README.md">Tiếng Việt</a>
</p>

<p align="center"><a href="https://github.com/minhkhoi-dev/ntp-time-sync-s60/actions/workflows/build.yml"><img src="https://github.com/minhkhoi-dev/ntp-time-sync-s60/actions/workflows/build.yml/badge.svg" alt="Build"></a></p>

NTP Time Sync is two programs in one package:

- `ntpsyncd.exe`, a small background daemon with no screen. It starts at boot
  and keeps the system clock right using NTP over Wi-Fi.
- **NTP Time Sync** (`ntpsyncui.exe`), the app in the menu. It shows what the
  daemon is doing and changes its settings. Closing it does not stop the syncing.

## Features

- **Starts at boot** (Startup List Management API) and right after install.
  Syncs about a minute after boot, then every 6 hours (15 minutes to 1 week).
- **Wi-Fi only.** It first uses a Wi-Fi connection that is already up; Wi-Fi
  access points saved in the settings it may also switch on by itself, without
  a prompt. It never switches on anything but Wi-Fi, so it needs no SIM and
  uses no mobile data.
- **Picks your Wi-Fi once.** Right after install, or on **Sync now** with no
  access point saved, it shows the phone's Wi-Fi access point list and saves
  the one you pick. It never asks at boot or on a scheduled sync.
- **Careful measurement.** Up to 3 samples per sync, keeping the one with the
  shortest round trip. Replies are checked the way RFC 4330 asks (mode,
  version, stratum, kiss-o'-death, originate timestamp, sane delay).
- **No needless nudges.** A clock off by less than 0.5 s (a setting) is left
  alone. It sets UTC with `User::SetUTCTime`; your time zone setting does the rest.
- **Watches for clock changes.** If something else moves the clock (network
  operator time, another app), it syncs again 30 s later, at most once per 15
  minutes, so it never fights in a loop. It tells such moves from its own by
  checking the clock against the phone's tick counter, which setting the time
  does not change, and does that check every 5 minutes too, so a jump the
  system never announces still shows in the log.
- **Logs to the phone only**, in `C:\Data\ntpsyncd.log` (two files of 64 KB at
  most).

## The app

Open **NTP Time Sync** from the menu (it lands wherever newly installed apps
go, usually Applications). Its screens are in Vietnamese or English
(Settings → Language).

- **Status screen**: background sync on or off (or syncing right now), the last
  good sync and what it did to the clock, the last attempt when it failed and
  why, the server and Wi-Fi access point used, and when the next sync is due.
  It updates by itself while open.
- **Sync now** (Options, or the selection key): syncs right away.
- **Settings**: the Wi-Fi access points the daemon may switch on (a checklist
  of the phone's Wi-Fi access points), sync interval, correction threshold,
  watching for clock changes, NTP servers (one per line; Enter starts a new
  line; up to 5 are used) and the language. A change is saved to
  `C:\Data\ntpsyncd.ini` and applied at once.
- **View log**: the end of `C:\Data\ntpsyncd.log`, newest line first.
- **Stop background sync / Start background sync**: stops the daemon and keeps
  it off after a restart (`enabled=0`), or starts it again.

The status lives in the daemon's memory, so after a restart the app shows
"None yet" until the first sync of that boot, about a minute after boot.

## Install

You need:

- **A Nokia E72** (S60 3rd Edition Feature Pack 2, Symbian OS 9.3). NTP Time
  Sync was written and tested on it. Other S60 3rd Edition phones may work but
  are untested.
- **A hacked phone** (InstallServer patch). The daemon needs `WriteDeviceData`
  to set the clock and `ReadDeviceData` to read the access point list (the app
  needs `ReadDeviceData`); a self-signed package only gets those on a patched
  phone. See [Why a hacked phone](#why-a-hacked-phone).

Steps:

1. If the phone has a SIM, turn **off** Ctrl. panel → Settings → General →
   Date and time → **Network operator time**. Otherwise the network and NTP
   Time Sync take turns setting the clock.
2. Download `ntp-time-sync-x.y.z.sisx` from
   [Releases](https://github.com/minhkhoi-dev/ntp-time-sync-s60/releases/latest).
   On the phone, open the [lite page](https://minhkhoi-dev.github.io/ntp-time-sync-s60/m/en.html)
   in the phone's browser or Opera Mini.
3. Install it. It goes to phone memory (C:) on purpose: the boot entry points
   at `c:\sys\bin\ntpsyncd.exe`. The daemon starts straight away.
4. A few seconds later it shows the phone's access point list. Pick your
   Wi-Fi. It syncs over it and adds a line to `C:\Data\ntpsyncd.ini`:

   ```
   iap=13    ; HomeWifi (picked from the menu)
   ```

   From then on it may switch that access point on by itself, without asking.
   If you pick Easy WLAN ("Search for WLAN"), it is used for that sync but not
   saved, because starting it later would open a network picker. Cancel the
   list and it asks again on the next **Sync now**.
5. Open **NTP Time Sync** to see the status, sync now, or change the settings.

GitHub only accepts TLS 1.2, and the stock S60 browser only has TLS 1.0. To
download on the phone itself, install the [TLS patch](https://nnproject.cc/tls/),
or download on a computer and copy the file over.

A first sync right after install looks like:

```
2026-10-22 22:31:04  ntpsyncd 2.0.3 started by hand
2026-10-22 22:31:04  Sync (started by hand)
2026-10-22 22:31:04  Asking which Wi-Fi access point to use
2026-10-22 22:31:12  Wi-Fi access point 13 (HomeWifi) picked
2026-10-22 22:31:12  Saved access point 13 to C:\Data\ntpsyncd.ini
2026-10-22 22:31:16  Clock corrected by -214.381 s - vn.pool.ntp.org (203.0.113.7), stratum 2, 47 ms round trip
2026-10-22 22:31:16  Next sync around 04:31
```

### Good to know

- **Updating:** if the installer says ntpsyncd is in use, choose Stop
  background sync in the app first, and Start background sync after installing.
- **Uninstalling:** choose Stop background sync first; otherwise the installer
  may say ntpsyncd is in use. `C:\Data\ntpsyncd.ini` is kept.
- **Privacy:** it only talks to the NTP servers in its settings (UDP port 123).
  The log stays on the phone.

### Why a hacked phone

Since S60 3rd Edition, every program declares the capabilities it needs. The
installer only grants system capabilities to packages signed through Symbian
Signed; a self-signed package only gets user capabilities, such as network access.

| Capability | Kind | Used to |
|---|---|---|
| `WriteDeviceData` | system | Set the time with `User::SetUTCTime`, the only way to set the clock on Symbian. Without it the app could measure how far off the clock is, but not fix it |
| `ReadDeviceData` | system | Read the access point list: pick out the Wi-Fi ones, show the checklist in Settings and the Wi-Fi name on the status screen |
| `NetworkServices` | user | Send NTP packets. Self-signed packages get it as usual |

Symbian Signed, including its free per-IMEI developer certificates, has shut
down, so there is no longer a legitimate way to get a signature with system
capabilities. The "hack" is only the InstallServer patch: the installer skips
the signature check and grants a self-signed package what it asks for. No
Open4All or other patch is needed.

## Settings (`C:\Data\ntpsyncd.ini`)

| Key | Default | Meaning |
|---|---|---|
| `server=` | vn / asia / global pool | NTP servers tried in order, up to 5 lines |
| `iap=` | none | Wi-Fi access point IDs the daemon may start itself, one per line. Picking one from the list adds it here. IDs that are not Wi-Fi access points on this phone (packet data, or one that was deleted) and Easy WLAN are skipped, with a line in the log. With no usable one, scheduled syncs only work while Wi-Fi is already up, checking every 10 minutes until one succeeds |
| `interval=` | 360 | Minutes between syncs (15 to 10080) |
| `threshold=` | 500 | Leave the clock alone if it is off by less than this many ms |
| `watch=` | 1 | Sync again when something else changes the clock |
| `enabled=` | 1 | 0 stops the daemon and keeps it off at boot. Set by Stop / Start background sync in the app |

The app's Settings screen covers all of these (interval and threshold as a
list of common values). Saving a setting there rewrites the whole file in a
standard layout, so comments you added by hand are not kept. The file lists
every Wi-Fi access point, the ones not in use as commented lines
(`;iap=3    ; HomeWifi`); remove the `;` to enable one. Changes made by hand
apply at the next sync, or at once on **Sync now**.

After a failed sync it retries in 10 minutes, then backs off 20, 40, 80…
minutes (capped at the interval) when it has access points to switch on.

## Troubleshooting

- **"Could not set the clock (-46)… WriteDeviceData capability missing"**: the
  package lost its capabilities. Re-sign it and install with the InstallServer
  patch applied.
- **"No Wi-Fi connection is up and no iap= is enabled"**: tick your Wi-Fi in
  Settings → Wi-Fi access points, or use **Sync now** and pick it from the list.
- **"No access point picked (-3)"**: the list was cancelled. Nothing is saved;
  it asks again on the next **Sync now**.
- **"Settings: iap=N is not a Wi-Fi access point on this phone, ignored"**: that
  access point was deleted (re-creating one, say after a Wi-Fi password change,
  gives it a new ID) or it is not Wi-Fi. Tick the right one in Settings → Wi-Fi
  access points; saving there drops the stale line.
- **"… did not connect (n)"**: that access point is out of range or its password
  changed. It tries the next one.
- **"No reply from …"**: UDP port 123 is blocked on that network, or the server
  is down. Add another server.
- **"Clock moved by +X s by something else"**: another program set the time.
  The jump is measured against the tick counter; "N s after ntpsyncd last set
  it" helps tell whether something undoes its corrections.
- **"Clock moved by +X s with no time-change notice"**: the clock jumped without
  the system announcing it, found by the 5-minute check.
- **"Clock reads … off right after it was set"**: the phone did not keep the
  exact time it was given (for example, it rounded it to whole seconds).
- **"UTC offset changed from A to B min"**: the time zone or daylight saving
  time changed.
- **Nothing logged after a reboot**: the startup entry was not accepted, or
  background sync is off (the log says `Background sync is off`). Use Start
  background sync in the app, and check that
  `c:\private\101f875a\import\[E7A15D01].rsc` exists.
- **The app says "Stopped" but you did not stop it**: the daemon is not running.
  Start background sync starts it again; the log tells why it ended.
- **Sync now does nothing**: a sync is already running (the status says so).
  Starting `c:\sys\bin\ntpsyncd.exe` from X-plore while the daemon runs also
  means "sync now".

To keep it from reacting to other clock changes, turn off Settings → Watch
clock changes (`watch=0`).

## Build from source

The build runs in the public Docker image
[`minhkhoi4dev/s60v3-sdk-docker`](https://hub.docker.com/r/minhkhoi4dev/s60v3-sdk-docker)
(S60 3rd Edition FP2 SDK, GnuPoc, GCCE), on Mac, Linux or Windows (WSL):

```sh
bash group/docker-build.sh   # -> sis/ntpsyncd.sisx
```

The script copies the project into the container before building, because the
32-bit GCCE tools fail on a Windows drive mounted in WSL ("Value too large for
defined data type"). On Windows, run it from WSL:
`wsl -- bash /mnt/c/path/to/ntp-time-sync-s60/group/docker-build.sh`. Set
`NTPSYNCD_SDK_IMAGE` to use another image.

With a local GnuPoc SDK instead:

```sh
export EPOCROOT=/path/to/s60_sdk/   # trailing slash required
group/build.sh                       # bldmake, abld build gcce urel, makesis, signsis
```

`build.sh` checks that both exes and the app's resources were really built
(abld reports success even when a file fails), puts `gfx/ntpsyncui.svg` into
`ntpsyncui_aif.mif` with GnuPoc's `mifconv` (SVG text as it is; the phone reads
it), and replaces `$(EPOCROOT)` in `sis/ntpsyncd.pkg` (written to
`sis/build.pkg`) before `makesis`. The first build makes a self-signed key
pair, `sis/dev.key` and `sis/dev.cer`, and later builds reuse it.

The NTP maths has PC unit tests (offset and delay, the 2036 NTP era rollover, a
phone clock reset to 2009, and every rejection rule):

```sh
g++ -std=c++98 -Wall -Wextra -Itest/shim -Iinc test/test_ntppacket.cpp src/ntppacket.cpp -o /tmp/t && /tmp/t
```

GitHub Actions (`.github/workflows/build.yml`) runs the tests and the Docker
build on every push that changes the code. To release a version, set it in
`sis/ntpsyncd.pkg` and in `KVersion` in `src/daemon.cpp` (the build checks they
match), then push a tag `v<version>` (for example `v2.0.3`). Actions makes a
release with `ntp-time-sync-<version>.sisx` and updates the website (`site/`,
`.github/workflows/pages.yml`). With the repository secrets `NTPSYNCD_DEV_KEY`
and `NTPSYNCD_DEV_CER` it signs with that key; without them each run makes a
throwaway key, which a hacked phone takes as well.

## Source layout

```
group/   bld.inf, ntpsyncd.mmp (daemon), ntpsyncui.mmp (app),
         build.sh (in the SDK), docker-build.sh (via Docker)
inc/     headers (ntpsyncd.hrh holds the UIDs and P&S keys,
         ntpshared.h what the daemon publishes and the app may send)
src/     main.cpp          single instance; 2nd launch = sync now
         daemon.cpp        scheduling, clock-change watch, applying the result,
                           status and commands (Publish & Subscribe)
         syncsession.cpp   Wi-Fi connection, DNS, UDP exchange, timeouts
         ntppacket.cpp     SNTP packet build/validate, offset/delay maths
         config.cpp        ini file, access point list (daemon and app)
         logger.cpp        C:\Data\ntpsyncd.log (rotates at 64 KB)
         propwatcher.cpp   P&S subscriber (daemon and app)
src/ui/  the NTP Time Sync app: status screen, settings list, log viewer
data/    startup list entry; the app's resources, registration and texts
         (ntpsyncui_vi.loc, ntpsyncui_en.loc)
gfx/     ntpsyncui.svg, the app icon (SVG Tiny, put into a MIF by build.sh)
sis/     ntpsyncd.pkg
test/    PC unit tests for ntppacket.cpp
site/    the website and the lite page for S60 phones (site/m/)
```

The daemon and the app talk through Publish & Subscribe in the daemon's
category: the daemon writes its status (key 2), the app sends commands (key 3:
sync now, reload settings, stop), and only the app's SID may write that key.

The UIDs `0xE7A15D01` (daemon, package) and `0xE7A15D02` (app) are in the
self-signed range. If you change them, change them in `ntpsyncd.hrh`, both
`.mmp` files, the `.pkg` (including the `[E7A15D01].rsc` name) and the app's
private folder name for `ui.ini`.

C++98 for GCCE 3.4.3: no STL, no exceptions.

---

A personal project by [Khoi Nguyen](https://github.com/minhkhoi-dev). Not affiliated
with Nokia, HMD Global or the NTP Pool Project.
