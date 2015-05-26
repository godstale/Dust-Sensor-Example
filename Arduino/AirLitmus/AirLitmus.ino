#include <U8glib.h>
#include "bitmap.h"

// Demo using DHCP and DNS to perform a web client request.
// 2011-06-08 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php
#include <EtherCard.h>

///////////////////////////////////////////////////////////////////
//----- Ethernet instance

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x37,0x34 };

byte Ethernet::buffer[500];
static uint32_t timer;

const char website[] PROGMEM = "www.bloter.net";

//timing variable
int res = 0;

///////////////////////////////////////////////////////////////////
//----- OLED instance

// IMPORTANT NOTE: The complete list of supported devices 
// with all constructor calls is here: http://code.google.com/p/u8glib/wiki/device

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE|U8G_I2C_OPT_DEV_0);	// I2C / TWI 


///////////////////////////////////////////////////////////////////
// Dust sensor 
/*
 Interface to Sharp GP2Y1010AU0F Particle Sensor
 Program by Christopher Nafis 
 Written April 2012
 
 http://www.sparkfun.com/datasheets/Sensors/gp2y1010au_e.pdf
 http://sensorapp.net/?p=479
 
 Sharp pin 1 (V-LED)   => 5V (connected to 150ohm resister)
 220uF capacitor between pin1 and pin2
 Sharp pin 2 (LED-GND) => Arduino GND pin
 Sharp pin 3 (LED)     => Arduino pin 2
 Sharp pin 4 (S-GND)   => Arduino GND pin
 Sharp pin 5 (Vo)      => Arduino A0 pin
 Sharp pin 6 (Vcc)     => 5V
 */
 
int dustPin = 0;
int dustVal=0;  // analog read
int count=0;
float ppm=0;    // sum of analog read
char s[32];
float voltage = 0;  // average voltage
float dustdensity = 0;  // average dust density
float ppmpercf = 0;  // 
unsigned long lastSensingTime = 0;
const unsigned long SENSING_INTERVAL = 3000;

int ledPower=2;
int delayTime=280;
int delayTime2=40;
float offTime=9680;

unsigned long lastConnectionTime = 0;        // last time you connected to the server, in milliseconds
const unsigned long POSTING_INTERVAL = 30*1000;  //delay between updates to Pachube.com

void setup() {
  //Serial.begin(9600);
  
  pinMode(ledPower,OUTPUT);
  pinMode(4, OUTPUT);
  
  //showInfo("initializing...", 3);

  //Initialize Ethernet
  initialize_ethernet();
}
 
void loop(){
  //if correct answer is not received then re-initialize ethernet module
  //if (res > 220){
  //  //initialize_ethernet(); 
  //}
  //es = res + 1;
  
  // Monitoring incoming packets
  ether.packetLoop(ether.packetReceive());
  
  // Read dust sensor 
  if(millis() - lastSensingTime > SENSING_INTERVAL) {
    readDustSensor();
    lastSensingTime = millis();
  }

  if(millis() - lastConnectionTime > POSTING_INTERVAL) {
    // calculate average voltage, dust density
    voltage = ppm/count*0.0049;
    dustdensity = 0.17*voltage-0.1;
    ppmpercf = (voltage-0.0256)*120000;
    if (ppmpercf < 0)
      ppmpercf = 0;
    if (dustdensity < 0 )
      dustdensity = 0;
    if (dustdensity > 0.5)
      dustdensity = 0.5;
    
    // for debug
    String dataString = "";
    //dataString += dtostrf(voltage, 9, 4, s);
    //dataString += ",";
    dataString += dtostrf(dustdensity, 5, 4, s);
    dataString += " mg/m3";
    //dataString += ",";
    //dataString += dtostrf(ppmpercf, 8, 0, s);
    showInfo(dataString, 2);
    
    sendData();

    count=0;
    ppm=0;    
    lastConnectionTime = millis();
  }

/*
  // 0 - 5V mapped to 0 - 1023 integer values
  // recover voltage
  voltageRead = ((float)dustVal) * cMultiplier;
  // linear eqaution taken from http://www.howmuchsnow.com/arduino/airquality/
  // Chris Nafis (c) 2012
  dustDensity = 0.17 * voltageRead - 0.1;
  Serial.print("Voltage read = ");
  Serial.print(dustVal);
  Serial.print(", Dust density (mg/m3) = ");
  Serial.println(dustDensity);
*/

}

//////////////////////////////////////////////////////
// Read analog voltage value from dust sensor

