/**
 * @file       TinyGsmClientUBLOX.h
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMCLIENTSARAG450_H_
#define SRC_TINYGSMCLIENTSARAG450_H_
// #pragma message("TinyGSM:  TinyGsmClientSaraG450")

// #define TINY_GSM_DEBUG Serial

#define TINY_GSM_MUX_COUNT 7

#include "TinyGsmCommon.h"

#define GSM_NL "\r\n"
static const char GSM_OK[] TINY_GSM_PROGMEM        = "OK" GSM_NL;
static const char GSM_ERROR[] TINY_GSM_PROGMEM     = "ERROR" GSM_NL;
static const char GSM_CME_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CME ERROR:";

enum RegStatus {
  REG_NO_RESULT    = -1,
  REG_UNREGISTERED = 0,
  REG_SEARCHING    = 2,
  REG_DENIED       = 3,
  REG_OK_HOME      = 1,
  REG_OK_ROAMING   = 5,
  REG_UNKNOWN      = 4,
};

class TinyGsmSaraG450
    : public TinyGsmModem<TinyGsmSaraG450, READ_AND_CHECK_SIZE,
                          TINY_GSM_MUX_COUNT> {
  friend class TinyGsmModem<TinyGsmSaraG450, READ_AND_CHECK_SIZE,
                            TINY_GSM_MUX_COUNT>;


  /*
   * Inner Client
   */
 public:
  class GsmClientSaraG450 : public GsmClient {
    friend class TinyGsmSaraG450;

   public:
    GsmClientSaraG450() {}

    explicit GsmClientSaraG450(TinyGsmSaraG450& modem, uint8_t mux = 0) {
      init(&modem, mux);
    }

    bool init(TinyGsmSaraG450* modem, uint8_t mux = 0) {
      this->at       = modem;
      this->mux      = mux;
      sock_available = 0;
      prev_check     = 0;
      sock_connected = false;
      got_data       = false;

      at->sockets[mux] = this;

      return true;
    }

   public:
    int connect(const char* host, uint16_t port, int timeout_s) {
      stop();
      TINY_GSM_YIELD();
      rx.clear();

      uint8_t oldMux = mux;
      sock_connected = at->modemConnect(host, port, &mux, false, timeout_s);
      if (mux != oldMux) {
        DBG("WARNING:  Mux number changed from", oldMux, "to", mux);
        at->sockets[oldMux] = NULL;
      }
      at->sockets[mux] = this;
      at->maintain();

      return sock_connected;
    }
    int connect(IPAddress ip, uint16_t port, int timeout_s) {
      return connect(TinyGsmStringFromIp(ip).c_str(), port, timeout_s);
    }
    int connect(const char* host, uint16_t port) override {
      return connect(host, port, 75);
    }
    int connect(IPAddress ip, uint16_t port) override {
      return connect(ip, port, 75);
    }

    void stop(uint32_t maxWaitMs) {
      dumpModemBuffer(maxWaitMs);
      at->modemDisconnect(mux);
    }
    void stop() override {
      stop(15000L);
    }

    /*
     * Extended API
     */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;
  };

  /*
   * Inner Secure Client
   */
 public:
  class GsmClientSecureSaraG450 : public GsmClientSaraG450 {
   public:
    GsmClientSecureSaraG450() {}

    explicit GsmClientSecureSaraG450(TinyGsmSaraG450& modem, uint8_t mux = 1)
        : GsmClientSaraG450(modem, mux) {}

   public:
    int connect(const char* host, uint16_t port, int timeout_s) {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      uint8_t oldMux = mux;
      sock_connected = at->modemConnect(host, port, &mux, true, timeout_s);
      if (mux != oldMux) {
        DBG("WARNING:  Mux number changed from", oldMux, "to", mux);
        at->sockets[oldMux] = NULL;
      }
      at->sockets[mux] = this;
      at->maintain();
      return sock_connected;
    }
  };

  /*
   * Constructor
   */
 public:
  explicit TinyGsmSaraG450(Stream& stream) : stream(stream) {
    memset(sockets, 0, sizeof(sockets));
  }

  /*
   * Basic functions
   */
 protected:
  bool initImpl(const char* pin = NULL) {
    DBG(GF("### TinyGSM Version:"), TINYGSM_VERSION);

    powerOnModule();

    if (!testAT()) { return false; }

    delay(1000);
    streamClear();

    sendAT(GF("E0"));  // Echo Off
    if (waitResponse() != 1) { return false; }
    sendAT(GF(
        "&W"));  // Echo Off sometimes only takes affect after we force a save
    if (waitResponse(30000) != 1) {  // Takes long to save
      return false;
    }
    // Echo off takes a while to be effective
    delay(200);
    streamClear();

#ifdef TINY_GSM_DEBUG
    sendAT(GF("+CMEE=2"));  // turn on verbose error codes
#else
    sendAT(GF("+CMEE=0"));  // turn off error codes
#endif
    if (waitResponse() != 1) { return false; }

    DBG(GF("### Modem:"), getModemName());

    // Enable automatic time zome update
    sendAT(GF("+CTZU=1"));
    if (waitResponse(10000L) != 1) { return false; }

    int ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pin != NULL && strlen(pin) > 0) {
      simUnlock(pin);
      return (getSimStatus() == SIM_READY);
    } else {
      // if the sim is ready, or it's locked but no pin has been provided,
      // return true
      return (ret == SIM_READY || ret == SIM_LOCKED);
    }
  }

  void powerOffModuleHard() {
    // TODO(jpmeijers):  This should probabaly take a pin as an imput argument
    // instead of requireing a pre-processor define
#if defined(TINY_GSM_PIN_POWER_OFF)
    pinMode(TINY_GSM_PIN_POWER_OFF, OUTPUT);
    digitalWrite(TINY_GSM_PIN_POWER_OFF, LOW);
    delay(1000);
    digitalWrite(TINY_GSM_PIN_POWER_OFF, HIGH);
    pinMode(TINY_GSM_PIN_POWER_OFF, INPUT);
#else
#error "Please define TINY_GSM_PIN_POWER_OFF"
#endif
  }

  void powerOnModule() {
    // TODO(jpmeijers):  This should probabaly take a pin as an imput argument
    // instead of requireing a pre-processor define
#if defined(TINY_GSM_PIN_POWER_ON)
    pinMode(TINY_GSM_PIN_POWER_ON, OUTPUT);
    digitalWrite(TINY_GSM_PIN_POWER_ON, LOW);
    delay(1000);
    digitalWrite(TINY_GSM_PIN_POWER_ON, HIGH);
    pinMode(TINY_GSM_PIN_POWER_ON, INPUT);
#else
#error "Please define TINY_GSM_PIN_POWER_ON"
#endif
  }

  bool testATImpl(uint32_t timeout_ms = 10000L) {
    delay(1000);
    for (uint32_t start = millis(); millis() - start < timeout_ms;) {
      sendAT(GF(""));
      if (waitResponse(500) == 1) { return true; }
      delay(200);
    }
    return false;
  }

  bool thisHasSSL() {
    return true;
  }

  bool thisHasWifi() {
    return false;
  }

  bool thisHasGPRS() {
    return true;
  }

  /*
   * Power functions
   */
 protected:
  bool softRestart() {
    if (!testAT()) { return false; }
    sendAT(GF("+CFUN=16"));
    if (waitResponse(10000L) != 1) { return false; }
    delay(3000);  // TODO(jpmeijers):  Verify delay timing here
    return init();
  }

  bool hardRestart() {
    powerOffModuleHard();
    delay(3000);  // TODO(jpmeijers):  Verify delay timing here
    return init();
  }

  bool powerOffImpl() {
    sendAT(GF("+CPWROFF"));
    return waitResponse(40000L) == 1;
  }

  bool sleepEnableImpl(bool enable = true) TINY_GSM_ATTR_NOT_IMPLEMENTED;

  /*
   * SIM card functions
   */
 protected:
  String getIMEIImpl() {
    sendAT(GF("+CGSN"));
    // if (waitResponse(GF(GSM_NL)) != 1) {
    //   return "";
    // }
    String res = "";
    for (int i = 0; res == "" && i < 3; i++) {
      res = stream.readStringUntil('\n');
      res.trim();
    }
    waitResponse();
    // res.trim();
    return res;
  }

  /*
   * Generic network functions
   */
 public:
  RegStatus getRegistrationStatus() {
    return getGprsRegistrationStatus();
  }

  RegStatus getGsmRegistrationStatus() {
    return (RegStatus)getRegistrationStatusXREG("CREG");
  }

  RegStatus getGprsRegistrationStatus() {
    return (RegStatus)getRegistrationStatusXREG("CGREG");
  }

  bool isNetworkGsmConnected() {
    RegStatus s = getGsmRegistrationStatus();
    if (s == REG_OK_HOME || s == REG_OK_ROAMING)
      return true;
    else
      return false;
  }

  bool isNetworkGprsConnected() {
    RegStatus s = getGprsRegistrationStatus();
    if (s == REG_OK_HOME || s == REG_OK_ROAMING)
      return true;
    else
      return false;
  }

  bool waitForNetworkGsm(uint32_t timeout_ms = 60000L) {
    for (uint32_t start = millis(); millis() - start < timeout_ms;) {
      if (isNetworkGsmConnected()) { return true; }
      delay(1000);
    }
    return false;
  }

  bool waitForNetworkGprs(uint32_t timeout_ms = 60000L) {
    for (uint32_t start = millis(); millis() - start < timeout_ms;) {
      if (isNetworkGprsConnected()) { return true; }
      delay(1000);
    }
    return false;
  }

 protected:
  bool isNetworkConnectedImpl() {
    return isNetworkGprsConnected();
  }

  /*
   * GPRS functions
   */
 protected:
  bool gprsAttach() {
    sendAT(GF("+CGATT=1"));  // attach to GPRS
    if (waitResponse(360000, GF(GSM_NL "+CGATT:")) != 1) { return false; }
    int status = stream.readStringUntil('\n').toInt();
    waitResponse();
    if (status == 1)
      return true;
    else
      return false;
  }

  RegStatus getGprsAttachedStatus() {
    sendAT(GF("+CGATT?"));
    if (waitResponse(GF(GSM_NL "+CGATT:")) != 1) { return REG_UNKNOWN; }
    // streamSkipUntil(','); /* Skip format (0) */
    int status = stream.readStringUntil('\n').toInt();
    waitResponse();
    return (RegStatus)status;
  }

  bool isGprsAttached() {
    RegStatus s = getGprsRegistrationStatus();
    if (s == 1) {
      return true;
    } else {
      return false;
    }
  }

  bool waitForGprsAttached(uint32_t timeout_ms = 60000L) {
    for (uint32_t start = millis(); millis() - start < timeout_ms;) {
      if (isGprsAttached()) { return true; }
      delay(1000);
    }
    return false;
  }

 protected:
  bool gprsConnectImpl(const char* apn, const char* user = NULL,
                       const char* pwd = NULL) {
    gprsDisconnect();

    DBG("Connecting GPRS");

    /*
    Profile might already be active
    */
    {
      sendAT(GF("+UPSND=0,8"));  // Check if PSD profile 0 is now active
      int res = waitResponse(GF(",8,1"), GF(",8,0"));
      waitResponse();  // Should return another OK
      if (res == 1) {
        return true;  // It's active
      }
    }

    // sendAT(GF("+UPSD=0"));
    // waitResponse();

    // Setting up the PSD profile/PDP context with the UPSD commands sets up an
    // "internal" PDP context, i.e. a data connection using the internal IP
    // stack and related AT commands for sockets.

    // Packet switched data configuration
    // AT+UPSD=<profile_id>,<param_tag>,<param_val>
    // profile_id = 0 - PSD profile identifier, in range 0-6 (NOT PDP context)
    // param_tag = 1: APN
    // param_tag = 2: username
    // param_tag = 3: password
    // param_tag = 7: IP address Note: IP address set as "0.0.0.0" means
    //    dynamic IP address assigned during PDP context activation
    sendAT(GF("+UPSD=0,1,\""), apn, '"');  // Set APN for PSD profile 0
    waitResponse();

    if (user && strlen(user) > 0) {
      sendAT(GF("+UPSD=0,2,\""), user, '"');  // Set user for PSD profile 0
      waitResponse();
    }
    if (pwd && strlen(pwd) > 0) {
      sendAT(GF("+UPSD=0,3,\""), pwd, '"');  // Set password for PSD profile 0
      waitResponse();
    }

    sendAT(GF("+UPSD=0,7,\"0.0.0.0\""));  // Dynamic IP on PSD profile 0
    waitResponse();

    // AT+UPSDA=0,3 returns an error. Try a delay
    delay(1000);

    // Packet switched data action
    // AT+UPSDA=<profile_id>,<action>
    // profile_id = 0: PSD profile identifier, in range 0-6 (NOT PDP context)
    // action = 3: activate; it activates a PDP context with the specified
    // profile, using the current parameters
    sendAT(GF(
        "+UPSDA=0,3"));  // Activate the PDP context associated with profile 0
    if (waitResponse(360000L) != 1) {  // Should return ok
      return false;
    }

    // Packet switched network-assigned data - Returns the current (dynamic)
    // network-assigned or network-negotiated value of the specified parameter
    // for the active PDP context associated with the specified PSD profile.
    // AT+UPSND=<profile_id>,<param_tag>
    // profile_id = 0: PSD profile identifier, in range 0-6 (NOT PDP context)
    // param_tag = 8: PSD profile status: if the profile is active the return
    // value is 1, 0 otherwise
    sendAT(GF("+UPSND=0,8"));  // Check if PSD profile 0 is now active
    int res = waitResponse(GF(",8,1"), GF(",8,0"));
    waitResponse();  // Should return another OK
    if (res == 1) {
      return true;          // It's now active
    } else if (res == 2) {  // If it's not active yet, wait for the +UUPSDA URC
      if (waitResponse(180000L, GF("+UUPSDA: 0")) != 1) {  // 0=successful
        // TODO(jpmeijers): Sometimes we get +UUPSDA: 36 which will take 3
        // minutes to timeout
        return false;
      }
      streamSkipUntil('\n');  // Ignore the IP address, if returned
    } else {
      return false;
    }

    return true;
  }

  bool gprsDisconnectImpl() {
    DBG("Disconnecting GPRS");
    sendAT(GF(
        "+UPSDA=0,4"));  // Deactivate the PDP context associated with profile 0
    if (waitResponse() != 1) {  // Wait for OK
      return true;              // if we get an error we are disconnected
    }

    if (waitResponse(30000, GF("+UUPSDD: 0")) == 1) { return true; }

    // sendAT(GF("+CGATT=0"));  // detach from GPRS
    // if (waitResponse(360000L) != 1) {
    //   return false;
    // }

    return false;
  }

  /*
   * IP Address functions
   */
 protected:
  String getLocalIPImpl() {
    sendAT(GF("+UPSND=0,0"));
    if (waitResponse(GF(GSM_NL "+UPSND:")) != 1) { return ""; }
    streamSkipUntil(',');   // Skip PSD profile
    streamSkipUntil('\"');  // Skip request type
    String res = stream.readStringUntil('\"');
    if (waitResponse() != 1) { return ""; }
    return res;
  }

  /*
   * Phone Call functions
   */
 protected:
  // Can follow all of the phone call functions from the template

  /*
   * Messaging functions
   */
 protected:
  // Can follow all template functions

  /*
   * Location functions
   */
 protected:
  String getGsmLocationImpl() {
    sendAT(GF("+ULOC=2,3,0,120,1"));
    if (waitResponse(30000L, GF(GSM_NL "+UULOC:")) != 1) { return ""; }
    String res = stream.readStringUntil('\n');
    waitResponse();
    res.trim();
    return res;
  }

  /*
   * GPS location functions
   */
 public:
  // No functions of this type supported

  /*
   * Time functions
   */
 protected:
  // Can follow the standard CCLK function in the template

  /*
   * Battery & temperature functions
   */
 protected:
  uint16_t getBattVoltageImpl() TINY_GSM_ATTR_NOT_AVAILABLE;

  int8_t getBattPercentImpl() {
    sendAT(GF("+CIND?"));
    if (waitResponse(GF(GSM_NL "+CIND:")) != 1) { return 0; }

    int    res     = stream.readStringUntil(',').toInt();
    int8_t percent = res * 20;  // return is 0-5
    // Wait for final OK
    waitResponse();
    return percent;
  }

  uint8_t getBattChargeStateImpl() TINY_GSM_ATTR_NOT_AVAILABLE;

  bool getBattStatsImpl(uint8_t& chargeState, int8_t& percent,
                        uint16_t& milliVolts) {
    chargeState = 0;
    percent     = getBattPercent();
    milliVolts  = 0;
    return true;
  }

  // This would only available for a small number of modules in this group
  // (TOBY-L)
  float getTemperatureImpl() TINY_GSM_ATTR_NOT_IMPLEMENTED;

  /*
   * Client related functions
   */
 protected:
  bool modemConnect(const char* host, uint16_t port, uint8_t* mux,
                    bool ssl = false, int timeout_s = 120) {
    uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;
    sendAT(GF("+USOCR=6"));  // create a socket
    if (waitResponse(GF(GSM_NL "+USOCR:")) !=
        1) {  // reply is +USOCR: ## of socket created
      return false;
    }
    *mux = stream.readStringUntil('\n').toInt();
    waitResponse();

    if (ssl) {
      sendAT(GF("+USOSEC="), *mux, ",1");
      waitResponse();
    }

    // Enable NODELAY
    sendAT(GF("+USOSO="), *mux, GF(",6,1,1"));
    waitResponse();

    // Enable KEEPALIVE, 30 sec
    // sendAT(GF("+USOSO="), *mux, GF(",6,2,30000"));
    // waitResponse();

    // connect on the allocated socket
    sendAT(GF("+USOCO="), *mux, ",\"", host, "\",", port);
    int rsp = waitResponse(timeout_ms);
    waitResponse();
    // After a connect we can't immediately write data or else we get:
    // AT+USOWR=0,3
    // CME ERROR: Operation not allowed
    delay(200);  // Give the connection 200ms to settle

    return (1 == rsp);
  }

  bool modemDisconnect(uint8_t mux) {
    TINY_GSM_YIELD();
    if (!modemGetConnected(mux)) {
      sockets[mux]->sock_connected = false;
      return true;
    }
    bool success;
    sendAT(GF("+USOCL="), mux);
    success = 1 == waitResponse();  // should return within 1s
    if (success) { sockets[mux]->sock_connected = false; }
    return success;
  }

  int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
    sendAT(GF("+USOWR="), mux, ',', (uint16_t)len);
    if (waitResponse(GF("@")) != 1) { return 0; }
    // 50ms delay, see AT manual section 25.10.4
    delay(50);
    stream.write(reinterpret_cast<const uint8_t*>(buff), len);
    stream.flush();
    if (waitResponse(GF(GSM_NL "+USOWR:")) != 1) { return 0; }
    streamSkipUntil(',');  // Skip mux
    int sent = stream.readStringUntil('\n').toInt();
    waitResponse();  // sends back OK after the confirmation of number sent
    return sent;
  }

  size_t modemRead(size_t size, uint8_t mux) {
    sendAT(GF("+USORD="), mux, ',', (uint16_t)size);
    if (waitResponse(GF(GSM_NL "+USORD:")) != 1) {
      // Might end in an error because the socket is closed
      sockets[mux]->sock_connected = modemGetConnected(mux);
      return 0;
    }
    streamSkipUntil(',');  // Skip mux
    int len = stream.readStringUntil(',').toInt();
    streamSkipUntil('\"');

    for (int i = 0; i < len; i++) { moveCharFromStreamToFifo(mux); }
    streamSkipUntil('\"');
    waitResponse();
    DBG("### READ:", len, "from", mux);
    sockets[mux]->sock_available = modemGetAvailable(mux);
    return len;
  }

  size_t modemGetAvailable(uint8_t mux) {
    // NOTE:  Querying a closed socket gives an error "operation not allowed"
    sendAT(GF("+USORD="), mux, ",0");
    size_t  result = 0;
    uint8_t res    = waitResponse(GF(GSM_NL "+USORD:"));
    // Will give error "operation not allowed" when attempting to read a socket
    // that you have already told to close
    if (res == 1) {
      streamSkipUntil(',');  // Skip mux
      result = stream.readStringUntil('\n').toInt();
      // if (result) DBG("### DATA AVAILABLE:", result, "on", mux);
      waitResponse();
    } else {
      // We received an error checking number of bytes to read
      DBG("modemGetAvailable error");
      sockets[mux]->sock_connected =
          false;  // disconnect to prevent infinite loops
      return 0;
    }
    if (!result && sockets[mux]->sock_connected) {
      sockets[mux]->sock_connected = modemGetConnected(mux);
    }
    return result;
  }

  bool modemGetConnected(uint8_t mux) {
    // NOTE:  Querying a closed socket gives an error "operation not allowed"
    sendAT(GF("+USOCTL="), mux, ",10");
    uint8_t res = waitResponse(GF(GSM_NL "+USOCTL:"), GF("+CME ERROR:"));
    if (res == 1) {
      // Valid response, need to read state
    } else if (res == 2) {
      // +CME ERROR: Operation not allowed - means socket is not connected
      stream.readStringUntil(
          '\n');  // read the rest of the error string to purge the buffer
      return false;
    } else {
      // Unknown response
      DBG("Socket connection state error");
      return false;
    }

    streamSkipUntil(',');  // Skip mux
    streamSkipUntil(',');  // Skip type
    int result = stream.readStringUntil('\n').toInt();
    // 0: the socket is in INACTIVE status (it corresponds to CLOSED status
    // defined in RFC793 "TCP Protocol Specification" [112])
    // 1: the socket is in LISTEN status
    // 2: the socket is in SYN_SENT status
    // 3: the socket is in SYN_RCVD status
    // 4: the socket is in ESTABILISHED status
    // 5: the socket is in FIN_WAIT_1 status
    // 6: the socket is in FIN_WAIT_2 status
    // 7: the sokcet is in CLOSE_WAIT status
    // 8: the socket is in CLOSING status
    // 9: the socket is in LAST_ACK status
    // 10: the socket is in TIME_WAIT status
    waitResponse();
    return (result != 0);
  }

  /*
   * Utilities
   */
 public:
  // TODO(vshymanskyy): Optimize this!
  uint8_t waitResponse(uint32_t timeout_ms, String& data,
                       GsmConstStr r1 = GFP(GSM_OK),
                       GsmConstStr r2 = GFP(GSM_ERROR),
                       GsmConstStr r3 = GFP(GSM_CME_ERROR),
                       GsmConstStr r4 = NULL, GsmConstStr r5 = NULL) {
    /*String r1s(r1); r1s.trim();
    String r2s(r2); r2s.trim();
    String r3s(r3); r3s.trim();
    String r4s(r4); r4s.trim();
    String r5s(r5); r5s.trim();
    DBG("### ..:", r1s, ",", r2s, ",", r3s, ",", r4s, ",", r5s);*/
    data.reserve(64);
    int      index       = 0;
    uint32_t startMillis = millis();
    do {
      TINY_GSM_YIELD();
      while (stream.available() > 0) {
        TINY_GSM_YIELD();
        int a = stream.read();
        if (a <= 0) continue;  // Skip 0x00 bytes, just in case
        data += static_cast<char>(a);
        if (r1 && data.endsWith(r1)) {
          index = 1;
          goto finish;
        } else if (r2 && data.endsWith(r2)) {
          index = 2;
          goto finish;
        } else if (r3 && data.endsWith(r3)) {
          index = 3;
          if (r3 == GFP(GSM_CME_ERROR)) {
            streamSkipUntil('\n');  // Read out the error
          }
          goto finish;
        } else if (r4 && data.endsWith(r4)) {
          index = 4;
          goto finish;
        } else if (r5 && data.endsWith(r5)) {
          index = 5;
          goto finish;
        } else if (data.endsWith(GF("+UUSORD:"))) {
          int mux = stream.readStringUntil(',').toInt();
          int len = stream.readStringUntil('\n').toInt();
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            sockets[mux]->got_data       = true;
            sockets[mux]->sock_available = len;
          }
          data = "";
          DBG("### URC Data Received:", len, "on", mux);
        } else if (data.endsWith(GF("+UUSOCL:"))) {
          int mux = stream.readStringUntil('\n').toInt();
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            sockets[mux]->sock_connected = false;
          }
          data = "";
          DBG("### URC Sock Closed: ", mux);
        }
      }
    } while (millis() - startMillis < timeout_ms);
  finish:
    if (!index) {
      data.trim();
      if (data.length()) { DBG("### Unhandled:", data); }
      data = "";
    }
    // data.replace(GSM_NL, "/");
    // DBG('<', index, '>', data);
    return index;
  }

  uint8_t waitResponse(uint32_t timeout_ms, GsmConstStr r1 = GFP(GSM_OK),
                       GsmConstStr r2 = GFP(GSM_ERROR),
                       GsmConstStr r3 = GFP(GSM_CME_ERROR),
                       GsmConstStr r4 = NULL, GsmConstStr r5 = NULL) {
    String data;
    return waitResponse(timeout_ms, data, r1, r2, r3, r4, r5);
  }

  uint8_t waitResponse(GsmConstStr r1 = GFP(GSM_OK),
                       GsmConstStr r2 = GFP(GSM_ERROR),
                       GsmConstStr r3 = GFP(GSM_CME_ERROR),
                       GsmConstStr r4 = NULL, GsmConstStr r5 = NULL) {
    return waitResponse(1000, r1, r2, r3, r4, r5);
  }

 protected:
  Stream&            stream;
  GsmClientSaraG450* sockets[TINY_GSM_MUX_COUNT];
  const char*        gsmNL = GSM_NL;
};

#endif  // SRC_TINYGSMCLIENTSARAG450_H_
