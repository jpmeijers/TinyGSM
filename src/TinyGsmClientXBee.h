/**
 * @file       TinyGsmClientXBee.h
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef TinyGsmClientXBee_h
#define TinyGsmClientXBee_h

// #define TINY_GSM_DEBUG Serial

#include <TinyGsmCommon.h>

enum RegStatus {
  REG_UNREGISTERED = 0,
  REG_SEARCHING    = 2,
  REG_DENIED       = 3,
  REG_OK           = 1,
  REG_UNKNOWN      = 4,
};

enum XBeeType {
  XBEE_CELL  = 0,
  XBEE_WIFI  = 1,
};


//============================================================================//
//============================================================================//
//                   Declaration of the TinyGsmXBee Class
//============================================================================//
//============================================================================//

class TinyGsmXBee : public TinyGSMModem
{

public:

//============================================================================//
//============================================================================//
//                          The XBee Client Class
//============================================================================//
//============================================================================//

class GsmClientXBee : public TinyGSMModem::GsmClientCommon
{
  friend class TinyGsmXBee;

public:
  GsmClientXBee() {}

  GsmClientXBee(TinyGsmXBee& modem, uint8_t mux = 0) {
    init(&modem, mux);
  }

  bool init(TinyGsmXBee* modem, uint8_t mux = 0) {
    this->at = modem;
    this->mux = mux;
    sock_connected = false;

    at->sockets[mux] = this;
    return true;
  }

public:
  virtual int connect(const char *host, uint16_t port) override {
    at->streamClear();  // Empty anything remaining in the buffer;
    bool sock_connected = false;
    if (at->commandMode())  {  // Don't try if we didn't successfully get into command mode
      sock_connected = at->modemConnect(host, port, mux, false);
      at->writeChanges();
      at->exitCommand();
    }
    at->streamClear();  // Empty anything remaining in the buffer;
    return sock_connected;
  }

  virtual int connect(IPAddress ip, uint16_t port) override {
    at->streamClear();  // Empty anything remaining in the buffer;
    bool sock_connected = false;
    if (at->commandMode())  {  // Don't try if we didn't successfully get into command mode
      sock_connected = at->modemConnect(ip, port, mux, false);
      at->writeChanges();
      at->exitCommand();
    }
    at->streamClear();  // Empty anything remaining in the buffer;
    return sock_connected;
  }

  // This is a hack to shut the socket by setting the timeout to zero and
  //  then sending an empty line to the server.
  virtual void stop() override {
    at->streamClear();  // Empty anything remaining in the buffer;
    at->commandMode();
    at->sendAT(GF("TM0"));  // Set socket timeout to 0;
    at->waitResponse();
    at->writeChanges();
    at->exitCommand();
    at->modemSend("", 1, mux);
    at->commandMode();
    at->sendAT(GF("TM64"));  // Set socket timeout back to 10 seconds;
    at->waitResponse();
    at->writeChanges();
    at->exitCommand();
    at->streamClear();  // Empty anything remaining in the buffer;
    sock_connected = false;
  }

  virtual size_t write(const uint8_t *buf, size_t size) override {
    TINY_GSM_YIELD();
    //at->maintain();
    return at->modemSend(buf, size, mux);
  }

  virtual int available() override {
    TINY_GSM_YIELD();
    return at->stream.available();
  }

  virtual int read(uint8_t *buf, size_t size) override {
    TINY_GSM_YIELD();
    return at->stream.readBytes((char*)buf, size);
  }

  virtual int read() override {
    TINY_GSM_YIELD();
    return at->stream.read();
  }

  virtual int peek() override { return at->stream.peek(); }

  /*
   * Extended API
   */

  String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;

private:
  TinyGsmXBee*  at;
};

//============================================================================//
//============================================================================//
//                          The Secure XBee Client Class
//============================================================================//
//============================================================================//


class GsmClientXBeeSecure : public GsmClientXBee
{
public:
  GsmClientXBeeSecure() {}

