# HAND-OFF — Proiect „Porți & Garaj" (control remot de la distanță)

**Data:** 2026-09-25 · **Autor:** sesiunea ground82 (Sisyphus/OMO) · **Destinatar:** sesiune/agent nou pe proiectul de porți

---

## 1. Scopul proiectului nou

Dispozitiv IoT pentru deschiderea de la distanță (telefon) a **porții de intrare în grădină** și/sau **porții de garaj**, pe aceeași platformă ESP32+MQTT ca soneria școlară ground82. Produs înrudit, dar **alt segment**: rezidențial/consumer (posibil B2B mic: pensiuni, curți de firme), nu instituțional.

## 2. Ce se reutilizează DIRECT din ground82 (nu reinventa!)

Repo-ul de referință: `~/IoT/esp32/boards/ground82_school_ring_bell/mqtt/` (ESP-IDF v4.1, make-based)

| Asset | Path | Relevanță pentru porți |
|---|---|---|
| **Modul releu** | `main/app_relay.c` + Kconfig `MQTT_RELAYS_NB`, `RELAY_ACTIVE_LOW`, `RELAY_0_TYPE` (GPIO **sau SERIAL**), `RELAY_0_GPIO`/`RELAY_0_SERIAL_ON/OFF` | comanda motorului portei — DIRECT reutilizabil |
| **Modul contact (reed)** | `main/app_contact.c` | stare deschis/închis portă — DIRECT reutilizabil (feedback OBLIGATORIU, vezi §4) |
| **Scheduler** | `main/app_scheduler.c` | ferestre de acces/livrări (fază 2) |
| **MQTT core** | `main/app_mqtt.c` (SSL 8883, LWT, reconnect), `main/app_publish_data.c` | neschimbat |
| **OTA** | `main/app_ota_esp32.c` + cmd MQTT `.../cmd/ota` | neschimbat |
| **Provisioning portal** | `main/app_wifi_portal.c/.h` + `main/portal.html` (Kconfig: `WIFI_PROVISIONING_METHOD` = SMARTCONFIG/PORTAL) | neschimbat — SSID `<client_id>-setup-<mac3>`, parolă din `WIFI_PORTAL_PASSWORD`, watchdog 5 min |
| **Switch/intrări** | `main/app_switch.c` (buton fizic: toggle <3s / provisioning ≥3s) | buton local pe cutie = deschidere manuală de rezervă |
| **NVS** | `main/app_nvs.c` | persistență stare/config |
| **Web admin pattern** | `web/index.html` (single-file, badge availability LWT, OTA button, broadcast cu version-bump) | pagină nouă pe același tipar: dale „Stare portă", „Comandă", „Istoric", „Firmware" |
| **Landing playbook** | `../prezentare/` (PLAN.md → DESIGN.md → showcase.html → index.html → qa/) | procesul complet de marketing page, refolosibil pas cu pas |
| **Deploy pipeline** | `deploy_firmware.sh` → rPi → `https://sw.iot.cipex.ro:8911/<nume>.bin` | la fel, alt nume de fișier |
| **AGENTS.md** | repo root | convenții (stil, build, teste) — COPIAZĂ în noul repo |

Familia de board-uri există deja ca pattern: `ls ~/IoT/esp32/boards/` → ground0, ground4, ground78, ground82, ground83 (derivată C3). **Noul proiect = director nou sibling**, ex. `~/IoT/esp32/boards/gate82/` (numele îl decide owner-ul).

## 3. Arhitectura de referință (ground82, pe scurt)

- **Hardware:** ESP32 (MINIR4 la școală — factor de formă releu; pentru portă un devkit/MINIR4 la fel e ok) + alimentare 5V.
- **Broker:** EMQX Cloud `r2af5151.ala.eu-central-1.emqxsl.com:8883` (SSL), user per dispozitiv creat în consola EMQX (credențialele NU se pun în git/docs — vezi §9).
- **Topicuri:** `CONFIG_DEVICE_TYPE/CONFIG_CLIENT_ID/<acțiune>/<serviciu>/<id>` — ex. `gate/gate82/cmd/relay/0`, `evt/status/relay/0`. LWT → `.../evt/status/availability`.
- **Build:** `export IDF_PATH=~/IoT/esp32/esp-idf && . $IDF_PATH/export.sh; make -j4` (IDF v4.1, make, NU idf.py).
- **Teste host:** `make -C test-host test` (Catch2, C++; mock-uri pe publish).

