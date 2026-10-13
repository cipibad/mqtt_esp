# HANDOFF — ESP8266 port experiment (old ESP-IDF / FreeRTOS)

Date: 2026-10-13
Branch: `school-ring-bell`, rebased on `origin/master`, NOT pushed.
Origin: `git@github.com:cipibad/mqtt_esp.git` (public — never commit credentials).

## Goal of the next session

Build and flash this firmware for **ESP8266** using the **old** toolchain
(make-based IDF / ESP8266_RTOS_SDK, FreeRTOS old), on the serial board
(`/dev/ttyUSB0`). Validate the async MQTT publishing works there too.

## What was done in the previous session (context)

1. **Async MQTT publishing (P1-P3)** — `mqtt_publish_data` is now a non-blocking
   enqueue into `publishQueue` (depth 8, `PublishMessage{topic[64], data[512],
   qos, retain}`) drained by `handle_publish_task`, the single publisher.
   The global semaphore and `MQTT_INIT_FINISHED_BIT` gate are GONE; the gate is
   `esp_mqtt_client_get_state(client) == MQTT_CLIENT_STATE_CONNECTED`.
   Wifi force-restart no longer sleeps 10s inside the MQTT client task.
   Stack sizes: sub_pub ×3, publish worker ×4 (watermarks verified healthy).
2. **ESP-IDF v6 build fixes** for boards with sensors/heating: components
   migrated from legacy `register_component()` to `idf_component_register()`,
   `ets_delay_us`→`esp_rom_delay_us`, `gpio_pad_select_gpio`→`rom_gpio_pad_select_gpio`,
   `pcTimerGetTimerName`→`pcTimerGetName`, format/pointer fixes under -Werror,
   self-contained headers, `app_remote_log.c` added to main CMakeLists.
3. **Validated live** on the ground4_test ESP32 board (heating config):
   full boot storm identical to old firmware (A/B via serial), relay cmd
   round-trip <1s, PUBACK correlation OK, remote-log via Grafana/Loki clean.
4. **History cleaned**: web dashboard removed from branch history (belongs to
   another project); file kept untracked at `web/index.html` (gitignored).
5. Commits: 6 on top of `origin/master` (see `git log --oneline -6`).

## CRITICAL pitfall for ESP8266 with OLD SDK

**HEAD requires newer-IDF APIs even before my changes**: nested
`esp_mqtt_client_config_t` (`.broker/.credentials/.session`), `esp_mqtt_client_register_event`,
`esp_mqtt_client_get_state`. On an old SDK these will NOT compile. Options:
- Preferred: try building; when it fails on these, add compat shims or
  use `esp_mqtt_client_is_connected()` equivalent for the old SDK at the gate
  in `mqtt_publish_data` (main/app_mqtt.c).
- Fallback: build from the pre-v6 commit (the commit before
  "initial support for newer esp-idf" on this branch) and re-apply only the
  publish-queue pattern there.
- `dht.c`/`onewire.c` now call `esp_rom_delay_us` + include `esp_rom_sys.h`;
  ESP8266_RTOS_SDK v3.1+ has them; older needs `ets_delay_us` guarded by
  `#if defined(CONFIG_IDF_TARGET_ESP8266)`.
- `component.mk` files are untouched — the old make build should still find
  the components. The CMakeLists changes are irrelevant for make builds.

## Build environment (ESP32, for reference)

IDF v6.0.3 at `~/IoT/esp32/esp-idf-preint`, python env
`~/.espressif/python_env/idf6.0_py3.10_env`. The persistent shell gets
polluted; the working recipe:
```bash
unset IDF_PYTHON_ENV_PATH ESP_IDF_VERSION
export IDF_PATH=/home/cbadescu/IoT/esp32/esp-idf-preint
export PATH=$(echo $PATH | tr ':' '\n' | grep -v "esp-idf" | paste -sd:)
. $IDF_PATH/export.sh
idf.py build
```
Old IDF for ESP8266: `~/IoT/esp32/esp-idf` (v4.1-era, make) — see AGENTS.md
note: AGENTS.md build instructions are STALE (describe make/v4.1).

## Config convention

`sdkconfig` is a symlink to `../sdkconfig` (PRIVATE, outside repo, has real
wifi/broker credentials). Board variants live untracked:
`sdkconfig_ground4_test`, `sdkconfig_devboard`, `sdkconfig_esp32`, `sdkconfig_live`.
For 8266: create/point the symlink at an 8266 config. Never commit these.
Optimization is `-Os` (needed: -Og overflowed the 1MB OTA slot on the
heating config; the 8266 1MB variant is even tighter — OTA slot there is
smaller, mind `deploy_firmware.sh`'s 1MB branch using `mqtt_ssl.ota.bin`).

## Verification checklist for the 8266 session

1. `make -C test-host test` still passes (292 assertions / 73 cases) — note
   test-host fakes the v6 mqtt API; irrelevant for the 8266 build.
2. Boot storm on serial: same message set as ESP32 (available, config,
   relay status/availability/sleep, sensor/thermostat topics per config).
3. Dropped-before-connect messages are EXPECTED early (~1s) — same as old fw.
4. Relay cmd round-trip < 1s on a healthy network.
5. Watch stack watermarks via `evt/ops/stack` (through MQTT) — 8266 heap is
   ~40KB: if tight, reduce `PUBLISH_QUEUE_DEPTH` (8) and `MAX_PUBLISH_DATA_LEN`
   (512) in main/app_mqtt.h — depth 4 / len 256 was the budgeted fallback.

## Known-good reference states

- ESP32 ground4_test board (desk, /dev/ttyUSB0): running the committed fw,
  stable 4h+, broker `home_mqtt_1` (mqtt.iot.cipex.ro:8883, ROTTEN TLS chain
  on the broker — renew certs eventually; local access via rPi 192.168.200.254
  port 1883).
- Grafana: http://192.168.200.252:3000 (admin pass in
  `~/IoT/esp32/boards/ground4/mqtt/logging/.env`), Loki datasource; device
  logs under label `client="ground4_test"`.
- DHT22 on the test board fails reads every minute (likely no sensor on the
  bench GPIO) — known, unrelated to fw.

## Out of scope / later

- OTA Range-resume for the school device (ground82): network cuts long
  transfers (261KB/48KB of 1MB); fix = HTTP Range resume in app_ota or
  physical flash. School device still runs pre-fix firmware.
- Keepalive 60→30s, TWDT, hard OFF-bounding via sleep timer escalation
  (design agreed, deliberately deferred).
