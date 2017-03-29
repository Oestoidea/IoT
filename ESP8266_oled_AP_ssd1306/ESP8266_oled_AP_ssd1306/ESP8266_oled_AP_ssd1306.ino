#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include "font.h"

extern "C" {
#include "user_interface.h"
}

// Battery voltage
int16_t analogNow;
int16_t analogPrev = 1000;
float analogNowVolts;

// Delays
int setupDelay = 10;  // seconds
int webRestart = 30;  // seconds

// General variables
bool flashPressed = 0;
int preClientsNumber = 0;
bool displayResetFlag = 0;

#define height 64                      // 64 or 32 for 128x64 or 128x32 OLEDs

#define offset        0x00             // offset=0 for SSD1306 controller
//#define offset      0x02             // offset=2 for SH1106 controller
#define OLEDaddress   0x3c
#define myFont        myFont6         
#define fontWeigth    6

char bufferIP[16];
char bufferMAC[18];
char bufferClients[4];
char bufferChannel[4];
char bufferRSSI[4];
char bufferVoltage[5];

String myAPStandard;
String myAPChannel;
IPAddress myAPIP;
String mySTARSSI;
String mySTABSSID;
String mySTAMAC;
IPAddress mySTAIP;
WiFiClient client;

ESP8266WebServer server(80);

//*** Soft Ap variables ***
const char *APssid = "ESPap";
const char *APpassword = "password"; // No password for the AP
int APchannel = 11; // default channel
IPAddress APlocalIP(192, 168, 4, 1);
IPAddress APgateway(192, 168, 4, 1);
IPAddress APsubnet(255, 255, 255, 0);

//***STAtion variables ***
const char *STAssid = "SSID"; // Network to be joined as a station SSID
const char *STApassword = "password"; // Network to be joined as a station password
/*IPAddress STAlocal_IP(192, 168, 3, 50);
IPAddress STAgateway(192, 168, 3, 1);
IPAddress STAsubnet(255, 255, 255, 0);*/

/* Just a little test message.  Go to http://192.168.4.1 in a web browser
 * connected to this access point to see it.
 */
