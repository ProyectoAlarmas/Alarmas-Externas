#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <Arduino.h>

// ===================== WiFi =====================
const char* ssid = "moto";
const char* password = "12345678";

// ===================== MQTT =====================
const char *mqtt_broker = "d128f885.ala.us-east-1.emqxsl.com";
const char *mqtt_topic  = "esp32/External_Alarms_2";
const char *mqtt_user   = "Hola";
const char *mqtt_pass   = "12345";
const int   mqtt_port   = 8883;

// ===================== Certificado =====================
const char* root_ca = \ 
"-----BEGIN CERTIFICATE-----\n" \
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n" \
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n" \
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n" \
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n" \
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n" \
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n" \
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n" \
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n" \
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n" \
"1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n" \
"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n" \
"Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n" \
"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n" \
"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n" \
"MrY=\n" \
"-----END CERTIFICATE-----\n" ;

// ===================== Pines =====================
const int total_pines = 16;

// Entradas de alarmas
const uint8_t pinList[total_pines] = {
  36, 39, 34, 35,
  32, 33, 25, 26,
  27, 14, 13, 21,
  19, 18, 17, 16
};

// Pines con lógica invertida (índices dentro de pinList)
const bool invertedPin[total_pines] = {
  false, false, false, false,
  false, false, false, false,
  false, false, false, false,
  true,  true,  true,  true   // 34, 35, 36, 39
};

// LEDs
const uint8_t LED_POWER = 23;
const uint8_t LED_WIFI  = 22;

WiFiClientSecure wifiSecureClient;
PubSubClient client(wifiSecureClient);

// ===================== WiFi =====================
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }

  Serial.println("\nWiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ===================== MQTT =====================
void reconnect() {
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());

    Serial.print("Conectando a EMQX MQTT... ");
    if (client.connect(client_id.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Conectado");
    } else {
      Serial.print("Falló, rc=");
      Serial.print(client.state());
      Serial.println(" intentando en 5 seg...");
      delay(5000);
    }
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);

  // LEDs
  pinMode(LED_POWER, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);

  digitalWrite(LED_POWER, HIGH); // LED de poder encendido
  digitalWrite(LED_WIFI, LOW);

  // Entradas (todas PULLDOWN físicos)
  for (int i = 0; i < total_pines; i++) {
    pinMode(pinList[i], INPUT);
  }

  initWiFi();

  digitalWrite(LED_WIFI, HIGH); // WiFi conectado

  wifiSecureClient.setCACert(root_ca);
  client.setServer(mqtt_broker, mqtt_port);
}

// ===================== LOOP =====================
void loop() {

  // Estado WiFi → LED
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_WIFI, HIGH);
  } else {
    digitalWrite(LED_WIFI, LOW);
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Leer pines y armar uint16_t
  uint16_t pinStates = 0;

  for (int i = 0; i < total_pines; i++) {
    bool state = digitalRead(pinList[i]);

    // Invertir lógica si aplica
    if (invertedPin[i]) {
      state = !state;
    }

    pinStates |= (state << i);
  }

  uint8_t payload[2];
  payload[0] = (pinStates >> 8) & 0xFF;
  payload[1] = pinStates & 0xFF;

  Serial.print("pinStates: ");
  Serial.print(pinStates, BIN);
  Serial.print(" (0x");
  Serial.print(pinStates, HEX);
  Serial.println(")");

  client.publish(mqtt_topic, payload, 2);

  delay(1000);
}
