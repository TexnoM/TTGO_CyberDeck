#include "wifi_manager.h"
#include "settings_manager.h"
#include "ui_manager.h"
#include "animation_engine.h"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include <esp_wifi.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/inet_chksum.h>
#include <lwip/netdb.h>

WifiManager Net;

// Minimal ICMP header structure for raw Echo Request
struct IcmpEchoHdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t chksum;
    uint16_t id;
    uint16_t seqno;
};

// Volatile counters updated in Wi-Fi promiscuous RX callback
static volatile uint32_t s_rxMgmtCount  = 0;
static volatile uint32_t s_rxDataCount  = 0;
static volatile uint32_t s_rxCtrlCount  = 0;
static volatile int32_t  s_rxLastRssi   = -95;
static volatile uint16_t s_rxChanBurst[13] = {0};

static void IRAM_ATTR wifiPromiscuousCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!buf) return;
    const auto* pkt = static_cast<const wifi_promiscuous_pkt_t*>(buf);
    s_rxLastRssi = pkt->rx_ctrl.rssi;
    uint8_t ch = pkt->rx_ctrl.channel;
    if (ch >= 1 && ch <= 13) {
        if (s_rxChanBurst[ch - 1] < 60000) {
            s_rxChanBurst[ch - 1] = s_rxChanBurst[ch - 1] + 1;
        }
    }
    if (type == WIFI_PKT_MGMT) {
        s_rxMgmtCount = s_rxMgmtCount + 1;
    } else if (type == WIFI_PKT_DATA) {
        s_rxDataCount = s_rxDataCount + 1;
    } else {
        s_rxCtrlCount = s_rxCtrlCount + 1;
    }
}