void handleRoot() {
    String content = "<!DOCTYPE html PUBLIC '-//W3C//DTD HTML 3.2 Final//EN'><html>";
    content += "<head><link rel='icon' href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAS0lEQVR42s2SMQ4AIAjE+P+ncSYdasgNXMJgcyIIlVKPIKdvioAXyWBeJmVpqRZKWtj9QWAKZyWll50b8IcL9JUeQF50n28ckyb0ADG8RLwp05YBAAAAAElFTkSuQmCC' type='image/x-png' />";
    content += "<title>ESP8266 AP</title><meta charset='UTF-8'><script type='text/javascript'>var interval = setInterval(function(){location.reload(true)}, " + String(webRestart) + "000);</script></head>";
    content += "<body><h1>ESP8266 AP&STA</h1><form action='/' method='post' id='channelForm'></form><table><tbody>";
    content += "<tr><th colspan='2'><span style='font-size:1.4em'>AP</span></th></tr>";
    content += "<tr><td>Channel:</td><td><select size='1' form='channelForm' name='APchannel'><option ";
    content += (myAPChannel == "1")?"selected ":" ";
    content += "value='1'>1</option><option ";
    content += (myAPChannel == "2")?"selected ":" ";
    content += "value='2'>2</option><option ";
    content += (myAPChannel == "3")?"selected ":" ";
    content += "value='3'>3</option><option ";
    content += (myAPChannel == "4")?"selected ":" ";
    content += "value='4'>4</option><option ";
    content += (myAPChannel == "5")?"selected ":" ";
    content += "value='5'>5</option><option ";
    content += (myAPChannel == "6")?"selected ":" ";
    content += "value='6'>6</option><option ";
    content += (myAPChannel == "7")?"selected ":" ";
    content += "value='7'>7</option><option ";
    content += (myAPChannel == "8")?"selected ":" ";
    content += "value='8'>8</option><option ";
    content += (myAPChannel == "9")?"selected ":" ";
    content += "value='9'>9</option><option ";
    content += (myAPChannel == "10")?"selected ":" ";
    content += "value='10'>10</option><option ";
    content += (myAPChannel == "11")?"selected ":" ";
    content += "value='11'>11</option><option ";
    content += (myAPChannel == "12")?"selected ":" ";
    content += "value='12'>12</option><option ";
    content += (myAPChannel == "13")?"selected ":" ";
    content += "value='13'>13</option>";
    content += "</select>&nbsp;<input type='submit' form='channelForm' value='Set' /></td></tr>";
    content += "<tr><td>SSID:</td><td>" + String(APssid) + "</td></tr>";
    content += "<tr><td>Standard:</td><td>" + myAPStandard + "</td></tr>";
    content += "<tr><td>AP IP:</td><td>" + String(myAPIP[0]) + '.' + String(myAPIP[1]) + '.' + String(myAPIP[2]) + '.' + String(myAPIP[3]) + "</td></tr>";
    content += "<tr><td>Battery:</td><td>" + String(analogNowVolts) + "V</td></tr>";
    content += "<tr><th colspan='2'></th></tr><tr><th colspan='2'><span style='font-size:1.4em'>STA</span></th></tr>";
    content += "<tr><td>BSSID:</td><td>" + String(STAssid) + "</td></tr>";
    content += "<tr><td>RSSI:</td><td>" + mySTARSSI + "&nbsp;dBm</td></tr>";
    content += "<tr><td>BSSID MAC:</td><td>" + mySTAMAC + "</td></tr>";
    content += "<tr><td>Client MAC:</td><td>" + mySTABSSID + "</td></tr>";
    content += "<tr><td>Client IP:</td><td>" + String(mySTAIP[0]) + '.' + String(mySTAIP[1]) + '.' + String(mySTAIP[2]) + '.' + String(mySTAIP[3]) + "</td></tr>";
    content += "<tr><th colspan='2'></th></tr><tr><th colspan='2'><span style='font-size:1.4em'>Clients</span></th></tr>";
    content += "<tr><td>Clients number:</td><td>" + String(wifi_softap_get_station_num()) + "</td></tr>";

    struct station_info *station_list = wifi_softap_get_station_info();
    while (station_list != NULL) {
        IPAddress clientIP = (&station_list->ip)->addr;
        char station_mac[18] = {0};
        sprintf(station_mac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(station_list->bssid)); 
        content += "<tr><td>" + String(clientIP[0]) + "." + String(clientIP[1]) + "." + String(clientIP[2]) + "." + String(clientIP[3]) + "</td><td>" + station_mac  + "</td></tr>";
        station_list = STAILQ_NEXT(station_list, next);
    }

    content += "</tbody></table></body></html>";
    server.send(200, "text/html", content);
}

void showSettingsAP() {
    // AP
    myAPChannel = String(APchannel);
    Serial.print("AP channel: ");
    Serial.println(myAPChannel);
    myAPChannel.toCharArray(bufferChannel, 4);
    sendStrXY("Ch", 0, 0);
    sendStrXY(bufferChannel, 0, 3);

    Serial.print("AP SSID: ");
    Serial.println(APssid);
    sendStrXY(APssid, 0, 7);

    Serial.print("AP standard: ");
    sendStrXY("Std", 1, 0);
    switch(wifi_get_phy_mode()) {
        case PHY_MODE_11B:
            Serial.println("802.11b");
            myAPStandard = "802.11b";
            sendStrXY("802.11b", 1, 3);
            break;
      
        case PHY_MODE_11G:
            Serial.println("802.11g");
            myAPStandard = "802.11g";
            sendStrXY("802.11g", 1, 3);
            break;
            
        case PHY_MODE_11N:
            Serial.println("802.11n");
            myAPStandard = "802.11n";
            sendStrXY("802.11n", 1, 3);
            break;
    }
  
    myAPIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myAPIP);
    String ipStr = String(myAPIP[0]) + '.' + String(myAPIP[1]) + '.' + String(myAPIP[2]) + '.' + String(myAPIP[3]);
    ipStr.toCharArray(bufferIP, 16);
    sendStrXY("IP", 2, 0);
    sendStrXY(bufferIP, 2, 3);
}