  GsmClientXBeeSecure(TinyGsmXBee& modem, uint8_t mux = 1)
    : GsmClientXBee(modem, mux)
  {}

public:
  virtual int connect(const char *host, uint16_t port) {
    at->streamClear();  // Empty anything remaining in the buffer;
    bool sock_connected = false;
    if (at->commandMode())  {  // Don't try if we didn't successfully get into command mode
      sock_connected = at->modemConnect(host, port, mux, true);
      at->writeChanges();
      at->exitCommand();
    }
    at->streamClear();  // Empty anything remaining in the buffer;
    return sock_connected;
  }

  virtual int connect(IPAddress ip, uint16_t port) {
    at->streamClear();  // Empty anything remaining in the buffer;
    bool sock_connected = false;
    if (at->commandMode())  {  // Don't try if we didn't successfully get into command mode
      sock_connected = at->modemConnect(ip, port, mux, true);
      at->writeChanges();
      at->exitCommand();
    }
    at->streamClear();  // Empty anything remaining in the buffer;
    return sock_connected;
  }
};


//============================================================================//
//============================================================================//
//                          The XBee Modem Functions
//============================================================================//
//============================================================================//

public:

#ifdef GSM_DEFAULT_STREAM
  TinyGsmXBee(Stream& stream = GSM_DEFAULT_STREAM)
#else
  TinyGsmXBee(Stream& stream)
