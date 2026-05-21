# mac-fans-cli

Small macOS CLI for reading fan telemetry and setting fan RPM through Apple SMC.

The installed command is `fans`.

## Requirements

- macOS with an `AppleSMC` service
- Xcode Command Line Tools (`clang`, `make`)
- Admin privileges for fan writes

Fan writes are hardware and macOS-version dependent. This tool has been tested on a two-fan Apple Silicon Mac using the `Ftst`, `F%dMd`, and `F%dTg` SMC flow.

Compatibility is not guaranteed across all Mac models. Fan key names, data types, and write permissions vary by hardware and macOS version.

## Build And Test

```bash
make clean
make
make test
```

The default tests cover RPM validation, argument parsing, Intel `fpe2` RPM encoding, force-bit calculation, and raw SMC key normalization. They do not require SMC access.

## Install

Refresh sudo once so install and hardware tests do not reprompt while sudo's timestamp is valid:

```bash
sudo -v
```

Then install:

```bash
sudo make install
```

Install copies the setuid binary and registers the wake helper. Verification runs as your user (not root) so it does not block install if SMC briefly rejects the check.

By default this installs `fans` to `/usr/local/bin/fans` with owner `root:wheel` and mode `4755`, registers the wake helper, then optionally runs `fans info` as your user.

Security note: `make install` installs `fans` as a setuid-root binary so non-root users can perform SMC writes. Only install binaries you built from trusted source, and remove the setuid binary with `make uninstall` if you no longer need it.

Override the install prefix if needed:

```bash
make install PREFIX=/opt/homebrew
```

Uninstall:

```bash
make uninstall
```

## Usage

Read fans:

```bash
fans info
```

Set all detected fans to 3500 RPM:

```bash
fans 3500
# or
fans set-all 3500
```

Set one fan:

```bash
fans set 0 3500
```

Return to default (automatic control and clear saved wake RPM):

```bash
fans reset
# same as:
fans auto-all
fans auto 0
```

Sleep, wake, and reboot:

`make install` registers one login item that runs `fans restore` at login and after each wake. On wake it waits for SMC, then re-applies your saved RPM several times over ~2 minutes (macOS often rejects the first write right after wake). Shutdown/reboot returns fans to automatic. State: `~/.config/mac-fans-cli/state`. Wake logs: `~/.config/mac-fans-cli/restore.log`. Manual restore: `fans restore wake`.

Read raw SMC keys:

```bash
fans read F0Tg
fans read "FS! "
```

## Hardware Test

Install the current build first, then run:

```bash
make install
make hardware-test RPM=3500
```

Expected result:

- The command detects your fans.
- Each fan target reads back near the requested RPM.
- A follow-up `info` call shows current fan speeds settling toward the target.
- The target restores automatic control before exiting.

To intentionally leave fans forced at the requested RPM after the test:

```bash
make hardware-test RPM=3500 KEEP_FORCED=1
```

## Safety Notes

- Use a reasonable RPM within your hardware limits.
- The CLI rejects values below `500` and above `8000` RPM.
- `fans reset` (or `fans auto-all`) returns fan mode to system control and clears saved wake settings.
- After manual tests, run `fans reset` if you do not want to keep a fixed RPM.
- If a target write does not read back within tolerance, the command exits non-zero.

## License

GPL-2.0. The SMC access code is based on smcFanControl by devnull and Hendrik Holtmann.