void showSettingsSTA() {
    // STA

    mySTARSSI = WiFi.RSSI();
    Serial.print("STA RSSI: ");
    Serial.println(mySTARSSI);
    mySTARSSI.toCharArray(bufferRSSI, 4);
    sendStrXY(bufferRSSI, 4, 0);
    sendStrXY("dBm", 4, 3);

    Serial.print("STA SSID: ");
    Serial.println(STAssid);
    sendStrXY(STAssid, 4, 7);

    uint8* mac = WiFi.BSSID();
    mySTABSSID = "";
    for (int i = 0; i < 6; ++i) {
        String digit = String(*mac, HEX);
        if (digit.length() < 2)
            mySTABSSID += String("0") + digit;
        else
            mySTABSSID += digit;
        mac++;
        if (i != 5)
            mySTABSSID += ":";
    }
    mySTABSSID.toUpperCase();
    Serial.print("STA MAC address: ");
    Serial.println(mySTABSSID);
    mySTABSSID.toCharArray(bufferMAC, 18);    
    sendStrXY("STA", 5, 0);
    sendStrXY(bufferMAC, 5, 3);

    mySTAMAC = WiFi.macAddress();
    Serial.print("Client MAC address: ");
    Serial.println(mySTAMAC);
    mySTAMAC.toCharArray(bufferMAC, 18);    
    sendStrXY("MAC CL", 6, 0);
    sendStrXY(bufferMAC, 6, 3);

    mySTAIP = WiFi.localIP();
    Serial.print("AP IP address: ");
    Serial.println(mySTAIP);
    String ipStr = String(mySTAIP[0]) + '.' + String(mySTAIP[1]) + '.' + String(mySTAIP[2]) + '.' + String(mySTAIP[3]);
    ipStr.toCharArray(bufferIP, 16);
    sendStrXY("IP", 7, 0);
    sendStrXY(bufferIP, 7, 3);
}

