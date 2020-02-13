/*
 Program to talk to weather sensors and send output to MQTT topic /skybadger/device/temperature
 Supports REST web interface on port 80 returning application/json string
 
To do:
Cut MLX and TSL
Replace encoder with ADC i2c device to detect wind bearing

Done: 
fix all x.begin errors to represent their status as to whether present on the bus . 

Sensors added are:
Pulse counter for wind speed gauge (1 pin) 

Use MLX library for sky temperature sensing
Use TSL2591 Library for sky brightness (SQR) measurements

 Layout:
 GPIO 4,2 to SDA
 GPIO 5,0 to SCL 
 All 3.3v logic. 
 
 */

#include <esp8266_peri.h> //register map and access
#include <ESP8266WiFi.h>
#define MQTT_MAX_PACKET_SIZE 300
#include <PubSubClient.h> //https://pubsubclient.knolleary.net/api.html
#include <EEPROM.h>
#include <Wire.h>         //https://playground.arduino.cc/Main/WireLibraryDetailedReference
#include <Time.h>         //Look at https://github.com/PaulStoffregen/Time for a more useful internal timebase library
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>  //https://arduinojson.org/v5/api/
#include <Math.h>         //Used for PI.

//Ntp dependencies - available from v2.4
#include <time.h>
#include <sys/time.h>
#include <coredecls.h>
#define TZ              0       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries
#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
time_t now; //use as 'gmtime(&now);'

IPAddress timeServer(193,238,191,249); // pool.ntp.org NTP server
unsigned long NTPseconds; //since 1970

#include "skybadger_strings.h"
char* defaultHostname        = "espSQM01";
char* thisID                 = "espSQM01";
char* myHostname             = "espSQM01";

WiFiClient espClient;
PubSubClient client(espClient);
volatile bool callbackFlag = 0;

// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

//Hardware device system functions - reset/restart etc
EspClass device;
ETSTimer timer, timeoutTimer;
volatile bool newDataFlag = false;
volatile bool timeoutFlag = false;

const int MAXDATA = 3600;

//Function definitions
void onTimer(void);
String& getTimeAsString(String& );
uint32_t inline ICACHE_RAM_ATTR myGetCycleCount();
void callback(char* topic, byte* payload, unsigned int length) ;

//Sky Temperature Sensor
#include "MLX90614.h"
MLX90614 mlx( (uint8_t) MLX90614_ADDRESS, Wire );
bool mlxPresent = false;
float ambientTemperature;
float skyTemperature;

//Sky brightness sensor
#include "TSL2591.h"
TSL2591 tsl(TSL2591_ADDR, Wire ); 
bool tslPresent = false;
uint32_t lum = 0L;
float lux = 0.0F;

//Web Handler function definitions
#include "ESP8266_SQMHandlers.h"

void setup_wifi()
{
  WiFi.hostname( myHostname );
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to ");
  //Serial.println(ssid2);

  WiFi.setOutputPower( 20.5F );//full power WiFi
  WiFi.begin(ssid2, password2 );
  Serial.print("Searching for WiFi..");
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
      Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.printf("SSID: %s, RSSI %i dBm \n\r",  WiFi.SSID().c_str(), WiFi.RSSI() );
  Serial.printf("Hostname: %s\n\r",      WiFi.hostname().c_str() );
  Serial.printf("IP address: %s\n\r",    WiFi.localIP().toString().c_str() );
  Serial.printf("DNS address 0: %s\n\r", WiFi.dnsIP(0).toString().c_str() );
  Serial.printf("DNS address 1: %s\n\r", WiFi.dnsIP(1).toString().c_str() );

  delay(5000);
}