#endif
  : TinyGSMModem(stream)
  {}

  /*
   * Basic functions
   */

  bool init() {
    guardTime = 1100;  // Start with a default guard time of 1 second

    if (!commandMode(10)) return false;  // Try up to 10 times for the init

    sendAT(GF("AP0"));  // Put in transparent mode
    bool ret_val = waitResponse() == 1;
    ret_val &= writeChanges();

    sendAT(GF("GT64")); // shorten the guard time to 100ms
    ret_val &= waitResponse();
    ret_val &= writeChanges();
    if (ret_val) guardTime = 125;

    sendAT(GF("HS"));  // Get the "Hardware Series"; 0x601 for S6B (Wifi)
    int res = waitResponse(GF("601"));
    if (res == 1) beeType = XBEE_WIFI;
    else beeType = XBEE_CELL;

    exitCommand();
    return ret_val;
  }

  void setBaud(unsigned long baud) {
    if (!commandMode()) return;
    switch(baud)
    {
      case 2400: sendAT(GF("BD1")); break;
      case 4800: sendAT(GF("BD2")); break;
      case 9600: sendAT(GF("BD3")); break;
      case 19200: sendAT(GF("BD4")); break;
      case 38400: sendAT(GF("BD5")); break;
      case 57600: sendAT(GF("BD6")); break;
      case 115200: sendAT(GF("BD7")); break;
      case 230400: sendAT(GF("BD8")); break;
      case 460800: sendAT(GF("BD9")); break;
      case 921600: sendAT(GF("BDA")); break;
      default: {
          DBG(GF("Specified baud rate is unsupported! Setting to 9600 baud."));
          sendAT(GF("BD3")); // Set to default of 9600
          break;
      }
    }
    waitResponse();
    writeChanges();
    exitCommand();
  }

  bool testAT(unsigned long timeout = 10000L) {
    for (unsigned long start = millis(); millis() - start < timeout; ) {
      if (commandMode())
      {
          sendAT();
          if (waitResponse(200) == 1) {
              return true;
          }
          exitCommand();
      }
      delay(100);
    }
    return false;
  }

  void maintain() {}

  bool factoryDefault() {
    if (!commandMode()) return false;  // Return immediately
    sendAT(GF("RE"));
    bool ret_val = waitResponse() == 1;
    ret_val &= writeChanges();
    exitCommand();
    return ret_val;
  }

  bool hasSSL() {
    if (beeType == XBEE_WIFI) return false;
    else return true;
  }

  String getBeeType() {
    if (beeType == XBEE_WIFI) return "S6B Wifi";
    else return "Cellular";
  }

  /*
   * Power functions
   */

  bool restart() {
    if (!commandMode()) return false;  // Return immediately
    sendAT(GF("AM1"));  // Digi suggests putting into airplane mode before restarting
                       // This allows the sockets and connections to close cleanly
    writeChanges();
    if (waitResponse() != 1) goto fail;
    sendAT(GF("FR"));
    if (waitResponse() != 1) goto fail;

    delay (2000);  // Actually resets about 2 seconds later

    // Wait until reboot complete and responds to command mode call again
    for (unsigned long start = millis(); millis() - start < 60000L; ) {
      if (commandMode(1)) {
        sendAT(GF("AM0"));  // Turn off airplane mode
        writeChanges();
        exitCommand();
        delay(250);  // wait a litle before trying again
      }
    }
    return true;


    fail:
      exitCommand();
      return false;
  }

  void setupPinSleep(bool maintainAssociation = false) {
    if (!commandMode()) return;  // Return immediately
    sendAT(GF("SM"),1);  // Pin sleep
    waitResponse();
    if (beeType == XBEE_WIFI && !maintainAssociation) {
        sendAT(GF("SO"),200);  // For lowest power, dissassociated deep sleep
        waitResponse();
    }
    else if (!maintainAssociation){
        sendAT(GF("SO"),1);  // For lowest power, dissassociated deep sleep
                             // Not supported by all modules, will return "ERROR"
        waitResponse();
    }
    writeChanges();
    exitCommand();
  }

  /*
   * SIM card functions
   */

  String getSimCCID() {
    if (!commandMode()) return "";  // Return immediately
    sendAT(GF("S#"));
    String res = readResponse();
    exitCommand();
    return res;
  }

  String getIMEI() {
    if (!commandMode()) return "";  // Return immediately
    sendAT(GF("IM"));
    String res = readResponse();
    exitCommand();
    return res;
  }

  String getOperator() {
    if (!commandMode()) return "";  // Return immediately
    sendAT(GF("MN"));
    String res = readResponse();
    exitCommand();
    return res;
  }

  /*
   * Generic network functions
   */

  int getRegistrationStatus() {
    if (!commandMode()) return REG_UNKNOWN;  // Return immediately

    sendAT(GF("AI"));
    String res = readResponse();
    char buf[3] = {0,};  // Set up buffer for response
    res.toCharArray(buf, 3);
    int intRes = strtol(buf, 0, 16);
    RegStatus stat = REG_UNKNOWN;

    switch (beeType){
      case XBEE_WIFI: {
        if(intRes == 0x00)  // 0x00 Successfully joined an access point, established IP addresses and IP listening sockets
          stat = REG_OK;
        else if(intRes == 0x01)  // 0x01 Wi-Fi transceiver initialization in progress.
          stat = REG_SEARCHING;
        else if(intRes == 0x02)  // 0x02 Wi-Fi transceiver initialized, but not yet scanning for access point.
          stat = REG_SEARCHING;
        else if(intRes == 0x13) { // 0x13 Disconnecting from access point.
          sendAT(GF("NR0"));  // Do a network reset; the S6B tends to get stuck "disconnecting"
          waitResponse(5000);
          writeChanges();
          stat = REG_UNREGISTERED;
        }
        else if(intRes == 0x23)  // 0x23 SSID not configured.
          stat = REG_UNREGISTERED;
        else if(intRes == 0x24)  // 0x24 Encryption key invalid (either NULL or invalid length for WEP).
          stat = REG_DENIED;
        else if(intRes == 0x27)  // 0x27 SSID was found, but join failed.
          stat = REG_DENIED;
        else if(intRes == 0x40)  // 0x40 Waiting for WPA or WPA2 Authentication.
          stat = REG_SEARCHING;
        else if(intRes == 0x41)  // 0x41 Device joined a network and is waiting for IP configuration to complete
          stat = REG_SEARCHING;
        else if(intRes == 0x42)  // 0x42 Device is joined, IP is configured, and listening sockets are being set up.
          stat = REG_SEARCHING;
        else if(intRes == 0xFF)  // 0xFF Device is currently scanning for the configured SSID.
          stat = REG_SEARCHING;
        else stat = REG_UNKNOWN;
        break;
      }
      case XBEE_CELL: {
        if(intRes == 0x00)  // 0x00 Connected to the Internet.
          stat = REG_OK;
        else if(intRes == 0x22)  // 0x22 Registering to cellular network.
          stat = REG_SEARCHING;
        else if(intRes == 0x23)  // 0x23 Connecting to the Internet.
          stat = REG_SEARCHING;
        else if(intRes == 0x24)  // 0x24 The cellular component is missing, corrupt, or otherwise in error.
          stat = REG_UNKNOWN;
        else if(intRes == 0x25)  // 0x25 Cellular network registration denied.
          stat = REG_DENIED;
        else if(intRes == 0x2A) {  // 0x2A Airplane mode.
          sendAT(GF("AM0"));  // Turn off airplane mode
          waitResponse();
          writeChanges();
          stat = REG_UNKNOWN;
        }
        else if(intRes == 0x2F) {  // 0x2F Bypass mode active.
          sendAT(GF("AP0"));  // Set back to transparent mode
          waitResponse();
          writeChanges();
          stat = REG_UNKNOWN;
        }
        else if(intRes == 0xFF)  // 0xFF Device is currently scanning for the configured SSID.
          stat = REG_SEARCHING;
        else stat = REG_UNKNOWN;
          break;
        }
    }

    exitCommand();
    return stat;
  }

  int getSignalQuality() {
    if (!commandMode()) return 0;  // Return immediately
    if (beeType == XBEE_WIFI) sendAT(GF("LM"));  // ask for the "link margin" - the dB above sensitivity
    else sendAT(GF("DB"));  // ask for the cell strength in dBm
    String res = readResponse();  // it works better if we read in as a string
    exitCommand();
    char buf[3] = {0,};  // Set up buffer for response
    res.toCharArray(buf, 3);
    int intRes = strtol(buf, 0, 16);
    if (beeType == XBEE_WIFI) return -93 + intRes;  // the maximum sensitivity is -93dBm
    else return -1*intRes; // need to convert to negative number
  }

  bool isNetworkConnected() {
    int s = getRegistrationStatus();
    return (s == REG_OK);
  }

  bool waitForNetwork(unsigned long timeout = 60000L) {
    commandMode();
    for (unsigned long start = millis(); millis() - start < timeout; ) {
      sendAT(GF("AI"));
      String res = readResponse();
      char buf[3] = {0,};  // Set up buffer for response
      res.toCharArray(buf, 3);
      int intRes = strtol(buf, 0, 16);
      if (intRes == 0) {
        exitCommand();
        return true;
      }
      delay(250);
    }
    exitCommand();
    return false;
  }

  String getLocalIP() {
    if (!commandMode()) return "";  // Return immediately
    sendAT(GF("MY"));
    String IPaddr; IPaddr.reserve(16);
    // wait for the response - this response can be very slow
    IPaddr = readResponse(30000);
    exitCommand();
    return IPaddr;
  }

  /*
   * WiFi functions
   */
  bool networkConnect(const char* ssid, const char* pwd) {

    if (!commandMode()) return false;  // return immediately

    sendAT(GF("EE"), 2);  // Set security to WPA2
    if (waitResponse() != 1) goto fail;

    sendAT(GF("ID"), ssid);
    if (waitResponse() != 1) goto fail;

    sendAT(GF("PK"), pwd);
    if (waitResponse() != 1) goto fail;

    writeChanges();
    exitCommand();

    return true;

fail:
    exitCommand();
    return false;
  }

  bool networkDisconnect() {
    if (!commandMode()) return false;  // return immediately
    sendAT(GF("NR0"));  // Do a network reset in order to disconnect
    int res = (1 == waitResponse(5000));
    writeChanges();
    exitCommand();
    return res;
  }

  /*
   * GPRS functions
   */
  bool gprsConnect(const char* apn, const char* user = "", const char* pwd = "") {
    if (!commandMode()) return false;  // Return immediately
    sendAT(GF("AN"), apn);  // Set the APN
    waitResponse();
    writeChanges();
    exitCommand();
    return true;
  }

  bool gprsDisconnect() {
    if (!commandMode()) return false;  // return immediately
    sendAT(GF("AM1"));  // Cheating and disconnecting by turning on airplane mode
    int res = (1 == waitResponse(5000));
    writeChanges();
    sendAT(GF("AM0"));  // Airplane mode off
    waitResponse(5000);
    writeChanges();
    exitCommand();
    return res;
  }

  /*
   * Messaging functions
   */

  bool sendSMS(const String& number, const String& text) {
    if (!commandMode()) return false;  // Return immediately

    sendAT(GF("IP"), 2);  // Put in text messaging mode
    if (waitResponse() !=1) goto fail;
    sendAT(GF("PH"), number);  // Set the phone number
    if (waitResponse() !=1) goto fail;
    sendAT(GF("TDD"));  // Set the text delimiter to the standard 0x0D (carriage return)
    if (waitResponse() !=1) goto fail;
    if (!writeChanges()) goto fail;

    exitCommand();
    stream.print(text);
    stream.write((char)0x0D);  // close off with the carriage return
    return true;

    fail:
      exitCommand();
      return false;
  }


  /*
   * Location functions
   */


  /*
   * Battery functions
   */