void setup(void) {
    delay(500);
    Serial.begin(115200);
    pinMode(A0, INPUT);
    Wire.begin(4, 2);    // Initialize I2C and OLED Display SDA - GPIO4 (D2), SLC - GPIO2 (D4)
    initOLED();

    Serial.println("OLED initialized");
    Serial.println("Configuring access point...");

    WiFi.mode(WIFI_AP_STA);

    // Configure the Soft Access Point. Somewhat verbosely... (for completeness sake)
    Serial.print("Soft-AP configuration ... ");
    Serial.println(WiFi.softAPConfig(APlocalIP, APgateway, APsubnet) ? "OK" : "Failed!"); // configure network
    Serial.print("Setting soft-AP ... ");
    Serial.println(WiFi.softAP(APssid, APpassword, APchannel) ? "OK" : "Failed!"); // Setup the Access Point
    Serial.print("Soft-AP IP address = ");
    Serial.println(WiFi.softAPIP()); // Confirm AP IP address

    resetDisplay();
    showSettingsAP();

    // Fire up wifi station
    Serial.printf("Station connecting to %s\n", STAssid);
    WiFi.begin(STAssid, STApassword);
    //WiFi.config(STAlocal_IP, STAgateway, STAsubnet);
    int times = 0;
    while (WiFi.status() != WL_CONNECTED && times < setupDelay) {
        delay(500);
        Serial.print(".");
        times++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        showSettingsSTA();
        delay(setupDelay * 500);
    }
    Serial.println();
    Serial.print("Station connected, IP address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());

    // HTTP server
    
    server.on("/", handleRoot);
    server.begin();
    Serial.println("HTTP server started");
    Serial.println("Setup done");
    
    clearDisplay();
//    digitalWrite(LEDpin,LOW);
}

void loop(void) {
    server.handleClient();

    if (server.hasArg("APchannel")) {
        myAPChannel = server.arg("APchannel");
        wifi_set_channel(myAPChannel.toInt());
    }

    // show list of clients or setting when the Flash button pressed on
    if (digitalRead(0) == LOW) {
        flashPressed = !flashPressed;
        displayResetFlag = 1;
        while (digitalRead(0) == HIGH)
            delay(20);
    }

    if (displayResetFlag) {
        delay(500);
        for (int row = 0; row < (height / 8); row++)  // Clear empty rows
            sendStrXY("                      ", row, 0);
        //resetDisplay();
        displayResetFlag = 0;
    }

    if (!flashPressed) {
        int currentClientsNumber = wifi_softap_get_station_num();
        if (preClientsNumber > currentClientsNumber)
            for (int row = preClientsNumber; row > currentClientsNumber; row--)
              sendStrXY("                      ", row, 0);
        preClientsNumber = currentClientsNumber;
        
        String myClientsNumber = String(currentClientsNumber);
        Serial.print(myClientsNumber);
        Serial.println(" clients");
        myClientsNumber.toCharArray(bufferClients, 4);
        sendStrXY("Clients:", 0, 0);
        sendStrXY(bufferClients, 0, 7); // HEX
    
        struct station_info *station_list = wifi_softap_get_station_info();
    
        uint8_t row = 0;
        while (station_list != NULL) {
            row++;
          
            IPAddress clientIP = (&station_list->ip)->addr;
            Serial.print(clientIP);
            char ipStr[6] = {0};
            sprintf(ipStr, "%3d", clientIP[3]);
            sendStrXY(ipStr, row, 0);
            
            char station_mac[18] = {0};
            sprintf(station_mac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(station_list->bssid));
            Serial.print(" | ");
            Serial.println(station_mac);
            sendStrXY(station_mac, row, 3); // Print MAC to OLED
    
            station_list = STAILQ_NEXT(station_list, next);
        }

        wifi_softap_free_station_info();
    } else {
        showSettingsAP();
        if (WiFi.status() == WL_CONNECTED)
            showSettingsSTA();
    }

    analogNow = analogRead(A0);
    analogNowVolts = analogNow * 4.05 / 454;
    
    String myVoltage = String(analogNowVolts);
    Serial.print("--  ");
    Serial.print(analogNowVolts);
    Serial.println("V --");
    myVoltage.toCharArray(bufferVoltage, 5);
    sendStrXY(bufferVoltage, 0, 12); // HEX
    sendStrXY("V", 0, 15);
    
    if (analogPrev > 300 && analogNow <= 336 && analogPrev == analogNow) // 336 -> 3V
        ESP.deepSleep(999999999*999999999U, WAKE_NO_RFCAL); // 21,000 years in sleep mode
    analogPrev = analogNow;
    
    delay(1000);
}

// Resets display depending on the actual mode.
static void resetDisplay(void) {
    displayOff();
    clearDisplay();
    displayOn();
}

// Turns display on.
void displayOn(void) {
    sendCommand(0xAF);        //display on
}

// Turns display off.
void displayOff(void) {
    sendCommand(0xAE);    //display off
}

// Clears the display by sendind 0 to all the screen map.
static void clearDisplay(void) {
    for (int k = 0; k < height / 8; k++) { 
        setXY(k, 0);    
        for (int i = 0; i < (128 + 2 * offset); i++) {   //locate all COL
            sendChar(0);         //clear all COL
            //delay(10);
        }
    }
}

// Actually this sends a byte, not a char to draw in the display. 
// Display's chars uses 8 byte font the small ones and 96 bytes
// for the big number font.
static void sendChar(unsigned char data) {
    //if (interrupt && !doing_menu) return;   // Stop printing only if interrupt is call but not in button functions
    
    Wire.beginTransmission(OLEDaddress);  //begin transmitting
    Wire.write(0x40);                     //data mode
    Wire.write(data);
    Wire.endTransmission();               //stop transmitting
}

// Prints a display char (not just a byte) in coordinates X Y,
// being multiples of 8. This means we have 16 COLS (0-15) 
// and 8 ROWS (0-7).
static void sendCharXY(unsigned char data, int X, int Y) {
    setXY(X,Y);
    Wire.beginTransmission(OLEDaddress);  //begin transmitting
    Wire.write(0x40);                     //data mode
    
    for (int i = 0; i < fontWeigth; i++)
        Wire.write(pgm_read_byte(myFont[data - 0x20] + i));
      
    Wire.endTransmission();               //stop transmitting
}

// Used to send commands to the display.
static void sendCommand(unsigned char com) {
    Wire.beginTransmission(OLEDaddress);      //begin transmitting
    Wire.write(0x80);                         //command mode
    Wire.write(com);
    Wire.endTransmission();                   //stop transmitting
}

// Set the cursor position in a 16 COL * 8 ROW map.
static void setXY(unsigned char row, unsigned char col) {
    sendCommand(0xB0 + row);                      //set page address
    sendCommand(offset + (8 * col & 0x0F));       //set low col address
    sendCommand(0x10 + ((8 * col >> 4) & 0x0F));  //set high col address
}

// Prints a string regardless the cursor position.
static void sendStr(unsigned char *string) {
    while (*string) {
        for (int i = 0; i < fontWeigth; i++)
            sendChar(pgm_read_byte(myFont[*string - 0x20] + i));
        *string++;
    }
}

// Prints a string in coordinates X Y, being multiples of 8.
// This means we have 16 COLS (0-15) and 8 ROWS (0-7).
static void sendStrXY(const char *string, int X, int Y) {
    setXY(X, Y);
    while(*string) {
        for (int i = 0; i < fontWeigth; i++)
            sendChar(pgm_read_byte(myFont[*string - 0x20] + i));
        *string++;
    }
}

// Inits oled and draws logo at startup
static void initOLED(void) {
    displayOff();
    // Adafruit Init sequence for 128x64 OLED module
    sendCommand(0xAE);    //DISPLAYOFF
    sendCommand(0xD5);    //SETDISPLAYCLOCKDIV
    sendCommand(0x80);    // the suggested ratio 0x80
    sendCommand(0xA8);    //SSD1306_SETMULTIPLEX
    if (height == 64)
        sendCommand(0x3F);  // for 128x64
    else
        sendCommand(0x1F);  // for 128x32
    sendCommand(0xD3);    //SETDISPLAYOFFSET
    sendCommand(0x0);     //no offset
    sendCommand(0x40 | 0x0);    //SETSTARTLINE
    sendCommand(0x8D);    //CHARGEPUMP
    sendCommand(0x14);
    sendCommand(0x20);    //MEMORYMODE
    sendCommand(0x00);    //0x0 act like ks0108
    
    //sendCommand(0xA0 | 0x1);    //SEGREMAP   //Rotate screen 180 deg
    sendCommand(0xA0);
    
    //sendCommand(0xC8);    //COMSCANDEC  Rotate screen 180 Deg
    sendCommand(0xC0);
    
    sendCommand(0xDA);     //COMSCANDEC
    if (height == 64)       //
        sendCommand(0x12);  // for 128x64
    else
        sendCommand(0x02);  // for 128x32
    sendCommand(0x81);     //SETCONTRAS
    if (height == 64)       //
        sendCommand(0xCF);  // for 128x64
    else
        sendCommand(0x8F);  // for 128x32
    sendCommand(0xd9);     //SETPRECHARGE 
    sendCommand(0xF1); 
    sendCommand(0xDB);     //SETVCOMDETECT                
    sendCommand(0x40);
    sendCommand(0xA4);     //DISPLAYALLON_RESUME        
    sendCommand(0xA6);     //NORMALDISPLAY             
    
    //clearDisplay();
    sendCommand(0x2e);     // stop scroll
    //----------------------------REVERSE comments----------------------------//
    sendCommand(0xa0);     //seg re-map 0->127(default)
    sendCommand(0xa1);     //seg re-map 127->0
    sendCommand(0xc8);
    delay(1000);
    //----------------------------REVERSE comments----------------------------//
    //sendCommand(0xa7);    //Set Inverse Display  
    //sendCommand(0xae);    //display off
    sendCommand(0x20);     //Set Memory Addressing Mode
    sendCommand(0x00);     //Set Memory Addressing Mode ab Horizontal addressing mode
    //sendCommand(0x02);    // Set Memory Addressing Mode ab Page addressing mode(RESET)  
}
