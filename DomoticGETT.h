// Only modify this file to include
// - function definitions (prototypes)
// - include files
// - extern variable definitions
// In the appropriate section

#ifndef _DomoticGETT_H_
#define _DomoticGETT_H_
#include "Arduino.h"
//add your includes for the project DomoticGETT here
#include <ArduinoOTA.h>
#include <ESP.h>
#include <Bounce2.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SimpleDHT.h>
#include <Wire.h>
#include <Domotic.h>
//end of add your includes here
MDNSResponder mdns;
ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer server(HTTP_PORT);
SimpleDHT11 dht11;

String strWifiFile = "/wifi.json";
String strDeviceFile = "/device.json";
String strReleaysFile = "/relays.json";

String ssid    = "";
String password = "";
String hostName = "";

String ssidParam = "ssid";
String passParam = "password";
String hostParam = "hostname";
String statusParam = "status";

const int PINS_COUNT = 4;
const int BTN_COUNT = 2;
const int BUTTON_INT = 5;

int gpioReset  = 0;
int pinDHT11   = 4;
int pinSDA     = 9;
int pinSCL     = 10;
int gpioRelay  = 12;
int gpioLed    = 13;
int gpioButton = 14;

int relayStatus = LOW;

int iPins[PINS_COUNT] = {gpioLed, gpioRelay, gpioButton, gpioReset};
int iPinsFunction[PINS_COUNT] = {OUTPUT, OUTPUT, INPUT_PULLUP, INPUT_PULLUP};
int iPinsStatus[PINS_COUNT] = {LOW, relayStatus, HIGH, HIGH};

int iBtn[BTN_COUNT] = {gpioButton, gpioReset};
int iBtnInt[BTN_COUNT] = {BUTTON_INT, BUTTON_INT};

boolean isAP = true;
boolean isDHTconnected = false;

float fTemperature;
float fHumidity;
byte bTemperature;
byte bHumidity;

Bounce bSwitch = Bounce();
Bounce bReset = Bounce();

Bounce bBtn[BTN_COUNT] = {bSwitch, bReset};

String strRelay[MAX_RELAY];
int iRelayStatus[MAX_RELAY];
int iCountRelay = 0;

int iConnectTry = 0;

//add your function definitions for the project DomoticGETT here

void setupInitRelayStatus()
{
	int iInitValue = readByte(INIT_STATUS_ADDR);
	if(iInitValue == LOW)
		relayStatus = LOW;
	else
		relayStatus = HIGH;
}

void initialisePins()
{
	iPinsStatus[1] = relayStatus;

	for (int i = 0; i < PINS_COUNT; i++)
	{
		pinMode(iPins[i], iPinsFunction[i]);
		digitalWrite(iPins[i], iPinsStatus[i]);
	}

	for (int i = 0; i < BTN_COUNT; i++)
	{
		bBtn[i].attach(iBtn[i]);
		bBtn[i].interval(iBtnInt[i]);
	}
}

void setupDHT11()
{
	if(dht11.read(pinDHT11, &bTemperature, &bHumidity, NULL))
	{
		isDHTconnected = false;
		Serial.print("DHT11 not connected to pin ");
		Serial.println(pinDHT11);
	}
	else
	{
		isDHTconnected = true;
		Serial.print("DHT11 connected to pin ");
		Serial.println(pinDHT11);
	}
}



void flipStatus()
{
	relayStatus = !relayStatus;
}

void setLocalStatus()
{
	digitalWrite(gpioLed, !relayStatus);
	digitalWrite(gpioRelay, relayStatus);
	delay(50);
}

void setRelayStatus(String strRelay)
{
	HTTPClient http;
	if (relayStatus == LOW)
		http.begin("http://" + strRelay + "/?Status=off");
	else
		http.begin("http://" + strRelay + "/?Status=on");

	//int httpCode = http.GET();
	http.end();
}