private:

  bool modemConnect(const char* host, uint16_t port, uint8_t mux = 0, bool ssl = false) {
    String strIP; strIP.reserve(16);
    unsigned long startMillis = millis();
    bool gotIP = false;
    // XBee's require a numeric IP address for connection, but do provide the
    // functionality to look up the IP address from a fully qualified domain name
    while (!gotIP && millis() - startMillis < 45000L)  // the lookup can take a while
    {
      sendAT(GF("LA"), host);
      while (stream.available() < 4) {};  // wait for any response
      strIP = stream.readStringUntil('\r');  // read result
      strIP.trim();
      DBG("<<< ", strIP);
      if (!strIP.endsWith(GF("ERROR"))) gotIP = true;
      delay(100);  // short wait before trying again
    }
    if (gotIP) {  // No reason to continue if we don't know the IP address
      IPAddress ip = TinyGsmIpFromString(strIP);
      return modemConnect(ip, port, mux, ssl);
    }
    else return false;
  }

  bool modemConnect(IPAddress ip, uint16_t port, uint8_t mux = 0, bool ssl = false) {
    bool success = true;
    String host; host.reserve(16);
    host += ip[0];
    host += ".";
    host += ip[1];
    host += ".";
    host += ip[2];
    host += ".";
    host += ip[3];
    if (ssl) {
      sendAT(GF("IP"), 4);  // Put in SSL over TCP communication mode
      success &= (1 == waitResponse());
    } else {
      sendAT(GF("IP"), 1);  // Put in TCP mode
      success &= (1 == waitResponse());
    }
    sendAT(GF("DL"), host);  // Set the "Destination Address Low"
    success &= (1 == waitResponse());
    sendAT(GF("DE"), String(port, HEX));  // Set the destination port
    success &= (1 == waitResponse());
    return success;
  }

  int modemSend(const void* buff, size_t len, uint8_t mux = 0) {
    stream.write((uint8_t*)buff, len);
    stream.flush();
    return len;
  }

  bool modemGetConnected(uint8_t mux = 0) {
    if (!commandMode()) return false;
    sendAT(GF("AI"));
    int res = waitResponse(GF("0"));
    exitCommand();
    return 1 == res;
  }

