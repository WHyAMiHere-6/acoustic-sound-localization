# Acoustic Sound Localization and Camera Tracking

This repository contains the implementation files for a student project that combines a multi-microphone audio front end, a Python localization/servo-control script, and a separate ESP32-CAM video stream. The goal is to estimate a sound's direction and point a servo-mounted setup toward it, while the camera provides a visual view.

> **Project status:** This is the source snapshot available for sharing. The Python localization script is incomplete: it references `read_block`, `gcc_phat`, `PAIRS`, and other helpers that are not included in the files. Results and performance claims should be added only after the full pipeline has been implemented and evaluated.

## System overview

![System overview](docs/images/system-overview.svg)

The audio capture firmware, Python processing, servo control, and camera server are separate components. The camera sketch streams video; it does not itself perform sound localization.

## Repository contents

```text
firmware/
  arduino-servo/       Arduino servo control sketch
  esp32-audio/         ESP32 I2S microphone capture sketch
  esp32-camera/        ESP32-CAM web server and support files
src/
  python/              Python processing / serial-control snapshot
docs/
  images/              Project overview diagram
requirements.txt       Python dependencies
```

## Hardware and software

- ESP32 audio board with the I2S microphone hardware expected by `firmware/esp32-audio/record_mics_py.ino`
- Arduino-compatible board and servo for `firmware/arduino-servo/sketch_oct28a.ino`
- ESP32-CAM board for the independent video server
- Python 3 for the host-side script

The exact microphone model, wiring, board revisions, and tested library versions were not documented in the source snapshot. Confirm these details against your hardware before building.

## Setup

Install Python dependencies:

```sh
python -m pip install -r requirements.txt
```

Set `PORT` and `SERVO_PORT` in `src/python/calculation.py` to the serial ports used by the ESP32 audio board and Arduino. The script currently has missing helper definitions, so it is not ready to run as-is.

Open each firmware sketch in Arduino IDE, select the matching board, install the required board support and libraries, then verify pin assignments and wiring before uploading. The camera sketch includes Wi-Fi placeholders. Replace `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` locally; do not commit real credentials.

## Important camera configuration check

The camera sketch's pin definitions appear to target an AI-THINKER ESP32-CAM, while `board_config.h` selects `CAMERA_MODEL_ESP_EYE`. Confirm the actual camera board and make the board selection consistent before compiling.

## Images and project photos

The supplied project files did not include photos of the built hardware, microphone placement, or a demo. The diagram above summarizes the source structure; it is not a photograph or a claim that the system has been validated. Add your own project photos under `docs/images/` and link them here when available.

## Report

The project report PDF/LaTeX source is intentionally not included in this repository. This README summarizes the implementation files available in the project folder.