void getRelayStatus(int pos)
{
	HTTPClient http;
	http.begin("http://" + strRelay[pos] + "/getStatus");
	//int httpCode = http.GET();
	String strPayload = http.getString();
	StaticJsonBuffer<200> jsonBuffer; // @suppress("Abstract class cannot be instantiated")
	JsonObject& parsed = jsonBuffer.parseObject(strPayload); //Parse message
	if (!parsed.success()) {   //Check for errors in parsing
		Serial.println("Parsing failed");
		delay(5000);
	}
	else
	{
		iRelayStatus[pos] = parsed["status"];
	}

	http.end();
}

void setRelayStatus(int pos)
{
	HTTPClient http;
	if (relayStatus == LOW)
		http.begin("http://" + strRelay[pos] + "/?Status=off");
	else
		http.begin("http://" + strRelay[pos] + "/?Status=on");

	getRelayStatus(pos);
	//int httpCode = http.GET();
	http.end();
}

String getWebPage()
{
	String strWebPage = readTextFile("/index.html");

	if(strWebPage.length() > 0)
	{
		String strRelaysList = "";

		for (int i = 0; i < iCountRelay; i++)
			strRelaysList += "\t\t\t\t<TR><FORM method=\"POST\" action =\"\"><TD class=\"clients_table_cell\"><INPUT type=\"hidden\" name=\"delrelay\" value =\"" + strRelay[i] + "\"/><LABEL value =\"" + strRelay[i] + "\">" + strRelay[i] + "</LABEL></TD><TD class=\"clients_table_cell\"><BUTTON type=\"Submit\"> - DELETE</BUTTON></TD></FORM></TR>\n";

		if (relayStatus == LOW)
		{
			strWebPage.replace("<<STATUS>>", "OFF");
			strWebPage.replace("<<COLOR>>", "red");
		}
		else
		{
			strWebPage.replace("<<STATUS>>", "ON");
			strWebPage.replace("<<COLOR>>", "green");
		}

		strWebPage.replace("<<HOSTNAME>>", hostName);
		strWebPage.replace("<<RELAYS>>", strRelaysList);
	}
	else
	{
		strWebPage = "<!DOCTYPE html>\n<HTML>\n"
				"\t<TITLE>GETT Domotic System</TITLE>\n"
				"\t<BODY align = \"center\">\n"
				"index.html file not found"
				"\t</BODY>\n"
				"</HTML>";
	}
	return strWebPage;
}

String getConfigPage()
{
	String strConf = readTextFile("/index_config.html");

	if(strConf != "")
	{
		strConf.replace("<<HOSTNAME>>", hostName);
		strConf.replace("<<SSID>>", ssid);
		strConf.replace("<<PASSWORD>>", password);
	}
	else
	{
		strConf = "<!DOCTYPE html>\n<HTML>\n"
				"\t<TITLE>GETT Domotic System</TITLE>\n"
				"\t<BODY align = \"center\">\n"
				"index_config.html file not found"
				"\t</BODY>\n"
				"</HTML>";
	}
	return strConf;
}

boolean readSSID()
{
	//ssid = readString(SSID_ADDR);
	StaticJsonBuffer<200> jsonBuffer; // @suppress("Abstract class cannot be instantiated")
	String json = readTextFile(strWifiFile);
	ssid="";

	if(json.length()>0)
	{
		JsonObject& root = jsonBuffer.parseObject(json);
		if(sizeof(root)>0)
			ssid = String(root[ssidParam].as<const char*>());
	}

	if (ssid.length() <= 0)
		return false;
	else
		return true;
}

boolean readPassword()
{
	//password = readString(PASSWORD_ADDR);
	StaticJsonBuffer<200> jsonBuffer; // @suppress("Abstract class cannot be instantiated")
	String json = readTextFile(strWifiFile);
	password = "";

	if(json.length()>0)
	{
		JsonObject& root = jsonBuffer.parseObject(json);
		if(sizeof(root)>0)
			password = String(root[passParam].as<const char*>());
	}

	if (password.length() <= 0)
		return false;
	else
		return true;
}

