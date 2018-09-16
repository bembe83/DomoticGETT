// Do not remove the include below
#include "DomoticGETT.h"


//The setup function is called once at startup of the sketch
void setup()
{
	EEPROM.begin(4096);
	SPIFFS.begin();

	Serial.begin(SERIAL_BAUD);
	delay(1000);

	Wire.begin(pinSDA, pinSCL);

	setupInitRelayStatus();
	initialisePins();
	setupDHT11();
	readRelayList();
	//listSPIFFSfiles();

	server.on("/style.css", [](){handleFileRead("/style.css", server);});
	server.on("/jquery-ui.css", [](){handleFileRead("/jquery-ui.css", server);});
	server.on("/jquery.min.js", [](){handleFileRead("/jquery.min.js", server);});
	server.on("/device.json", [](){handleFileRead("/device.json", server);});
	server.on("/relays.json", [](){handleFileRead("/relays.json", server);});
	server.on("/config", handleConfigCall);
	server.on("/reset", handleResetCall);
	server.on("/cleanclient", handleResetClient);
	server.on("/getStatus", handleStatusCall);

	iConnectTry = 0;

	if (readHostName())
		WiFi.hostname(hostName);
	else
		WiFi.hostname("deviceAP");

	Serial.println("\nRetrieving SSID and Password from EEPROM");
	if (readWifiCredential())
	{
		WiFi.mode(WIFI_STA);
		Serial.print("Connecting to SSID ");
		Serial.println(ssid);
		WiFi.begin(ssid.c_str(), password.c_str());

		// Wait for connection
		while (WiFi.status() != WL_CONNECTED && iConnectTry <= 30) {
			Serial.print(".");
			delay(500);
			iConnectTry++;
		}

		if (iConnectTry <= 30)
		{
			isAP = false;
			Serial.print(".");
			digitalWrite(gpioLed, LOW);

			Serial.println("");
			Serial.print("Connected to ");
			Serial.println(ssid);
			Serial.print("IP address: ");
			Serial.println(WiFi.localIP());
			Serial.print("MAC address: ");
			Serial.println(WiFi.macAddress());

			digitalWrite(gpioLed, HIGH);

			if (mdns.begin("esp8266", WiFi.localIP()))
			{
				httpUpdater.setup(&server);
				Serial.println("MDNS responder started");
			}
			server.on("/", handleHttpCall);
		}
		else
			isAP = true;
	}
	else
		isAP = true;

	if(isAP)
	{
		Serial.println("No Credential previously saved so entering AP mode.");
		uint8_t mac[WL_MAC_ADDR_LENGTH];
		WiFi.softAPmacAddress(mac);
		String macID = String(mac[WL_MAC_ADDR_LENGTH - 3], HEX) +
				String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
				String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
		macID.toUpperCase();
		String AP_NameString = defAPSSID + macID;

		isAP = true;
		WiFi.mode(WIFI_AP);
		WiFi.softAPConfig(local_IP, gateway, subnet);
		WiFi.softAP(AP_NameString.c_str());
		Serial.print("Access Point ");
		Serial.print(AP_NameString);
		Serial.print(" started with address:");
		Serial.println(WiFi.softAPIP());
		server.on("/", handleConfigCall);
	}

	iConnectTry = 0;
	server.begin();
	Serial.println("HTTP server started");

}

// The loop function is called in an endless loop
void loop()
{
	  bSwitch.update();
	  bReset.update();

	  if (bSwitch.fell() || bReset.fell())
	  {
	    digitalWrite(gpioButton, HIGH);
	    digitalWrite(gpioReset, HIGH);
	    flipStatus();
	    setLocalStatus();
	    for (int i = 0; i < iCountRelay; i++)
	      setRelayStatus(strRelay[i]);
	  }

	  if(iConnectTry > 10)
	  {
	    iConnectTry = 0;
	    if(!dht11.read(pinDHT11, &bTemperature, &bHumidity, NULL))
	    {
	      Serial.print("Temperature: ");
	      Serial.print(bTemperature);
	      Serial.print("*C - Humidity:");
	      Serial.print(bHumidity);
	      Serial.println("% ");
	    }
	    else
	    {
	    	Serial.println("No DHT11 detected");
	    }
	  }
	  else
	    iConnectTry++;

	  server.handleClient();
}