void setup()
{
  Serial.begin( 115200, SERIAL_8N1, SERIAL_TX_ONLY);
  Serial.println("ESP starting.");
  
  //Start NTP client
  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
  
  //Setup timestruct
  now = time(nullptr);

  //Setup defaults first - via EEprom. 
  
  // Connect to wifi 
  setup_wifi();                   
  
  //Pins mode and direction setup for i2c on ESP8266-01
  pinMode(0, OUTPUT);
  pinMode(2, OUTPUT);
   
  //I2C setup SDA pin 0, SCL pin 2 on ESP-01
  //I2C setup SDA pin 5, SCL pin 4 on ESP-12
  Wire.begin(0, 2);
  //Wire.begin(5, 4);
  Wire.setClock(100000 );//100KHz target rate
  
  Serial.println("Pins setup & interrupts attached.");
    
  //Open a connection to MQTT
  client.setServer( mqtt_server, 1883 );
  client.connect( thisID, pubsubUserID, pubsubUserPwd ); 

  //Create a timer-based callback that causes this device to read the local i2C bus devices for data to publish.
  client.setCallback( callback );
  client.subscribe( inTopic );
  Serial.println("MQTT setup complete.");

//Setup the sensors
//MLX sky brightness sensor
  Serial.print("Probe MLX: ");
  mlxPresent = mlx.begin( );
  if( !mlxPresent ) 
  {
    Serial.println("MLX Sensor missing");
  }
  else
  {
    Serial.println("MLX Sensor found");
    // one-time measure: 
    skyTemperature = mlx.getTemperature();
    ambientTemperature = mlx.getAmbient();
  }

  Serial.print("Probe TSL: ");
  tslPresent = tsl.begin( );
  if( !tslPresent ) 
  {
    Serial.println("TSL Sensor missing");
  }
  else
  {
    Serial.println("TSL Sensor found");
    // one-time measure: 
  
    // You can change the gain on the fly, to adapt to brighter/dimmer light situations
    //tsl.setGain(TSL2591_GAIN_LOW);         // set no gain (for bright situations)
    tsl.setGain(TSL2591_GAIN_MED);      // set 16x gain (for dim situations)
    //tsl.setGain(TSL2591_GAIN_HI);      // set 16x gain (for dim situations)
    //tsl.setGain(TSL2591_GAIN_MAX);      // set 16x gain (for dim situations)
    
    // Changing the integration time gives you a longer time over which to sense light
    // longer timelines are slower, but are good in very low light situtations!
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time (bright light)
  
    //32-bit raw count where high word is IR, low word is IR+Visible
    lum = tsl.getFullLuminosity();
    uint16_t ir, full;
    ir = lum >> 16;
    full = lum & 0x0FFFF;
    lux = tsl.calculateLux(full, ir);
    Serial.printf("Flux [TSL2591] : ir %u Full %u Lux %f \n", ir, full, lux );
    tsl.enable();
  }

  //Setup webserver handler functions
  server.on("/", handleStatusGet);
  server.onNotFound(handleNotFound); 

  //TODO add handler code in separate file. 
  server.on("/skytemp",         HTTP_GET, handleSkyTempGet );
  server.on("/skybrightness",   HTTP_GET, handleSkyBrightnessGet );
  
  httpUpdater.setup(&server);
  Serial.println( "Setup webserver handlers");
  server.begin();
    
  //Setup timers for measurement loop
  //setup interrupt-based 'soft' alarm handler for periodic acquisition/recording of new measurements.
  ets_timer_setfn( &timer, onTimer, NULL ); 
  
  //fire timer every 1000 msec
  //Set the timer function first
  ets_timer_arm_new( &timer, 1000, 1/*repeat*/, 1);
  //ets_timer_arm_new( &timeoutTimer, 2500, 0/*one-shot*/, 1);
  
  //Setup sleep parameters
  //wifi_set_sleep_type(LIGHT_SLEEP_T);

  Serial.println( "Setup complete" );
}

//Timer handler for 'soft' interrupt timer. 
void onTimer( void * pArg )
{
  newDataFlag = true;
}

//Used to complete timeout actions. 
void onTimeoutTimer( void* pArg )
{
  //Read command list and apply. 
  timeoutFlag = true;
}