## 4. Specific de domeniu — porți & garaj (partea NOUĂ, de gândit)

### 4.1 Integrarea cu motorul portei (partea cea mai delicată)
- **Metoda corectă v1:** contact liber de potențial (releu NO) **în paralel cu butonul de perete** al motorului. Toți motorii majori (NICE, Somfy, Hörmann, BFT, Came) au intrare de buton (fire de comanda / comenzi locale). Dispozitivul doar „apasă butonul" — zero risc electric.
- **De evitat v1:** interfațare directă pe 230V pe placa motorului, emulare RF 433/868 (doar dacă motorul n-are buton de perete — caz rar), controlul fotocelulelor/siguranțelor — alea rămân ale motorului.
- **Comandă = impuls scurt** (single-pulse, ex. 300–500 ms), NU hold. Semnalul de „apasare buton" la majoritatea motoarelor e tocmai un impuls; la garaj unele tablouri cer STOP-uri intermediare (open→stop→close→stop) — impulsul scurt dă exact comportamentul ăsta.

### 4.2 Feedback de stare — OBLIGATORIU
Fără stare, produsul e „telecomandă oarbă". Minim:
- **2 reed switches / senzori magnetici** pe poartă: „deschis complet" + „închis complet" (poziții extreme) — intrări în `app_contact.c`.
- Ușă de garaj sectională: reed pe nivelul cel mai de sus al ușii + unul jos.
- Publicare pe `evt/status/contact/N` + stare recomandată în web (iconiță portă deschis/închis).

### 4.3 Siguranță & legal (EN 12453/13241 — porți motorizate)
- Dispozitivul **paralelizează butonul manual** → NU creează hazard nou de strivire Dacă motorul e conform (are limitare de forță/fotocelule). Păstrează asta ca principiu de design.
- **NU** declanșa mișcare pe program (scheduler) decât dacă clientul are fotocelule confirmate — la v1, schedulerul doar pentru notificări, nu pentru acționare.
- Timeout/web UI: comanda cere confirmare vizuală dacă poarta nu confirmă schimbarea de stare în X secunde.