static const char kPortalHtmlHead[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TTGO_CyberDeck // CYBER CONSOLE</title>
<style>
body{background:#050807;color:#d8ffe4;font-family:monospace;margin:0;padding:12px;}
.card{max-width:420px;margin:0 auto 12px auto;border:1px solid #00ff6e;background:#0a1610;padding:14px;box-shadow:0 0 14px rgba(0,255,110,0.18);}
h1{color:#00ff6e;font-size:18px;margin:0 0 4px 0;letter-spacing:1px;}
h2{color:#00e1ff;font-size:13px;margin:12px 0 6px 0;border-bottom:1px solid #163828;padding-bottom:4px;}
.sub{color:#00e1ff;font-size:11px;margin-bottom:10px;}
label{display:block;font-size:11px;color:#7ec89b;margin-top:8px;}
input{width:100%;box-sizing:border-box;background:#040a07;border:1px solid #00e1ff;color:#fff;padding:7px;font-family:monospace;margin-top:3px;}
button{width:100%;margin-top:10px;padding:9px;background:#00ff6e;color:#040a07;border:none;font-weight:bold;font-family:monospace;cursor:pointer;}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:6px;}
.btn-c{background:#09222c;color:#00e1ff;border:1px solid #00e1ff;margin-top:0;padding:8px;font-size:11px;}
.btn2{background:#0a1610;color:#ff3246;border:1px solid #ff3246;}
.stat{font-size:11px;color:#ffb900;padding:6px;border:1px dashed #ffb900;margin-bottom:8px;line-height:1.5;}
</style></head><body><div class="card">
<h1>TTGO_CyberDeck // WEB OS</h1>
<div class="sub">REMOTE CYBERDECK COMMAND &amp; TELEMETRY</div>
)rawliteral";

void WifiManager::begin() {
    uint32_t chipId = (uint32_t)(ESP.getEfuseMac() >> 32) ^ (uint32_t)ESP.getEfuseMac();
    snprintf(_apSsid, sizeof(_apSsid), "TTGO-CyberDeck-%04X", (uint16_t)(chipId & 0xFFFF));

    _bootEpochBase    = Settings.get().lastSavedEpoch;
    _bootMillisAtBase = millis();
    if (_bootEpochBase > 1700000000UL) {
        struct timeval tv{ (time_t)_bootEpochBase, 0 };
        settimeofday(&tv, nullptr);
    }

#if BATTERY_SENSE_ENABLED
    if (PIN_BAT_ADC_EN >= 0) {
        pinMode(PIN_BAT_ADC_EN, OUTPUT);
        digitalWrite(PIN_BAT_ADC_EN, HIGH);
    }
    analogReadResolution(12);
    updateBatteryReading();
#endif

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    if (Settings.hasWifiCredentials()) {
        connectStoredWifi();
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(false, false);
    }
}

void WifiManager::ensureWebServerStarted() {
    if (_webServerRunning) return;
    setupWebPortalRoutes();
    _server.begin();
    _webServerRunning = true;
}

void WifiManager::connectStoredWifi() {
    if (_sniffer.active) {
        stopSniffer();
    }
    String ssid, pass;
    if (!Settings.loadWifiCredentials(ssid, pass)) {
        return;
    }
    if (_apPortalActive) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_STA);
    }
    WiFi.begin(ssid.c_str(), pass.c_str());
    _connecting       = true;
    _connAttemptStart = millis();
    _ntpConfigured    = false;
}

void WifiManager::disconnectWifi() {
    _connecting = false;
    WiFi.disconnect(false, false);
}

bool WifiManager::isConnected() const {
    return (WiFi.status() == WL_CONNECTED);
}

String WifiManager::localIp() const {
    return isConnected() ? WiFi.localIP().toString() : String("0.0.0.0");
}

String WifiManager::gatewayIp() const {
    return isConnected() ? WiFi.gatewayIP().toString() : String("0.0.0.0");
}

String WifiManager::subnetMask() const {
    return isConnected() ? WiFi.subnetMask().toString() : String("0.0.0.0");
}

String WifiManager::dnsIp() const {
    return isConnected() ? WiFi.dnsIP(0).toString() : String("0.0.0.0");
}

String WifiManager::macAddress() const {
    return WiFi.macAddress();
}

String WifiManager::connectedSsid() const {
    return isConnected() ? WiFi.SSID() : String("OFFLINE");
}

int32_t WifiManager::currentRssi() const {
    return isConnected() ? WiFi.RSSI() : -100;
}

void WifiManager::startConfigPortal() {
    if (_sniffer.active) {
        stopSniffer();
    }
    if (_apPortalActive) return;

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(_apSsid);
    ensureWebServerStarted();
    _apPortalActive = true;
}

void WifiManager::stopConfigPortal() {
    if (!_apPortalActive) return;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _apPortalActive = false;
}

String WifiManager::portalIp() const {
    return _apPortalActive ? WiFi.softAPIP().toString() : String("192.168.4.1");
}

uint8_t WifiManager::portalClientCount() const {
    return _apPortalActive ? WiFi.softAPgetStationNum() : 0;
}

void WifiManager::setupWebPortalRoutes() {
    _server.on("/", HTTP_GET, [this]() {
        _webRequests++;
        char hms[16], dateStr[24];
        getFormattedTime(hms, sizeof(hms), dateStr, sizeof(dateStr));
        String html = FPSTR(kPortalHtmlHead);
        html += "<div class='stat'>";
        html += "<b>MCU:</b> ESP32 " + String(ESP.getCpuFreqMHz()) + "MHz | <b>FREE RAM:</b> " + String(ESP.getFreeHeap() / 1024) + " KB<br>";
        html += "<b>FPS:</b> " + String((int)(Anim.fps() + 0.5f)) + " | <b>CLOCK:</b> <span id='clk'>" + String(hms) + " (" + String(dateStr) + ")</span><br>";
        if (isConnected()) {
            html += "<b>WIFI:</b> " + connectedSsid() + " (" + String(currentRssi()) + " dBm) // " + localIp();
        } else {
            html += "<b>WIFI:</b> AP MODE (" + String(_apSsid) + " // 192.168.4.1)";
        }
        html += "</div>";

        html += "<h2>REMOTE DECK CONTROL</h2>";
        html += "<div class='grid'>";
        html += "<form method='POST' action='/cmd'><input type='hidden' name='c' value='theme'><button class='btn-c'>CYCLE THEME</button></form>";
        html += "<form method='POST' action='/cmd'><input type='hidden' name='c' value='matrix'><button class='btn-c'>OPEN MATRIX</button></form>";
        html += "<form method='POST' action='/cmd'><input type='hidden' name='c' value='starfield'><button class='btn-c'>OPEN STARFIELD</button></form>";
        html += "<form method='POST' action='/cmd'><input type='hidden' name='c' value='radar'><button class='btn-c'>OPEN RADAR</button></form>";
        html += "<form method='POST' action='/cmd'><input type='hidden' name='c' value='clock'><button class='btn-c'>OPEN CLOCK</button></form>";
        html += "<form method='POST' action='/cmd'><input type='hidden' name='c' value='home'><button class='btn-c'>GO LAUNCHER</button></form>";
        html += "</div>";

        html += "<form method='POST' action='/msg'>";
        html += "<label>SEND PAGER ALERT TO DECK SCREEN</label>";
        html += "<input name='m' maxlength='22' placeholder='HACK THE PLANET!' required>";
        html += "<button type='submit'>TRANSMIT TO SCREEN</button></form>";

        html += "<h2>WIFI &amp; NODE SETTINGS</h2>";
        html += "<form method='POST' action='/save'>";
        html += "<label>WIFI SSID</label>";
        String curSsid, dummyPass;
        Settings.loadWifiCredentials(curSsid, dummyPass);
        html += "<input name='ssid' maxlength='32' value='" + curSsid + "' placeholder='Enter network SSID' required>";
        html += "<label>WIFI PASSWORD (leave blank to keep stored)</label>";
        html += "<input name='pass' type='password' maxlength='63' placeholder='••••••••'>";
        html += "<label>HOME SERVER / PING HOST</label>";
        html += "<input name='ping' maxlength='47' value='" + String(Settings.get().pingHost) + "'>";
        html += "<label>TIMEZONE (UTC OFFSET HOURS)</label>";
        html += "<input name='tz' type='number' min='-12' max='14' value='" + String(Settings.get().timezoneHours) + "'>";
        html += "<button type='submit'>SAVE &amp; CONNECT</button>";
        html += "</form>";
        html += "<form method='POST' action='/clear'><button class='btn2' type='submit'>WIPE STORED WIFI</button></form>";
        html += "<script>fetch('/time?t='+Math.floor(Date.now()/1000),{method:'POST'});</script>";
        html += "</div></body></html>";
        _server.send(200, "text/html", html);
    });

    _server.on("/time", HTTP_ANY, [this]() {
        _webRequests++;
        String tStr = _server.arg("t");
        uint32_t ep = (uint32_t)strtoul(tStr.c_str(), nullptr, 10);
        if (ep > 1700000000UL) {
            setWifiEpoch(ep);
            _server.send(200, "text/plain", "SYNC_OK");
        } else {
            _server.send(400, "text/plain", "INVALID_EPOCH");
        }
    });

    _server.on("/cmd", HTTP_POST, [this]() {
        _webRequests++;
        String c = _server.arg("c");
        std::strncpy(_lastRemoteCmd, c.c_str(), sizeof(_lastRemoteCmd) - 1);
        if (c == "theme") {
            Settings.get().themeId = (Settings.get().themeId + 1) % THEME_COUNT;
            Settings.markDirty();
            Settings.saveIfDirty();
            UI.showToast(Settings.palette().name);
            UI.invalidateAll();
        } else if (c == "matrix") {
            UI.openApp(APP_MATRIX);
        } else if (c == "starfield") {
            UI.openApp(APP_STARFIELD);
        } else if (c == "radar") {
            UI.openApp(APP_WIFI_RADAR);
        } else if (c == "clock") {
            UI.openApp(APP_CLOCK);
        } else if (c == "home") {
            UI.returnToLauncher();
        }
        _server.sendHeader("Location", "/");
        _server.send(303);
    });

    _server.on("/msg", HTTP_POST, [this]() {
        _webRequests++;
        String m = _server.arg("m");
        m.trim();
        if (m.length() > 0) {
            std::strncpy(_lastRemoteCmd, m.c_str(), sizeof(_lastRemoteCmd) - 1);
            Buttons.playClickTone(2400, 30);
            UI.showToast(m.c_str(), Settings.palette().secondary, 3500);
        }
        _server.sendHeader("Location", "/");
        _server.send(303);
    });

    _server.on("/save", HTTP_POST, [this]() {
        _webRequests++;
        String ssid = _server.arg("ssid");
        String pass = _server.arg("pass");
        String ping = _server.arg("ping");
        String tz   = _server.arg("tz");

        ssid.trim();
        ping.trim();

        if (ssid.length() > 0) {
            if (pass.length() == 0) {
                String oldSsid, oldPass;
                if (Settings.loadWifiCredentials(oldSsid, oldPass) && oldSsid == ssid) {
                    pass = oldPass;
                }
            }
            Settings.saveWifiCredentials(ssid, pass);
        }
        if (ping.length() > 0) {
            std::strncpy(Settings.get().pingHost, ping.c_str(), sizeof(Settings.get().pingHost) - 1);
            Settings.get().pingHost[sizeof(Settings.get().pingHost) - 1] = '\0';
        }
        if (tz.length() > 0) {
            Settings.get().timezoneHours = constrain(tz.toInt(), -12, 14);
        }
        Settings.forceSave();
        _ntpConfigured = false;

        String resp = FPSTR(kPortalHtmlHead);
        resp += "<div class='stat'>CREDENTIALS STORED IN NVS. RECONNECTING...</div>";
        resp += "<a href='/' style='color:#00e1ff;'>[ RETURN TO CONSOLE ]</a></div></body></html>";
        _server.send(200, "text/html", resp);

        connectStoredWifi();
    });

    _server.on("/clear", HTTP_POST, [this]() {
        _webRequests++;
        Settings.clearWifiCredentials();
        disconnectWifi();
        String resp = FPSTR(kPortalHtmlHead);
        resp += "<div class='stat'>STORED WIFI CREDENTIALS WIPED.</div>";
        resp += "<a href='/' style='color:#00e1ff;'>[ RETURN TO CONSOLE ]</a></div></body></html>";
        _server.send(200, "text/html", resp);
    });
}

void WifiManager::startSniffer() {
    if (_sniffer.active) return;
    if (_scanInProgress) {
        WiFi.scanDelete();
        _scanInProgress = false;
    }
    if (_apPortalActive) {
        stopConfigPortal();
    }
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_STA);

    s_rxMgmtCount = 0;
    s_rxDataCount = 0;
    s_rxCtrlCount = 0;
    for (int i = 0; i < 13; ++i) s_rxChanBurst[i] = 0;

    wifi_promiscuous_filter_t filt{};
    filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL;
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous_rx_cb(&wifiPromiscuousCb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(_sniffer.currentChannel, WIFI_SECOND_CHAN_NONE);

    _sniffer.active     = true;
    _lastSnifferSecMs   = millis();
    _lastSnifferHopMs   = millis();
}

void WifiManager::stopSniffer() {
    if (!_sniffer.active) return;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    _sniffer.active = false;

    if (Settings.hasWifiCredentials()) {
        connectStoredWifi();
    }
}

void WifiManager::stepSnifferChannel() {
    _sniffer.hopping = false;
    _sniffer.currentChannel = (_sniffer.currentChannel % 13) + 1;
    if (_sniffer.active) {
        esp_wifi_set_channel(_sniffer.currentChannel, WIFI_SECOND_CHAN_NONE);
    }
}

void WifiManager::toggleSnifferHopping() {
    _sniffer.hopping = !_sniffer.hopping;
}

void WifiManager::updateSnifferMetrics() {
    if (!_sniffer.active) return;
    uint32_t now = millis();

    // Channel hopping every 240 ms when hopping mode is active
    if (_sniffer.hopping && (now - _lastSnifferHopMs) >= 240) {
        _lastSnifferHopMs = now;
        _sniffer.currentChannel = (_sniffer.currentChannel % 13) + 1;
        esp_wifi_set_channel(_sniffer.currentChannel, WIFI_SECOND_CHAN_NONE);
    }

    // Aggregate packets every 500 ms for smooth responsive graphs
    if ((now - _lastSnifferSecMs) >= 500) {
        _lastSnifferSecMs = now;
        uint32_t mgmt = s_rxMgmtCount; s_rxMgmtCount = 0;
        uint32_t data = s_rxDataCount; s_rxDataCount = 0;
        uint32_t ctrl = s_rxCtrlCount; s_rxCtrlCount = 0;

        uint32_t totalHalfSec = mgmt + data + ctrl;
        _sniffer.mgmtPerSec = mgmt * 2;
        _sniffer.dataPerSec = data * 2;
        _sniffer.pktPerSec  = totalHalfSec * 2;
        _sniffer.totalCaptured += totalHalfSec;
        _sniffer.lastRssi = s_rxLastRssi;

        _sniffer.historyPps[_sniffer.historyIdx] = (uint16_t)min<uint32_t>(_sniffer.pktPerSec, 9999);
        _sniffer.historyIdx = (_sniffer.historyIdx + 1) % 32;

        for (int i = 0; i < 13; ++i) {
            uint16_t burst = s_rxChanBurst[i];
            s_rxChanBurst[i] = 0;
            // Exponential moving average per channel
            _sniffer.channelPkts[i] = (uint16_t)((_sniffer.channelPkts[i] * 2 + burst * 4) / 3);
        }
    }
}

bool WifiManager::startAsyncScan() {
    if (_sniffer.active) {
        stopSniffer();
    }
    if (_scanInProgress) return false;
    int16_t st = WiFi.scanComplete();
    if (st == WIFI_SCAN_RUNNING) {
        _scanInProgress = true;
        return false;
    }
    if (!_apPortalActive && WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    }
    WiFi.scanDelete();
    int16_t res = WiFi.scanNetworks(true /* async */, false /* show_hidden */);
    if (res == WIFI_SCAN_RUNNING || res >= 0) {
        _scanInProgress = true;
        return true;
    }
    return false;
}

void WifiManager::collectScanResults() {
    int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;

    _scanInProgress = false;
    if (n < 0) return;

    String currentSsid = isConnected() ? WiFi.SSID() : String("");

    int indices[64];
    int count = min<int>(n, 64);
    for (int i = 0; i < count; ++i) indices[i] = i;
    std::sort(indices, indices + count, [](int a, int b) {
        return WiFi.RSSI(a) > WiFi.RSSI(b);
    });

    _scannedCount = min<size_t>((size_t)count, kMaxScannedAps);
    for (size_t i = 0; i < _scannedCount; ++i) {
        int idx = indices[i];
        String s = WiFi.SSID(idx);
        if (s.length() == 0) s = "<HIDDEN>";
        std::strncpy(_scannedAps[i].ssid, s.c_str(), sizeof(_scannedAps[i].ssid) - 1);
        _scannedAps[i].ssid[sizeof(_scannedAps[i].ssid) - 1] = '\0';
        _scannedAps[i].rssi          = WiFi.RSSI(idx);
        _scannedAps[i].channel       = WiFi.channel(idx);
        _scannedAps[i].authMode      = (uint8_t)WiFi.encryptionType(idx);
        _scannedAps[i].isConnectedAp = (isConnected() && s == currentSsid);
    }

    WiFi.scanDelete();
    _lastScanCompleteMs = millis();
    UI.invalidateBody();
}

const char* WifiManager::authModeName(uint8_t authMode) {
    switch ((wifi_auth_mode_t)authMode) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA2";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA3";
        default:                        return "SEC";
    }
}

void WifiManager::resetPingStats() {
    bool busy = _ping.inProgress;
    _ping = PingResult{};
    _ping.inProgress = busy;
}

void WifiManager::triggerPing(const char* host) {
    if (_ping.inProgress) return;
    if (!isConnected()) {
        std::strncpy(_ping.statusMsg, "NO WIFI LINK", sizeof(_ping.statusMsg) - 1);
        _ping.lastSuccess = false;
        return;
    }
    std::strncpy(_pingTarget, host, sizeof(_pingTarget) - 1);
    _pingTarget[sizeof(_pingTarget) - 1] = '\0';
    _ping.inProgress = true;
    std::strncpy(_ping.statusMsg, "PROBING...", sizeof(_ping.statusMsg) - 1);
    _lastPingTrigger = millis();

    xTaskCreatePinnedToCore(
        pingTaskEntry,
        "neon_ping",
        4096,
        this,
        1,
        nullptr,
        0
    );
}

void WifiManager::pingTaskEntry(void* param) {
    WifiManager* self = static_cast<WifiManager*>(param);
    self->executeSinglePing();
    vTaskDelete(nullptr);
}

void WifiManager::executeSinglePing() {
    IPAddress targetIp;
    if (!WiFi.hostByName(_pingTarget, targetIp)) {
        std::strncpy(_ping.resolvedIp, "DNS FAIL", sizeof(_ping.resolvedIp) - 1);
        std::strncpy(_ping.statusMsg, "HOST UNREACHABLE", sizeof(_ping.statusMsg) - 1);
        _ping.sentCount++;
        _ping.lastSuccess = false;
        _ping.historyMs[_ping.historyIdx] = 0;
        _ping.historyIdx = (_ping.historyIdx + 1) % 24;
        _ping.inProgress = false;
        return;
    }

    String ipStr = targetIp.toString();
    std::strncpy(_ping.resolvedIp, ipStr.c_str(), sizeof(_ping.resolvedIp) - 1);

    uint32_t startUs = micros();
    bool success = false;
    uint16_t rttMs = 0;

    // Strategy 1: Raw ICMP Echo Request
    int s = lwip_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (s >= 0) {
        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 200000;
        lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_in to{};
        to.sin_len    = sizeof(to);
        to.sin_family = AF_INET;
        to.sin_addr.s_addr = (uint32_t)targetIp;

        uint8_t pkt[sizeof(IcmpEchoHdr) + 16];
        IcmpEchoHdr* echo = reinterpret_cast<IcmpEchoHdr*>(pkt);
        echo->type   = 8;
        echo->code   = 0;
        echo->chksum = 0;
        echo->id     = htons(0x4E44);
        echo->seqno  = htons(_ping.sentCount + 1);
        for (size_t i = sizeof(IcmpEchoHdr); i < sizeof(pkt); ++i) {
            pkt[i] = (uint8_t)i;
        }
        echo->chksum = inet_chksum(pkt, sizeof(pkt));

        startUs = micros();
        if (lwip_sendto(s, pkt, sizeof(pkt), 0, (struct sockaddr*)&to, sizeof(to)) > 0) {
            uint8_t rxBuf[64];
            struct sockaddr_in from{};
            socklen_t fromLen = sizeof(from);
            int rxLen = lwip_recvfrom(s, rxBuf, sizeof(rxBuf), 0, (struct sockaddr*)&from, &fromLen);
            if (rxLen >= 20) {
                uint32_t elapsedUs = micros() - startUs;
                rttMs = (uint16_t)max<uint32_t>(1, (elapsedUs + 500) / 1000);
                success = true;
            }
        }
        lwip_close(s);
    }

    // Strategy 2: Non-blocking TCP SYN probe fallback
    if (!success) {
        static const uint16_t kPorts[] = {80, 443, 22, 53};
        for (uint16_t port : kPorts) {
            int ts = lwip_socket(AF_INET, SOCK_STREAM, 0);
            if (ts < 0) continue;

            int flags = lwip_fcntl(ts, F_GETFL, 0);
            lwip_fcntl(ts, F_SETFL, flags | O_NONBLOCK);

            struct sockaddr_in addr{};
            addr.sin_len    = sizeof(addr);
            addr.sin_family = AF_INET;
            addr.sin_port   = htons(port);
            addr.sin_addr.s_addr = (uint32_t)targetIp;

            startUs = micros();
            int rc = lwip_connect(ts, (struct sockaddr*)&addr, sizeof(addr));
            if (rc == 0) {
                success = true;
            } else if (errno == EINPROGRESS) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(ts, &wfds);
                struct timeval tv{0, 450000};
                int sel = lwip_select(ts + 1, nullptr, &wfds, nullptr, &tv);
                if (sel > 0) {
                    int soErr = 0;
                    socklen_t len = sizeof(soErr);
                    lwip_getsockopt(ts, SOL_SOCKET, SO_ERROR, &soErr, &len);
                    if (soErr == 0 || soErr == ECONNREFUSED) {
                        success = true;
                    }
                }
            }
            uint32_t elapsedUs = micros() - startUs;
            lwip_close(ts);
            if (success) {
                rttMs = (uint16_t)max<uint32_t>(1, (elapsedUs + 500) / 1000);
                break;
            }
        }
    }

    _ping.sentCount++;
    _ping.lastSuccess = success;
    if (success) {
        _ping.recvCount++;
        _ping.lastRttMs = rttMs;
        if (_ping.recvCount == 1 || rttMs < _ping.minRttMs) _ping.minRttMs = rttMs;
        if (rttMs > _ping.maxRttMs) _ping.maxRttMs = rttMs;
        _ping.avgRttMs = (uint16_t)(((_ping.avgRttMs * (_ping.recvCount - 1)) + rttMs) / _ping.recvCount);
        snprintf(_ping.statusMsg, sizeof(_ping.statusMsg), "ONLINE // %u ms", rttMs);
        _ping.historyMs[_ping.historyIdx] = rttMs;
    } else {
        _ping.lastRttMs = 0;
        std::strncpy(_ping.statusMsg, "TIMEOUT / NO REPLY", sizeof(_ping.statusMsg) - 1);
        _ping.historyMs[_ping.historyIdx] = 0;
    }
    _ping.historyIdx = (_ping.historyIdx + 1) % 24;
    _ping.inProgress = false;
}

void WifiManager::setWifiEpoch(uint32_t unixEpochUtc) {
    if (unixEpochUtc <= 1700000000UL) return;
    struct timeval tv{ (time_t)unixEpochUtc, 0 };
    settimeofday(&tv, nullptr);
    _bootEpochBase    = unixEpochUtc;
    _bootMillisAtBase = millis();
    _ntpSynced        = true;
    _lastEpochSaveMs  = _bootMillisAtBase;
    Settings.saveLastEpoch(unixEpochUtc);
}

void WifiManager::updateTimeSync() {
    uint32_t now = millis();
    if (isConnected() && !_ntpConfigured) {
        configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
        ensureWebServerStarted();
        _ntpConfigured = true;
    }

    if ((now - _lastNtpCheckMs) >= 2000) {
        _lastNtpCheckMs = now;
        time_t rawNow = time(nullptr);
        if (rawNow > 1700000000L) {
            _ntpSynced = true;
            _bootEpochBase    = (uint32_t)rawNow;
            _bootMillisAtBase = now;
            if (_lastEpochSaveMs == 0 || (now - _lastEpochSaveMs) >= 1800000UL) {
                _lastEpochSaveMs = now;
                Settings.saveLastEpoch(_bootEpochBase);
            }
        }
    }
}

uint32_t WifiManager::currentEpoch() const {
    time_t rawNow = time(nullptr);
    if (rawNow > 1700000000L) {
        return (uint32_t)rawNow;
    }
    if (_bootEpochBase > 1700000000UL) {
        uint32_t elapsedSec = (millis() - _bootMillisAtBase) / 1000UL;
        return _bootEpochBase + elapsedSec;
    }
    return millis() / 1000UL;
}

void WifiManager::getFormattedTime(char* hmsOut, size_t hmsLen, char* dateOut, size_t dateLen) const {
    uint32_t utcEp = currentEpoch();
    if (utcEp <= 1700000000UL) {
        if (hmsOut && hmsLen > 0) {
            snprintf(hmsOut, hmsLen, "--:--:--");
        }
        if (dateOut && dateLen > 0) {
            snprintf(dateOut, dateLen, "WAIT WIFI SYNC");
        }
        return;
    }

    time_t ep = (time_t)utcEp + ((long)Settings.get().timezoneHours * 3600L);
    struct tm tInfo{};
    gmtime_r(&ep, &tInfo);
    if (hmsOut && hmsLen > 0) {
        snprintf(hmsOut, hmsLen, "%02d:%02d:%02d", tInfo.tm_hour, tInfo.tm_min, tInfo.tm_sec);
    }
    if (dateOut && dateLen > 0) {
        static const char* kDays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        snprintf(dateOut, dateLen, "%s %04d-%02d-%02d",
                 kDays[tInfo.tm_wday % 7],
                 tInfo.tm_year + 1900, tInfo.tm_mon + 1, tInfo.tm_mday);
    }
}

void WifiManager::updateBatteryReading() {
#if BATTERY_SENSE_ENABLED
    uint32_t now = millis();
    if (_lastBatteryReadMs != 0 && (now - _lastBatteryReadMs) < 3000) {
        return;
    }
    _lastBatteryReadMs = now;

    uint32_t mvSum = 0;
    for (int i = 0; i < 4; ++i) {
        mvSum += analogReadMilliVolts(PIN_BAT_ADC);
    }
    float pinVolts = (mvSum * 0.25f) * 0.001f;
    float vBat = pinVolts * 2.0f;

    if (vBat >= 3.05f && vBat <= 4.45f) {
        _batteryValid = true;
        _batteryVolts = (_batteryVolts == 0.0f) ? vBat : (_batteryVolts * 0.7f + vBat * 0.3f);
        float pct = (_batteryVolts - 3.30f) / (4.18f - 3.30f) * 100.0f;
        _batteryPct = (uint8_t)constrain((int)pct, 0, 100);
    } else {
        _batteryValid = false;
    }
#endif
}

void WifiManager::update() {
    if (_webServerRunning) {
        _server.handleClient();
    }

    if (_sniffer.active) {
        updateSnifferMetrics();
        return;
    }

    if (_connecting && isConnected()) {
        _connecting = false;
        ensureWebServerStarted();
    } else if (_connecting && (millis() - _connAttemptStart) > 15000UL) {
        _connecting = false;
    }

    if (_scanInProgress) {
        collectScanResults();
    }

    if (_continuousPing && !_ping.inProgress && (millis() - _lastPingTrigger) >= 1800UL) {
        triggerPing(Settings.get().pingHost);
    }

    updateTimeSync();
    updateBatteryReading();
}