void readDustSensor() {
  count = count + 1;
  // ledPower is any digital pin on the arduino connected to Pin 3 on the sensor
  digitalWrite(ledPower,LOW); // power on the LED
  delayMicroseconds(delayTime);
  
  dustVal=analogRead(dustPin); // read the dust value via pin 5 on the sensor
  ppm = ppm+dustVal;
  delayMicroseconds(delayTime2);

  digitalWrite(ledPower,HIGH); // turn the LED off
  delayMicroseconds(offTime);
  
  delay(2000);
  
  String strVoltage = "Voltage=";
  strVoltage += dtostrf(dustVal, 5, 2, s);
  showInfo(strVoltage, 3);
}

//////////////////////////////////////////////////////
// Ethernet functions

// Send data to remote server
char url_buf[113];
void sendData() {
  for(int i=0; i<113; i++)
    url_buf[i] = 0x00;
  String strUrl = "admin-ajax.php?action=bloter_extension_update_air_data&id=2974CD80-0A61-2DDF-4198-2F7E68A4D1F1&field1=";
  strUrl += dtostrf(dustdensity, 5, 4, s);
  strUrl.toCharArray(url_buf, 112);
  url_buf[112] = 0x00;
  ether.browseUrl(PSTR("/wp-admin/"), url_buf, website, my_callback);
  
  //Serial.print(website);
  //Serial.print("/wp-admin/");
  //Serial.println(url_buf);
}

// called when the client request is complete
static void my_callback (byte status, word off, word len) {
  Ethernet::buffer[off+300] = 0;
  showInfo(">>> Received", 3);
  //showInfo((char*) Ethernet::buffer + off);
  //delay(5000);
}

// Initialize ethernet module
void initialize_ethernet(void){  
  for(;;){ // keep trying until you succeed 
    //Reinitialize ethernet module
    //pinMode(5, OUTPUT);
    //digitalWrite(5, LOW);
    //delay(1000);
    //digitalWrite(5, HIGH);
    //delay(500);

    if (ether.begin(sizeof Ethernet::buffer, mymac) == 0){ 
      showInfo("Failed to access Ethernet controller", 3);
      continue;
    }
    showInfo("Ethernet begin", 3);
    
    if (!ether.dhcpSetup()){
      showInfo("DHCP failed", 3);
      continue;
    }

    //ether.printIp("IP:  ", ether.myip);
    //ether.printIp("GW:  ", ether.gwip);  
    //ether.printIp("DNS: ", ether.dnsip);  

    if (!ether.dnsLookup(website)) {
      showInfo("DNS failed", 3);
      delay(5000);
      continue;
    }

    ether.printIp("SRV: ", ether.hisip);
    showInfo("Ethernet initialized!!", 3);
    delay(3000);

    //reset init value
    res = 0;
    break;
  }
}


//////////////////////////////////////////////////////
// Display

// Draw screen
const int DISP_CHAR_LEN = 13;
const int DISP_CHAR_LEN2 = 24;
char str_line1[] = "Dust density ";
char str_line2[] = "00.0000 mg/m3";
char str_line3[] = "Initializing............";
void showInfo(String strDisp, int line_index) {
  if(strDisp == NULL) {
    return;
  }
  
  int posY = 45;
  if(line_index == 1) {
    for(int i=0; i<DISP_CHAR_LEN; i++)
      str_line1[i] = 0x00;
    strDisp.toCharArray(str_line1, DISP_CHAR_LEN - 1);
  } else if(line_index == 2) {
    for(int i=0; i<DISP_CHAR_LEN; i++)
      str_line2[i] = 0x00;
    strDisp.toCharArray(str_line2, DISP_CHAR_LEN - 1);
  } else {
    for(int i=0; i<DISP_CHAR_LEN2; i++)
      str_line3[i] = 0x00;
    strDisp.toCharArray(str_line3, DISP_CHAR_LEN2 - 1);
  }
  
  // picture loop  
  u8g.firstPage();  
  do {
    // draw icon
    u8g.drawBitmapP( 13, 11, 3, 24, IMG_logo_24x24);
    
    // show text
    u8g.setFont(u8g_font_fixed_v0);
    u8g.setFontRefHeightExtendedText();
    u8g.setDefaultForegroundColor();
    u8g.setFontPosTop();
    u8g.drawStr(50, 10, str_line1);
    u8g.drawStr(50, 26, str_line2);
    u8g.drawStr(8, 47, str_line3);
  } while( u8g.nextPage() );
}

