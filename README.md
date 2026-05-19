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
make install
```

By default this installs `fans` to `/usr/local/bin/fans` with owner `root:wheel` and mode `4755`, then verifies the installed binary with `fans info`.

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

Restore automatic control:

```bash
fans auto-all
fans auto 0
```

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
- `fans auto-all` returns fan mode to system control.
- After manual tests, run `fans auto-all` if you do not want to keep a fixed RPM.
- If a target write does not read back within tolerance, the command exits non-zero.

## License

GPL-2.0. The SMC access code is based on smcFanControl by devnull and Hendrik Holtmann.