### 4.4 Securitate (produsul deschide o casă!)
- TLS obligatoriu (deja e), **user MQTT per dispozitiv** (nu shared!), parolă puternică.
- **Audit log**: fiecare deschidere cu sursă (web/local/scheduler) + timestamp — publicat și afișat în web („Istoric").
- Opțional v2: PIN de confirmare în UI la comandă, acces temporar pentru musafiri (link/cod cu expirare) — vânzător puternic, dar NU pentru v1.

### 4.5 Features — eșalonare
| v1 (MVP) | v2 | v3 (idei) |
|---|---|---|
| stare live deschis/închis | istoric complet în web | acces temporar musafiri/livrări |
| comandă din telefon (impuls) | notificări push/mail la schimbare stare | integrare interfon/cameră |
| buton fizic local de rezervă | ferestre programate (notificări, nu acționare) | „deschis pentru livrare" 10 min |

## 5. Model de business — diferit de școli!
- Segment: consumer (casă/grădină/garaj) + B2B mic. SEAP **irelevant** aici.
- Opțiuni: **(a) plată unică** hardware (~300–450 RON instalat de client, „fără abonament" e argument de vânzare la consumer) + cloud gratuit; **(b) abonament mic** (ex. 15–25 RON/lună) doar dacă vinzi și monitorizare/acces temporar.
- Recomandarea din sesiunea ground82: **validează cu plată unică mai întâi** (cerere necunoscută), abonamentul doar pentru feature-uri care chiar au cost recurring (acces temporar, istoric lung).
- Atenție la concurența directă: Shelly (+ relais universale, ~100–150 RON, foarte populari în RO), Sonoff, aprroape toți au app-uri cloud gratuite. Diferențiatorul tău posibil: **fără cloud străin / broker propriu + web admin self-hosted + integrare cu restul platformei tale IoT**.

## 6. BOM candidat (estimativ)
| Componentă | Detaliu | Cost est. |
|---|---|---|
| ESP32-WROOM-32 | devkit sau modul pe placă proprie | 25–40 RON |
| Releu 1× | modul cu opto-izolare, contact NO 10A | 10–15 RON |
| 2× reed switch | cu magnet, cablu 2×0.5 până la poartă | 2×10 RON |
| Alimentare 5V | de la sursa motorului (dacă are 12/24V → buck) sau priză proprie | 15–25 RON |
| Cutie IP65 + borne | exterior lângă motor | 20–30 RON |
| **Total** | | **~100–130 RON** |

## 7. Pași de start recomandați (în ordine)
1. `mkdir ~/IoT/esp32/boards/gate82` (nume provizoriu) — copiază scheletul `mqtt/` din ground82 (cum s-a făcut și ground83 din ground82) + `AGENTS.md`.
2. Kconfig: `DEVICE_TYPE="gate"`, `RELAYS_NB=1`, `RELAY_0_TYPE` (GPIO la devkit), `RELAY_ACTIVE_LOW` conform plăcii de releu folosită, reed-urile pe 2 GPIO cu **pull-up-uri activate** (vezi §8).
3. Consolă EMQX Cloud: user MQTT nou, dedicat dispozitivului (nu refolosi userul altui board).
4. `sdkconfig` nou (make menuconfig) → build → flash pe devkit pe birou, cu releu pe LED + 2 reed pe mese.
5. Web admin nou (`web/index.html` pattern ground82): dale Stare/Comandă/Istoric/Firmware.
6. Validare pe teren: interfașare la motorul propriu al owner-ului (poarta lui de grădină = primul pilot, exact ca școala la sonerie).
7. Landing page după playbook-ul `../prezentare/` (PLAN → DESIGN → showcase → index → QA), alte mesaje (consumer: „deschide poarta din telefon, fără abonament").

## 8. LESSONS LEARNED ground82 — citește înainte de prima linie de cod
1. **Polaritatea releului** e per-placă: `RELAY_ACTIVE_LOW` în Kconfig (ground82: MINIR4 = active-high; modulele releu chinezești tipic active-LOW). Testează cu LED înainte de motor!
2. **GPIO floating = furtună de întreruperi** (bug GPIO0 la switch): orice intrare (reed, buton) cu pull-up/pull-down activat în software, chiar dacă placa are rezistențe externe.
3. **Portal provisioning** — capcanele rezolvate la ground82: `sdkconfig.h` se include ÎNAINTE de orice `#ifdef CONFIG_...`; MAC-ul se citește DUPĂ `esp_wifi_init()`; parola WPA2 ≥ 8 caractere; watchdog 5 min pe ambele căi (portal + smartconfig).
4. **mqtt-monitor MCP publică doar UNRETAINED** — pentru retained folosește `mosquitto_pub -h <broker> -p 8883 --capath /etc/ssl/certs -u <user> -P <pass>`.
5. **Web admin**: LWT + `devStale` (ascunde dispozitivul offline), broadcast cu **version-bump** (`maxProgV + 1` — nu funcție, valoare!), tombstone guard (`if (!txt) return;`) la ștergerea retained.
6. **OTA** se validează mereu cap-coadă (build → deploy → cmd OTA → verify version) înainte de a atinge echipamentul instalat.
7. Testele host (Catch2) au salvat de multe ori — scrie teste pentru fiecare funcție publish/handle nouă.

## 9. Secrete & configurații (reguli)
- **Niciodată** parole broker/parole portal în git sau în documente — only Kconfig defaults locale (exclude-le din git) sau NVS. Broker endpoint public = ok.
- Broker: EMQX Cloud, consola = unde se creează useri per dispozitiv.
- Config agent/opencode: `~/.config/opencode/oh-my-opencode.json` (routing modele) + `~/.config/opencode/opencode.json` (model/small_model/tool_output) — deja optimizate în sesiunea ground82, valabile și pentru sesiunea nouă.

## 10. Unelte disponibile în sesiunea de lucru
- IDF v4.1: `~/IoT/esp32/esp-idf` (make-based; `. $IDF_PATH/export.sh` înainte de build).
- `mosquitto_pub/sub` la `/usr/bin` (cu `--capath /etc/ssl/certs` pentru SSL).
- mqtt-monitor MCP (publish + subscribe; **doar unretained**; moare la idle — disconnect→connect→resubscribe).
- Playwright (browser QA) pentru web admin și landing.
- Deploy firmware: `./deploy_firmware.sh` (scp către rPi care servește `sw.iot.cipex.ro:8911`).