boolean readHostName()
{
	//hostName = readString(HOSTNAME_ADDR);
	StaticJsonBuffer<200> jsonBuffer; // @suppress("Abstract class cannot be instantiated")
	String json = readTextFile(strDeviceFile);
	hostName = "";

	if(json.length()>0)
	{
		JsonObject& root = jsonBuffer.parseObject(json);
		root.printTo(Serial);
		if(sizeof(root)>0)
			hostName = String(root[hostParam].as<const char*>());
	}
	if (hostName.length() <= 0)
		return false;
	else
		return true;
}

void addRelay(String strName, boolean bAddDuplicates)
{
	Serial.print("Adding client ");
	Serial.print(strName);
	Serial.print(" at address ");
	Serial.println(RELAY_START_ADDR + (iCountRelay * STRING_BLOCK_LENGHT));

	int iOccur = 0;
	for (int i = 0; i < iCountRelay; i++)
	{
		if (strRelay[i].equals(strName))
			iOccur++;
	}

	if (strName.length() > 0 && (iOccur == 0 || bAddDuplicates))
	{
		strRelay[iCountRelay] = strName;
		saveString(RELAY_START_ADDR + (iCountRelay * STRING_BLOCK_LENGHT), strName);
		iCountRelay++;
		saveByte(RELAY_COUNT_ADDR, iCountRelay);
	}
	else
	{
		Serial.print("Relay ");
		Serial.print(strName);
		Serial.println(" already added");
	}
}

void emptyRelayList()
{
	for (int i = 0; i < MAX_RELAY; i++)
	{
		saveString(RELAY_START_ADDR + (i * STRING_BLOCK_LENGHT), "");
	}
	saveByte(RELAY_COUNT_ADDR, 0);
}

void saveRelayList()
{
	for (int i = 0; i < iCountRelay; i++)
	{
		saveString(RELAY_START_ADDR + (i * STRING_BLOCK_LENGHT), strRelay[i]);
	}
	saveByte(RELAY_COUNT_ADDR, iCountRelay);
}

void readRelayList()
{
	iCountRelay = readByte(RELAY_COUNT_ADDR);

	if(iCountRelay > MAX_RELAY)
	{
		emptyRelayList();
		iCountRelay = readByte(RELAY_COUNT_ADDR);
	}

	Serial.print("Stored ");
	Serial.print(iCountRelay);
	Serial.println(" clients");

	for (int i = 0; i < iCountRelay; i++)
	{
		strRelay[i] = readString(RELAY_START_ADDR + (i * STRING_BLOCK_LENGHT));
		iRelayStatus[i] = 0;//getRelayStatus(i);
		Serial.print("Client ");
		Serial.print(i + 1);
		Serial.print(": ");
		Serial.println(strRelay[i]);
	}
}

boolean removeRelay(String strName)
{
	boolean bFound = false;
	int i = 0;
	for (i = 0; i < iCountRelay && !bFound; i++)
	{
		if (strRelay[i].equals(strName))
			bFound = true;
	}

	for (i--; i < iCountRelay ; i++)
	{
		strRelay[i] = strRelay[i + 1];
	}

	if (bFound)
	{
		strRelay[iCountRelay - 1] = "";
		iCountRelay--;
	}

	emptyRelayList();
	saveRelayList();

	return bFound;
}

