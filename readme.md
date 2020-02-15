"ESP8266_SQM
environment sensor over WiFi connection" 
This application runs on the ESP8266-01 wifi-enabled SoC device to capture sensor readings and transmit them to the local MQTT service. 
In my arrangement, Node-red flows are used to listen for and graph the updated readings in the dashboard UI. 
The unit is setup for I2C operation and is expecting to see SCL on GIO0 and SDA on GPIO2. 

The light sensor is the TLS2591 which is set to auto range to keep flux readings sensibly high
The sky temperature sensor used is the Melexis 90614 which provides ambient and Sky temperature readings which determine whether cloud is present or not.

Dependencies:
Arduino 1.8, 
ESP8266 V2.4+ lwip1.9hb 
Arduino MQTT client (https://pubsubclient.knolleary.net/api.html)
Arduino DHT111 sensor library (https://github.com/beegee-tokyo/DHTesp/archive/master.zip)
Arduino JSON library (pre v6) 

Testing
Access by serial port  - Tx only is available from device at 115,600 baud at 3.3v. THis provides debug output .
Wifi is used for MQTT reporting only and servicing web requests
Use http://ESPTHM01 to receive json-formatted output of current sensors. 

Use:
I use mine to source a dashboard via hosting the mqtt server in node-red. It runs off a solar-panel supply in my observatory dome. 

ToDo:
clone the async MQTT reconnect to ther code or move to library