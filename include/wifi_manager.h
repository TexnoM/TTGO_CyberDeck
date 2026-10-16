#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "hardware_config.h"

struct ScannedApInfo {
    char    ssid[33];
    int32_t rssi;
    int32_t channel;
    uint8_t authMode;
    bool    isConnectedAp;
};

struct PingResult {
    bool     inProgress  = false;
    bool     lastSuccess = false;
    uint16_t lastRttMs   = 0;
    uint16_t minRttMs    = 0;
    uint16_t maxRttMs    = 0;
    uint16_t avgRttMs    = 0;
    uint16_t sentCount   = 0;
    uint16_t recvCount   = 0;
    uint16_t historyMs[24] = {0};
    uint8_t  historyIdx  = 0;
    char     resolvedIp[20] = "---";
    char     statusMsg[28]  = "READY // PRESS B";
};

struct SnifferStats {
    bool     active           = false;
    bool     hopping          = true;
    uint8_t  currentChannel   = 1;
    uint32_t pktPerSec        = 0;
    uint32_t mgmtPerSec       = 0;
    uint32_t dataPerSec       = 0;
    uint32_t totalCaptured    = 0;
    int32_t  lastRssi         = -95;
    uint16_t channelPkts[13]  = {0}; // Activity per channel 1..13
    uint16_t historyPps[32]   = {0}; // 32-sample oscilloscope history
    uint8_t  historyIdx       = 0;
};

class WifiManager {
public:
    static constexpr size_t kMaxScannedAps = 16;

    void begin();
    void update();

    // Connection control
    void connectStoredWifi();
    void disconnectWifi();
    bool isConnected() const;
    String localIp() const;
    String gatewayIp() const;
    String subnetMask() const;
    String dnsIp() const;
    String macAddress() const;
    String connectedSsid() const;
    int32_t currentRssi() const;

    // Captive Configuration AP & Remote Web Console
    void startConfigPortal();
    void stopConfigPortal();
    bool isConfigPortalActive() const { return _apPortalActive; }
    bool isWebServerRunning() const { return _webServerRunning; }
    const char* portalSsid() const { return _apSsid; }
    String portalIp() const;
    uint8_t portalClientCount() const;
    uint32_t webRequestCount() const { return _webRequests; }
    const char* lastRemoteCmd() const { return _lastRemoteCmd; }

    // Non-blocking Wi-Fi Radar Scanner
    bool startAsyncScan();
    bool isScanning() const { return _scanInProgress; }
    size_t scannedCount() const { return _scannedCount; }
    const ScannedApInfo& scannedAp(size_t idx) const { return _scannedAps[idx]; }
    uint32_t lastScanTimeMs() const { return _lastScanCompleteMs; }
    static const char* authModeName(uint8_t authMode);

    // Passive 2.4GHz Promiscuous Packet Sniffer & Channel Spectrum Analyzer
    void startSniffer();
    void stopSniffer();
    void stepSnifferChannel();
    void toggleSnifferHopping();
    const SnifferStats& sniffer() const { return _sniffer; }

    // Non-blocking Host Reachability / Ping Tool (runs on background Core 0 task)
    void triggerPing(const char* host);
    void setContinuousPing(bool enabled) { _continuousPing = enabled; }
    bool isContinuousPing() const { return _continuousPing; }
    const PingResult& pingStats() const { return _ping; }
    void resetPingStats();

    // Real-time Clock & Wi-Fi / NTP Synchronization (no fake fallback dates)
    bool isNtpSynced() const { return _ntpSynced; }
    void setWifiEpoch(uint32_t unixEpochUtc);
    uint32_t currentEpoch() const;
    void getFormattedTime(char* hmsOut, size_t hmsLen, char* dateOut, size_t dateLen) const;

    // Battery sensor check (only shown when real measurement is valid)
    bool hasValidBattery() const { return _batteryValid; }
    float batteryVoltage() const { return _batteryVolts; }
    uint8_t batteryPercent() const { return _batteryPct; }

private:
    WebServer _server{80};
    bool      _apPortalActive    = false;
    bool      _webServerRunning  = false;
    char      _apSsid[24]        = "TTGO_CyberDeck";
    uint32_t  _webRequests       = 0;
    char      _lastRemoteCmd[24] = "NONE";
    uint32_t  _connAttemptStart  = 0;
    bool      _connecting        = false;

    // Radar scan state
    bool          _scanInProgress     = false;
    ScannedApInfo _scannedAps[kMaxScannedAps];
    size_t        _scannedCount       = 0;
    uint32_t      _lastScanCompleteMs = 0;

    // Promiscuous Sniffer state
    SnifferStats _sniffer;
    uint32_t     _lastSnifferSecMs    = 0;
    uint32_t     _lastSnifferHopMs    = 0;

    // Ping worker state
    PingResult _ping;
    bool       _continuousPing   = false;
    uint32_t   _lastPingTrigger  = 0;
    char       _pingTarget[48]   = {0};

    // NTP & Wi-Fi Time state
    bool     _ntpSynced          = false;
    bool     _ntpConfigured      = false;
    uint32_t _bootEpochBase      = 0;
    uint32_t _bootMillisAtBase   = 0;
    uint32_t _lastNtpCheckMs     = 0;
    uint32_t _lastEpochSaveMs    = 0;

    // Battery monitoring state
    bool     _batteryValid       = false;
    float    _batteryVolts       = 0.0f;
    uint8_t  _batteryPct         = 0;
    uint32_t _lastBatteryReadMs  = 0;

    void ensureWebServerStarted();
    void setupWebPortalRoutes();
    void collectScanResults();
    void updateSnifferMetrics();
    void updateTimeSync();
    void updateBatteryReading();
    static void pingTaskEntry(void* param);
    void executeSinglePing();
};

extern WifiManager Net;