void handleHttpCall()
{
	String strStatus = "";

	if(server.args() >0)
	{
		for (int i = 0; i < server.args(); i++) {
			if (server.argName(i).equalsIgnoreCase("status"))
			{
				strStatus = server.arg(i);
				if (strStatus.equalsIgnoreCase("on"))
					relayStatus = HIGH;
				else if (strStatus.equalsIgnoreCase("off"))
					relayStatus = LOW;
				else if (strStatus.equalsIgnoreCase("flip"))
					flipStatus();

				setLocalStatus();
				for (int i = 0; i < iCountRelay; i++)
					setRelayStatus(strRelay[i]);
			}
			else if (server.argName(i).equalsIgnoreCase("addrelay"))
			{
				if (iCountRelay < MAX_RELAY)
				{
					addRelay(server.arg(i), false);
				}
				else
				{
					Serial.print("Reached the limit of ");
					Serial.print(MAX_RELAY);
					Serial.println(" elements to drive");
				}
			}
			else if (server.argName(i).equalsIgnoreCase("delrelay"))
			{
				if (removeRelay(server.arg(i)))
				{
					Serial.print("Relay removed");
				}
				else
				{
					Serial.println("Relay not found in the list");
				}
			}
		}
	}
	server.send(200, "text/html", getWebPage());
}

void handleConfigCall()
{
	String strSSID = "";
	String strPassword = "";
	String strHostName = "";
	String strWifiParam = "";
	String strHostParam = "";
	int iParamSet = 0;

	StaticJsonBuffer<200> jsonBufWifi; // @suppress("Abstract class cannot be instantiated")
	StaticJsonBuffer<200> jsonBufHost; // @suppress("Abstract class cannot be instantiated")

	Serial.println("ConfigCall");
	Serial.print("# of parameters: ");
	Serial.println(server.args());

	for (int i = 0; i < server.args(); i++) {
		if (server.argName(i).equalsIgnoreCase(ssidParam))
			strSSID = server.arg(i);
		else if (server.argName(i).equalsIgnoreCase(passParam))
			strPassword = server.arg(i);
		else if (server.argName(i).equalsIgnoreCase(strHostName))
			strHostName = server.arg(i);
	}

	JsonObject& rootWifi = jsonBufWifi.createObject();
	JsonObject& rootHost = jsonBufHost.createObject();

	if (strSSID.length() > 0)
	{
		//saveString(SSID_ADDR, strSSID);
		rootWifi[ssidParam] = strSSID;
		iParamSet++;
	}

	if (strPassword.length() > 0)
	{
		//saveString(PASSWORD_ADDR, strPassword);
		rootWifi[passParam] = strPassword;
		iParamSet++;
	}

	if (strHostName.length() > 0)
	{
		//saveString(HOSTNAME_ADDR, strHostName);
		rootHost[hostParam] = strHostName;
		rootHost[statusParam] = "0";
		iParamSet++;
	}

	Serial.print("Parameters set: ");
	Serial.println(iParamSet);
	if (iParamSet == 3)
	{
		rootWifi.printTo(strWifiParam);
		rootHost.printTo(strHostParam);
		Serial.println("Saving config files:");
		Serial.print(strWifiFile+"...");
		saveTextFile(strWifiFile, strWifiParam);
		Serial.println("Saved");
		Serial.print(strDeviceFile+"...");
		saveTextFile(strDeviceFile, strHostParam);
		Serial.println("Saved\nReboot");
		reboot();
	}
	else
	{
		server.send(200, (const char*)"text/html", getConfigPage());
	}
}

void handleResetCall()
{
	resetEEPROM();
	reboot();
}

void handleResetClient()
{
	emptyRelayList();
	readRelayList();
	server.send(200, "text/html", "<HTML><HEAD><meta http-equiv=\"refresh\" content=0; URL='http://" + String(WiFi.localIP()) + "'\" /></HEAD></BODY>");
}

void handleStatusCall()
{
	String strResponse = "";
	StaticJsonBuffer<200> jsonBuffer; // @suppress("Abstract class cannot be instantiated")
	JsonObject& root = jsonBuffer.createObject();
	root["status"] = relayStatus;
	root.prettyPrintTo(strResponse);
	server.send(200, "application/json", strResponse);
}

//Do not add code below this line
#endif /* _DomoticGETT_H_ */