//Main processing loop
void loop()
{
  String timestamp;
  String output;
  static int loopCount = 0;
  
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.createObject();

  if( newDataFlag == true ) //every second
  {
    root["time"] = getTimeAsString( timestamp );
    //Serial.println( getTimeAsString( timestamp ) );
    
    //MLX
    if( mlxPresent) 
    {
      skyTemperature = mlx.getTemperature();
      ambientTemperature = mlx.getAmbient();
    }

    //SQM Light meter
    if( tslPresent )
    {
      //32-bit raw count where high word is IR, low word is IR+Visible
      lum = tsl.getFullLuminosity(  );
      uint16_t ir, full;
      ir = lum >> 16;
      full = lum & 0x0FFFF;
      lux = tsl.calculateLux(full, ir);
    }

    //Manage gain to keep us in a HIGH COUNT range where possible. 
    if ( ( lum & 0x0FFFF ) < 1000 )
    {
      //increase gain  
      switch( tsl.getGain() )
      {
      case TSL2591_GAIN_LOW: tsl.setGain(TSL2591_GAIN_MED);break;
      case TSL2591_GAIN_MED: tsl.setGain(TSL2591_GAIN_HIGH);break;
      case TSL2591_GAIN_HIGH:tsl.setGain(TSL2591_GAIN_MAX);break;
      case TSL2591_GAIN_MAX://Increase collection time.
          switch ( tsl.getTiming() )
          {
            case TSL2591_INTEGRATIONTIME_100MS : tsl.setTiming( TSL2591_INTEGRATIONTIME_200MS ); break;
            case TSL2591_INTEGRATIONTIME_200MS : tsl.setTiming( TSL2591_INTEGRATIONTIME_300MS ); break;
            case TSL2591_INTEGRATIONTIME_300MS : tsl.setTiming( TSL2591_INTEGRATIONTIME_400MS ); break;
            case TSL2591_INTEGRATIONTIME_400MS : tsl.setTiming( TSL2591_INTEGRATIONTIME_500MS ); break;
            case TSL2591_INTEGRATIONTIME_500MS : tsl.setTiming( TSL2591_INTEGRATIONTIME_600MS ); break;
            case TSL2591_INTEGRATIONTIME_600MS : /* Can't do nuffin */ ; break;
            default: 
                break;
          }
          break;
      default: 
        break;
      }
    }
    else if ( ( lum & 0x0FFFF ) > 10000.0F )
    {
      //decrease gain  
      switch( tsl.getGain() )
      {
      case TSL2591_GAIN_LOW: break;/* Can't do nuffin*/
      case TSL2591_GAIN_MED: tsl.setGain(TSL2591_GAIN_LOW);break;
      case TSL2591_GAIN_HIGH:tsl.setGain(TSL2591_GAIN_MED);break;
      case TSL2591_GAIN_MAX: 
          switch ( tsl.getTiming() )
          {
            //decrease collection time
            case TSL2591_INTEGRATIONTIME_600MS : tsl.setTiming( TSL2591_INTEGRATIONTIME_500MS ); break;
            case TSL2591_INTEGRATIONTIME_500MS : tsl.setTiming( TSL2591_INTEGRATIONTIME_400MS ); break;
            case TSL2591_INTEGRATIONTIME_400MS : tsl.setTiming( TSL2591_INTEGRATIONTIME_300MS ); break;
            case TSL2591_INTEGRATIONTIME_300MS : tsl.setTiming( TSL2591_INTEGRATIONTIME_200MS ); break;
            case TSL2591_INTEGRATIONTIME_200MS : tsl.setTiming( TSL2591_INTEGRATIONTIME_100MS ); break;
            case TSL2591_INTEGRATIONTIME_100MS : tsl.setGain(TSL2591_GAIN_HIGH); break; //reduce gain to next level
            default: 
                break;
          }  
      default: 
        break;
      }
    }
    //Reset timing flag
    newDataFlag = false;
  }  

  //Handle web requests
  server.handleClient();
  
  if (callbackFlag == true )
  {
    //publish results
    if( client.connected() )
    {
      publishStuff();
      callbackFlag = false;
    }
    else
    {
      //Service MQTT keep-alives
    //reconnect();
      client.connect(thisID, pubsubUserID, pubsubUserPwd );
      delay(5);
    }
  } 
  client.loop();
}


/* MQTT callback for subscription and topic.
 * Only respond to valid states ""
 * Publish under ~/skybadger/sensors/<sensor type>/<host>
 * Note that messages have an maximum length limit of 18 bytes - set in the MQTT header file. 
 */
 void callback(char* topic, byte* payload, unsigned int length) 
 {  
  //set callback flag
  callbackFlag = true;  
 }

/*
 * Had to do a lot of work to get this to work 
 * Mostly around - 
 * length of output buffer
 * reset of output buffer between writing json strings otherwise it concatenates. 
 * Writing to serial output was essential.
 */
 void publishStuff( void )
 {
  String outTopic;
  String output;
  String timestamp;
  
  //checkTime();
  getTimeAsString( timestamp );
  
  if (mlxPresent) 
  {
    //publish to our device topic(s)
    DynamicJsonBuffer jsonBuffer(300);
    JsonObject& root = jsonBuffer.createObject();

    output="";//reset
    root["sensor"] = "MLX90614";
    root["time"] = timestamp;
    root["AmbTemp"] = ambientTemperature;
    root["SkyTemp"] = skyTemperature;

    outTopic = outSenseTopic;
    outTopic.concat("SkyTemperature/");
    outTopic.concat(myHostname);

    root.printTo( output );
    if ( client.publish( outTopic.c_str(), output.c_str(), true ) )        
      Serial.printf( "Published MLX90614 temperature sensor measurement %s to %s\n",  output.c_str(), outTopic.c_str() );
    else    
      Serial.printf( "Failed to publish MLX90614 Sky temperature sensor measurement %s to %s\n",  output.c_str(), outTopic.c_str() );

    output = "";
  }

  if( tslPresent )
  {
    //publish to our device topic(s)
    DynamicJsonBuffer jsonBuffer(300);
    JsonObject& root = jsonBuffer.createObject();

    output=""; //reset
    root["sensor"] = "TSL2591";
    root["time"] = timestamp;
    root["IR"]   = lum >> 16;
    root["Int"]  = lum & 0x0FFFF;
    root["Lux"]  = lux;

    outTopic = outSenseTopic;
    outTopic.concat("SkyBrightness/");
    outTopic.concat(myHostname);

    root.printTo( output );
    if ( client.publish( outTopic.c_str(), output.c_str(), true ) )        
      Serial.printf( " Published TSL reading: '%s' to %s\n",  output.c_str(), outTopic.c_str() );
    else
      Serial.printf( " Failed to publish TSL reading: '%s' to %s\n",  output.c_str(), outTopic.c_str() );
    
    output = "";
  }

  //Put a notice out regarding device health
  //publish to our device topic(s)
  DynamicJsonBuffer jsonBuffer(300);
  JsonObject& root = jsonBuffer.createObject();
  root["time"] = timestamp;
  root["hostname"] = myHostname;
  root["message"] = "Measurement published";
  root.printTo( output );
  outTopic = outHealthTopic;
  outTopic.concat( myHostname );
  
  if ( client.publish( outTopic.c_str(), output.c_str(), true ) )
    Serial.printf( " Published health message: '%s' to %s\n",  output.c_str(), outTopic.c_str() );
  else
    Serial.printf( " Failed to publish health message: '%s' to %s\n",  output.c_str(), outTopic.c_str() );
 }
 
