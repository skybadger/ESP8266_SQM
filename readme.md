<h2>8266_SQM Environment sensor over WiFi connection </h2>
<p>This application runs on the ESP8266-01 wifi-enabled SoC device to capture sensor readings and transmit them to the local MQTT service. 
In my arrangement, Node-red flows are used to listen for and graph the updated readings in the dashboard UI. 
The unit is setup for I2C operation and is expecting to see SCL on GIO0 and SDA on GPIO2. </p>

<p>The light sensor is the TLS2591 which is set to auto range to keep flux readings sensibly high. 
The sky temperature sensor used is the Melexis 90614 which provides ambient and Sky temperature readings which determine whether cloud is present or not.</p>

<h3>Dependencies:</h3>
<ul><li>Arduino 1.8</li>
<li>ESP8266 V2.4+ lwip 1.9hb </li>
<li>Arduino MQTT client (https://pubsubclient.knolleary.net/api.html)</li>
<li>Arduino Mlexis 90614 sensor library </li>
<li>Arduino JSON library (pre v6) </li> </ul>

<h2>Testing</h2>
<p>Access by serial port - Tx only is available from device at 115,600 baud at 3.3v. THis provides debug output .</p>
<p>Wifi is used for MQTT reporting only and servicing web requests</p>
<p>Use http://ESPSQM01 to receive json-formatted output of current sensors. </p>

<h3>Use:</h3>
<p>I use mine to source a dashboard via hosting the mqtt server in node-red. It runs off a solar-panel supply in my observatory dome. The SQM data is used in an ASCOM Safety ALPACA server hosted in Node-red to determine whether the dome is safe and fit to open. </p>

<h3>ToDo:</h3>
<p>Clone the async MQTT reconnect to this code or move to library function and refer from there </p>
