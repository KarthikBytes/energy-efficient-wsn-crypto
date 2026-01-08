# wireless-sensor-networks — ns-3 protocol example

This repository (or project folder) contains a single ns-3 simulation main file called `protocol.cc` that you can run with an ns-3.42 installation from the ns-allinone bundle.

This README explains how to place `protocol.cc` in the correct location, build ns-3 if needed, and run the simulation.

---

## Prerequisites

- Linux (or macOS) with a working shell.
- ns-3.42 installed via the ns-allinone bundle (the instructions below assume the bundle is located at `~/ns-allinone-3.42`).
- Common build tools (gcc, g++, python, etc.) required by ns-3. These are usually installed when you set up ns-allinone; see the ns-3 documentation if you need to install missing dependencies.
- Sufficient permissions to copy files into the ns-3 tree and execute build/run commands.

Official ns-3 website: [https://www.nsnam.org/](https://www.nsnam.org/)

---

## Where to place `protocol.cc`

Copy or move your `protocol.cc` main file into the `scratch` directory of the ns-3.42 tree:

```bash
# from wherever your protocol.cc currently is:
cp protocol.cc ~/ns-allinone-3.42/ns-3.42/scratch/
# or move it
mv protocol.cc ~/ns-allinone-3.42/ns-3.42/scratch/
```

After this, the file path will be:
`~/ns-allinone-3.42/ns-3.42/scratch/protocol.cc`

---

## Build ns-3 (if not already built)

There are two common ways to build ns-3 when using ns-allinone:

1) From the ns-allinone root (recommended if you haven't yet built anything):

```bash
cd ~/ns-allinone-3.42
./build.py
```

2) Or directly inside the `ns-3.42` directory using waf:

```bash
cd ~/ns-allinone-3.42/ns-3.42
./waf configure
./waf build
```

Note: Building may take several minutes depending on your machine and which optional models are enabled.

---

## Run the simulation

Change to the ns-3.42 directory, then run the scratch program. The instructions below follow the command format you provided:

```bash
# change into the ns-3.42 tree
cd ~/ns-allinone-3.42/ns-3.42

# run the protocol.cc scratch program
./ns3 run scratch/protocol.cc
```

If the `./ns3` wrapper is not executable or not available, try using the waf runner instead:

```bash
# using waf (alternative)
./waf --run "scratch/protocol"
```

To capture output to a file:

```bash
./ns3 run scratch/protocol.cc > protocol_output.txt 2>&1
# or with waf
./waf --run "scratch/protocol" > protocol_output.txt 2>&1
```

---

## Common notes & troubleshooting

- Permission denied running `./ns3`: ensure the file is executable:
  ```bash
  chmod +x ./ns3
  ```
  If `./ns3` does not exist, use the `./waf --run` command as shown above.

- If you get build errors, re-run the build and inspect the logs:
  ```bash
  cd ~/ns-allinone-3.42
  ./build.py  # or ./ns-3.42/build.py if present
  ```

- If compilation of `protocol.cc` fails, check:
  - `#include` directives at the top of `protocol.cc` are correct for ns-3.42 APIs.
  - You are not using APIs removed/renamed in ns-3.42.
  - Any extra .cc/.h files required by `protocol.cc` are also present (put them in `scratch` or in the ns-3 module tree and update build accordingly).

- To run with different ns-3 command-line arguments (if your program accepts them), pass them after `--` when using `waf`:
  ```bash
  ./waf --run "scratch/protocol --myArg=42 --Verbose=true"
  ```

---

## Example minimal workflow

1. Place file:
   ```bash
   cp protocol.cc ~/ns-allinone-3.42/ns-3.42/scratch/
   ```
2. Build ns-3 (if needed):
   ```bash
   cd ~/ns-allinone-3.42
   ./build.py
   ```
3. Run:
   ```bash
   cd ~/ns-allinone-3.42/ns-3.42
   ./ns3 run scratch/protocol.cc
   ```
4. Run using the automation script
   ```bash
   cd ~/ns-allinone-3.42/ns-3.42/sim-server
   chmod +x run.sh
   ./run.sh
   ```
   ```
    Working -> Start WebSocket server (ws://localhost:8080) → wait for clients → start web server (http://0.0.0.0:3000) → servers running → dashboard at http://localhost:3000/dashboard.html
    ```
4.1 Dashboard access
  ```
       http://localhost:3000/dashboard.html
  ```
---
# NetAnim XML Trace Visualizer

This repository contains a NetAnim-based animator for visualizing XML trace files produced by network simulators (for example, ns-3's AnimationInterface). This README explains practical, step-by-step instructions to build, run and troubleshoot NetAnim animations using both the GUI and command-line approaches.

Table of contents
- Prerequisites
- Quick start (GUI)
- Quick start (command-line / headless)
- Building NetAnim from source
- Generating XML traces (ns-3 example)
- Usage examples and tips
- Troubleshooting
- Project structure
- Contributing & license
- Contact

---

Prerequisites
- Supported OS: Linux (Ubuntu/Debian), macOS, Windows (MSYS2 / Qt Creator). Examples below use Ubuntu.
- Build tools: git, make, gcc / clang, cmake (optional)
- Qt: Qt 5.x (recommended) or Qt 6 (check project compatibility)
  - Ubuntu apt packages (example): build-essential git cmake qt5-qmake qtbase5-dev qttools5-dev-tools libqt5svg5-dev
  - macOS (Homebrew): brew install qt@5 cmake
  - Windows: Install Qt (Qt Creator) and MSVC/MinGW as appropriate
- Optional tools:
  - ffmpeg — to record or convert screen captures into a video
  - xvfb-run — to run GUI apps headless on Linux (for automated screenshotting or video export)
- Python 3 — useful for helper scripts in examples

Install packages on Ubuntu (example)
```bash
sudo apt update
sudo apt install -y build-essential git cmake qt5-qmake qtbase5-dev qttools5-dev-tools libqt5svg5-dev ffmpeg xvfb x11-apps
```

---

Quick start — GUI (recommended for interactive exploration)
1. Build NetAnim (see the Build section). After building, you'll have a binary (commonly `NetAnim` or `NetAnim-Qt`).
2. Launch the NetAnim GUI:
```bash
./NetAnim
```
3. In the GUI: File → Open → select your XML trace file (e.g., `anim.xml`).
4. Use playback controls: Play / Pause, Timeline slider, Speed control. Use the node list to select nodes, inspect packet events, enable/hide labels or links.