/*
Returns the current state of the client. If a connection attempt fails, this can be used to get more information about the failure.
int - the client state, which can take the following values (constants defined in PubSubClient.h):
-4 : MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time
-3 : MQTT_CONNECTION_LOST - the network connection was broken
-2 : MQTT_CONNECT_FAILED - the network connection failed
-1 : MQTT_DISCONNECTED - the client is disconnected cleanly
0 : MQTT_CONNECTED - the client is connected
1 : MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT
2 : MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier
3 : MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection
4 : MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected
5 : MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect
*/

void reconnect() 
{
  String output;
  String timestamp;
  
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(thisID, pubsubUserID, pubsubUserPwd )) 
    {
      getTimeAsString( timestamp);
      Serial.println("connected");
      // Once connected, publish an announcement...
      output.concat( "{ \"hostname\":" );
      output.concat ( (String)myHostname );
      output.concat( "\", " );      
      output.concat( "\"time\":" );
      output.concat ( (String)timestamp );
      output.concat( "\", " );            
      output.concat( "\"message\":" );
      output.concat( "connected\" }" );
      output = (String)myHostname;
      output.concat( " connected" );
      client.publish( outHealthTopic, output.c_str() );
      // ... and resubscribe
      client.subscribe(inTopic);
    }
    else
    {
     // Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      for(int i = 0; i<5000; i++)
      {
        delay(1);
        //delay(20);
        //yield();
      }
    }
  }
}

/*
 * Helper functions
 */
String& getTimeAsString(String& output)
{
/* ISO Dates (Date-Time)
   ISO dates can be written with added hours, minutes, and seconds (YYYY-MM-DDTHH:MM:SSZ)
   Z indicates Zulu or GMT time, 
   For other timezones use +/-HH:MM instead of Z
   Can replace 'T' with ' ' - works in javascript nicely.
*/
    struct timeval tv;
    gettimeofday( &tv, nullptr );
    //double milliTimestamp = ((double) (tv.tv_sec))*1000.0 + tv.tv_usec/1000.0;
    //Serial.println( tv.tv_sec );//works in seconds - get 10 digits 
    //Serial.println( tv.tv_usec/1000 );//works in usecs - get 5 digits
    //Serial.println( milliTimestamp, 3 ); //Doesnt work when you put them together - overflows

    //get time, maintained by NTP, but only to the second. 
    //Hence we'd like to use the system time in ms from 1970 - which is what the js date parser would like too, but esp cant provide 
    //an integer long enough it seems. 
    now = time(nullptr);
    struct tm* gnow = gmtime( &now );
    
    //This works but its heavy for an embedded device.
    char buf[256];
    sprintf( buf, "%4i-%02i-%02i %02i:%02i:%02.3fZ", gnow->tm_year + 1900, gnow->tm_mon +1, gnow->tm_mday, gnow->tm_hour, gnow->tm_min, (float) gnow->tm_sec + tv.tv_usec/1000000.0 );
    
    //String and Serial classes can't handle 'long long' - missing SerialHelper overload 
    output = String(buf);
    //Serial.print("timestamp value:");Serial.println( output ); 
    return output;
}

uint32_t inline ICACHE_RAM_ATTR myGetCycleCount()
{
    uint32_t ccount;
    __asm__ __volatile__("esync; rsr %0,ccount":"=a" (ccount));
    return ccount;
}