public:

  /* Utilities */

  void streamClear(void) {
    TINY_GSM_YIELD();
    while (stream.available()) { stream.read(); }
  }

  bool commandMode(int retries = 2) {
    int triesMade = 0;
    bool success = false;
    streamClear();  // Empty everything in the buffer before starting
    while (!success and triesMade < retries) {
      // Cannot send anything for 1 "guard time" before entering command mode
      // Default guard time is 1s, but the init fxn decreases it to 250 ms
      delay(guardTime);
      streamWrite(GF("+++"));  // enter command mode
      DBG("\r\n+++");
      success = (1 == waitResponse(guardTime*2));
      triesMade ++;
    }
    return success;
  }

  bool writeChanges(void) {
    sendAT(GF("WR"));  // Write changes to flash
    if (1 != waitResponse()) return false;
    sendAT(GF("AC"));  // Apply changes
    if (1 != waitResponse()) return false;
    return true;
  }

  void exitCommand(void) {
    sendAT(GF("CN"));  // Exit command mode
    waitResponse();
  }

  String readResponse(uint32_t timeout = 1000) {
    TINY_GSM_YIELD();
    unsigned long startMillis = millis();
    while (!stream.available() && millis() - startMillis < timeout) {};
    String res = stream.readStringUntil('\r');  // lines end with carriage returns
    res.trim();
    DBG("<<< ", res);
    return res;
  }

  // TODO: Optimize this!
  uint8_t waitResponse(uint32_t timeout, String& data,
                       GsmConstStr r1=GFP(GSM_OK), GsmConstStr r2=GFP(GSM_ERROR),
                       GsmConstStr r3=NULL, GsmConstStr r4=NULL, GsmConstStr r5=NULL)
  {
    /*String r1s(r1); r1s.trim();
    String r2s(r2); r2s.trim();
    String r3s(r3); r3s.trim();
    String r4s(r4); r4s.trim();
    String r5s(r5); r5s.trim();
    DBG("### ..:", r1s, ",", r2s, ",", r3s, ",", r4s, ",", r5s);*/
    data.reserve(16);  // Should never be getting much here for the XBee
    int index = 0;
    unsigned long startMillis = millis();
    do {
      TINY_GSM_YIELD();
      while (stream.available() > 0) {
        int a = stream.read();
        if (a <= 0) continue; // Skip 0x00 bytes, just in case
        data += (char)a;
        if (r1 && data.endsWith(r1)) {
          index = 1;
          goto finish;
        } else if (r2 && data.endsWith(r2)) {
          index = 2;
          goto finish;
        } else if (r3 && data.endsWith(r3)) {
          index = 3;
          goto finish;
        } else if (r4 && data.endsWith(r4)) {
          index = 4;
          goto finish;
        } else if (r5 && data.endsWith(r5)) {
          index = 5;
          goto finish;
        }
      }
    } while (millis() - startMillis < timeout);
finish:
    if (!index) {
      data.trim();
      data.replace(GSM_NL GSM_NL, GSM_NL);
      data.replace(GSM_NL, "\r\n    ");
      if (data.length()) {
        DBG("### Unhandled:", data, "\r\n");
      } else {
        DBG("### NO RESPONSE!\r\n");
      }
    } else {
      data.trim();
      data.replace(GSM_NL GSM_NL, GSM_NL);
      data.replace(GSM_NL, "\r\n    ");
      if (data.length()) {
        DBG("<<< ", data);
      }
    }
    return index;
  }

  uint8_t waitResponse(uint32_t timeout,
                       GsmConstStr r1=GFP(GSM_OK), GsmConstStr r2=GFP(GSM_ERROR),
                       GsmConstStr r3=NULL, GsmConstStr r4=NULL, GsmConstStr r5=NULL)
  {
    String data;
    return waitResponse(timeout, data, r1, r2, r3, r4, r5);
  }

  uint8_t waitResponse(GsmConstStr r1=GFP(GSM_OK), GsmConstStr r2=GFP(GSM_ERROR),
                       GsmConstStr r3=NULL, GsmConstStr r4=NULL, GsmConstStr r5=NULL)
  {
    return waitResponse(1000, r1, r2, r3, r4, r5);
  }

private:
  int           guardTime;
  XBeeType      beeType;
  GsmClientXBee*    sockets[TINY_GSM_MUX_COUNT];
};

#endif
