#include <Arduino.h>
#include <EEPROM.h>         
#include <WiFi.h>
#include "esp_wpa2.h"       
#include <HTTPClient.h>
#include "driver/uart.h"
#include "esp_wifi.h" 

const int ADDR_SSID      = 0;
const int ADDR_IDENTITY  = 50;
const int ADDR_USERNAME  = 120;
const int ADDR_PASSWORD  = 170;
const int ADDR_APIKEY    = 240;

String ssid;
String eap_identity;
String eap_username;
String eap_password;
String api_key;

#define RX_PIN 3
#define TX_PIN 1

String leerStringDeEEPROM(int direccion); 
void setupUART_Sleep();
void leerYProcesarUART();
void enviarDatosThingSpeak(float d1, float d2, float d3, float d4, float d5);

void setup() {
  Serial.begin(9600);
  EEPROM.begin(512); 
  delay(10);

  Serial.println("\n--- Arrancando Programa (Modo WPA2 Enterprise desde EEPROM) ---");

  ssid         = leerStringDeEEPROM(ADDR_SSID);
  eap_identity = leerStringDeEEPROM(ADDR_IDENTITY);
  eap_username = leerStringDeEEPROM(ADDR_USERNAME);
  eap_password = leerStringDeEEPROM(ADDR_PASSWORD);
  api_key      = leerStringDeEEPROM(ADDR_APIKEY);

  if (ssid.length() == 0) {
    Serial.println("Error crítico: EEPROM vacía. Asegúrate de haber guardado las credenciales previamente.");
    while (true) delay(1000);
  }

  Serial.flush(); 

  setupUART_Sleep();

  delay(50);      

  Serial.print("\nConectando a la red Wi-Fi: ");
  Serial.println(ssid);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)eap_identity.c_str(), eap_identity.length());
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)eap_username.c_str(), eap_username.length());
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)eap_password.c_str(), eap_password.length());
  esp_wifi_sta_wpa2_ent_enable();
  
  WiFi.begin(ssid.c_str());

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    intentos++;
    if (intentos > 40) {
      Serial.println("\nNo se pudo establecer conexión. Reiniciando hardware...");
      ESP.restart();
    }
  }

  Serial.println("\n¡Conexión Wi-Fi establecida!");
  Serial.print("Dirección IP asignada: ");
  Serial.println(WiFi.localIP());

  esp_wifi_set_ps(WIFI_PS_MIN_MODEM); 
}

void loop() {
  Serial.flush();
  Serial.println("\nEntrando en modo Light Sleep...");
  delay(50); 

  esp_sleep_enable_uart_wakeup(UART_NUM_0);
  esp_light_sleep_start(); 

  Serial.println("\n¡Despertado por interrupción de hardware UART0!");
  
  delay(20); 
  while (Serial.available()) {
    Serial.read();
  }

  Serial.println("Esperando el paquete de datos del Emisor...");
  unsigned long inicioEspera = millis();
  bool datosRecibidos = false;

  while (millis() - inicioEspera < 600) {
    if (Serial.available()) {
      leerYProcesarUART();
      datosRecibidos = true;
      break;
    }
    delay(10);
  }

  if (!datosRecibidos) {
    Serial.println("Aviso: Despertar falso (ruido) o el Emisor no envió datos a tiempo.");
  }
}

String leerStringDeEEPROM(int direccion) {
  String cadena = "";
  char caracter = EEPROM.read(direccion);
  int i = 0;
  while (caracter != '\0' && i < 100) {
    cadena += caracter;
    i++;
    caracter = EEPROM.read(direccion + i);
  }
  return cadena;
}

void setupUART_Sleep() {
  uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity    = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 122,
      .source_clk = UART_SCLK_REF_TICK
  };
  
  uart_param_config(UART_NUM_0, &uart_config);
  uart_set_wakeup_threshold(UART_NUM_0, 3);
}

void leerYProcesarUART() {
  if (Serial.available()) {
    String datos = Serial.readStringUntil('\n');
    datos.trim();

    Serial.print("Cadena bruta recibida: ");
    Serial.println(datos);

    int facil = 0, normal = 0, dificil = 0, puntaje = 0, equivocaciones = 0;
    int parseados = sscanf(datos.c_str(), "%*[^0-9]%d,%d,%d,%d,%d", &facil, &normal, &dificil, &puntaje, &equivocaciones);

    if (parseados != 5) {
        parseados = sscanf(datos.c_str(), "%d,%d,%d,%d,%d", &facil, &normal, &dificil, &puntaje, &equivocaciones);
    }

    if (parseados == 5) {
      Serial.println("Datos UART procesados correctamente.");
      enviarDatosThingSpeak((float)facil, (float)normal, (float)dificil, (float)puntaje, (float)equivocaciones);
    } else {
      Serial.println("Error: Estructura de paquete UART corrupta tras el despertar.");
    }
  }
}

void enviarDatosThingSpeak(float d1, float d2, float d3, float d4, float d5) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado. Forzando reconexión...");
    WiFi.disconnect();
    
    WiFi.begin(ssid.c_str()); 
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      timeout++;
      Serial.print(".");
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.thingspeak.com/update?api_key=" + api_key + 
                 "&field1=" + String(d1) + "&field2=" + String(d2) +
                 "&field3=" + String(d3) + "&field4=" + String(d4) +
                 "&field5=" + String(d5);
    
    int maxReintentos = 3;
    int intento = 0;
    int httpResponseCode = -1;

    while (intento < maxReintentos) {
      http.begin(url);
      httpResponseCode = http.GET();
      
      if (httpResponseCode > 0) {
        Serial.printf("Canal actualizado con éxito. Código del servidor: %d\n", httpResponseCode);
        http.end();
        break;
      } else {
        Serial.printf("Intento %d fallido. Código: %d\n", intento + 1, httpResponseCode);
        http.end();
        
        if (httpResponseCode == -1) {
          Serial.println("La red aún no está lista. Esperando 1 segundo para reintentar...");
        }
        delay(1000);
        intento++;
      }
    }

    if (httpResponseCode <= 0) {
      Serial.println("Fallo definitivo tras agotar los reintentos.");
    }
  } else {
    Serial.println("Imposible transmitir: Pérdida permanente de señal Wi-Fi.");
  }
}