#if ! defined _ESP8266_SQMHandlers_h_
#define _ESP8266_SQMHandlers_h_

  extern ESP8266WebServer server;
  void handleRoot(void);
  void handleNotFound();
  void handleSkyTempGet( void );
//  void handleStatusGet( void);
  void handleSkyBrightnessGet( void );
    
  void handleSkyTempGet( void )
  {
    String timeString = "", message = "";
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();

    root["time"] = getTimeAsString( timeString );
    if( mlxPresent )
    {
      root["Sky Temperature"] = (float) skyTemperature;
      root["Ambient Temperature"] = (float) ambientTemperature;
    }
    else
      root["Message"] = "MLX90614 sensor not present";

    root.printTo( Serial );Serial.println(" ");
    root.printTo(message);
    server.send(200, "application/json", message);      
  }

  void handleSkyBrightnessGet( void )
  {
    String timeString = "", message = "";
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();

    root["time"] = getTimeAsString( timeString );
    if( tslPresent )
    {
      root["Brightness"] = (float) lux;
    }
    else
      root["Message"] = "TLS2591 sensor not present";

    root.printTo( Serial );Serial.println(" ");
    root.printTo(message);
    server.send(200, "application/json", message);      
  }
 
 void handleStatusGet( void)
 {
      String timeString = "", message = "";
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    //Update the error message handling.
    // JsonArray errors = root.createArray( "errors" );
    
    root["time"] = getTimeAsString( timeString );
    if( mlxPresent )
    {
      root["Sky Temperature"] = (float) skyTemperature;
      root["Ambient Temperature"] = (float) ambientTemperature;
    }
    else
      root["Message"] = "MLX90614 sensor not present";

    if( tslPresent )
    {
      root["IR"] = lum >> 16;
      root["Full"] = lum & 0x0FFFF;
      root["Brightness"] = lux;
    }
    else
      root["Message"] = "TLS2591 sensor not present";

    root.printTo( Serial ); Serial.println(" ");
    root.printTo( message );
    server.send(200, "application/json", message);  
 }

   /*
   * Web server handler functions
   */
  void handleNotFound()
  {
  String message = "URL not understood\n";
  message.concat( "Simple status read: http://");
  message.concat( myHostname );
  message.concat ( "\n");
  message.concat( "Sky temperature read: http://");
  message.concat( myHostname );
  message.concat ( "/skytemp \n");
  message.concat( "Sky brightness read: http://");
  message.concat( myHostname );
  message.concat ( "/skybrightness \n");

  server.send(404, "text/plain", message);
  }
 
  //Return sensor status
  void handleRoot()
  {
    String timeString = "", message = "";
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();

    root["time"] = getTimeAsString( timeString );
    //Todo add skyTemp and SQM reading here
    
    
    //root.printTo( Serial );
    root.printTo(message);
    server.send(200, "application/json", message);
  }

#endif
