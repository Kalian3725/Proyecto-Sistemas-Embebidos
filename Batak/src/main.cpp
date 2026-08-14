#include <Arduino.h>
#include <Wire.h>
#include <PCF8575.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

const int pinBuzzer = 25; 
volatile bool reproducirMusica = true;
volatile bool pitidoError = false; 

#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247

#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494

#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_REST 0

#define NOTE_C6  1047
#define NOTE_D6  1175

const int melodiaFondo[] = {
  NOTE_E4, NOTE_GS4, NOTE_E4, NOTE_B4, NOTE_A4, NOTE_GS4, NOTE_A4, NOTE_E4, NOTE_E4, NOTE_GS4, NOTE_E4, NOTE_B4, NOTE_C5, NOTE_B4, NOTE_A4, NOTE_REST
};
const int durFondo[] = { 4, 8, 4, 4, 8, 8, 2, 4, 4, 8, 4, 4, 8, 8, 2, 4 };

const int melodiaCuenta[] = { 
  NOTE_E4, NOTE_REST, NOTE_E4, NOTE_REST, NOTE_E4, NOTE_REST, NOTE_A5 
  };
const int durCuenta[] = { 4, 4, 4, 4, 4, 4, 2 };

const int melodiaJuego[] = {
  NOTE_E5, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_C5, NOTE_B4, NOTE_A4, NOTE_A4, NOTE_C5, NOTE_E5, NOTE_D5, NOTE_C5, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_E5, NOTE_C5, NOTE_A4, NOTE_A4, NOTE_REST
};
const int durJuego[] = { 4, 8, 8, 4, 8, 8, 4, 8, 8, 4, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4 };

const int melodiaVictoria[] = {
  NOTE_E5, NOTE_G5, NOTE_C6, NOTE_G5, NOTE_E5, NOTE_G5, NOTE_C6, NOTE_G5, NOTE_F5, NOTE_A5, NOTE_D6, NOTE_A5, NOTE_F5, NOTE_A5, NOTE_D6, NOTE_A5
};
const int durVictoria[] = { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };

const int melodiaDerrota[] = {
  NOTE_A4, NOTE_F4, NOTE_E4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_E4, NOTE_REST
};
const int durDerrota[] = {
  2, 2, 2, 4, 4, 4, 1, 2
};

int ultimaResolucion = 4;

const int pinFCMax = 19;
const int pinFCMin = 18;

int dirArriba = HIGH;
int dirAbajo = LOW;

bool fcMinAccionado = false;

#define RX_PIN 16
#define TX_PIN 17

hd44780_I2Cexp lcd(0x27);
PCF8575 pcfLeds(0x20);
PCF8575 pcfBotones(0x21);

const int pinPaso1 = 27;
const int pinDireccion1 = 26;
const int pinEnable1 = 12;

const int pinDisparadorEsp = 14;
const int pinEcoEsp = 32;

const int pinBotonReset = 13; 
volatile bool peticionReset = false;

enum EstadoJuego { CALIBRACION, ESPERA, MENU, CUENTA_REGRESIVA, JUGANDO, DERROTA, VICTORIA, FIN_JUEGO, REAJUSTE };
EstadoJuego estadoActual = CALIBRACION;

int dificultadActual = 0;
const int intervalosDificultad[3] = {1000, 500, 250};
const char* nombresDificultad[3] = {"Facil", "Normal", "Dificil"};

int puntuacionTotal = 0;
int ledsGenerados = 0;
int cantidadLedsActivos = 0;
bool estaLedEncendido[12] = {false};
unsigned long tiempoEncendidoLed[12] = {0};

unsigned long sumaTiemposReaccion[3] = {0, 0, 0};
int aciertosDificultad[3] = {0, 0, 0};
int equivocaciones = 0;
bool datosEnviados = false;

unsigned long ultimoTiempoGeneracion = 0;
unsigned long tiempoInicioEstado = 0;
unsigned long tiempoUltimoMovimiento = 0;
float ultimaDistanciaReposo = 0;

float alturaMaxima = 0;
float alturaMinima = 0;
const float distanciaSensorUsuario = 10.0;

float pasosPorVuelta = 2200.0;
float distanciaInicial = 0;
float distanciaMovimiento = 0;
float diferenciaAltura = 0;

bool estadoEstable[13] = {false};
bool ultimoEstadoFisico[13] = {false};
unsigned long ultimoTiempoAntirrebote[13] = {0};

void tareaMusica(void * pvParameters) {
  int notaActual = 0;
  int cancionActual = -1;
  
  const int* pMelodia = melodiaFondo;
  const int* pDuraciones = durFondo;
  int longitudMelodia = 4;
  int tempoBase = 1000;

  while (true) {
    if (!reproducirMusica) {
      noTone(pinBuzzer);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    int cancionDeseada = 1; 
    switch (estadoActual) {
      case CALIBRACION: case ESPERA: case MENU: 
        cancionDeseada = 1; break;
      case CUENTA_REGRESIVA: 
        cancionDeseada = 2; break;
      case JUGANDO: 
        cancionDeseada = 3; break;
      case VICTORIA: 
        cancionDeseada = 4; ultimaResolucion = 4; break;
      case DERROTA: 
        cancionDeseada = 5; ultimaResolucion = 5; break;
      case FIN_JUEGO: case REAJUSTE: 
        cancionDeseada = ultimaResolucion; break;
    }

    if (cancionDeseada != cancionActual) {
      cancionActual = cancionDeseada;
      notaActual = 0;
      
      if (cancionActual == 1) {
        pMelodia = melodiaFondo; 
        pDuraciones = durFondo; 
        longitudMelodia = sizeof(melodiaFondo)/sizeof(melodiaFondo[0]); 
      } else if (cancionActual == 2) { 
        pMelodia = melodiaCuenta; 
        pDuraciones = durCuenta; 
        longitudMelodia = sizeof(melodiaCuenta)/sizeof(melodiaCuenta[0]); 
      } else if (cancionActual == 3) { 
        pMelodia = melodiaJuego; 
        pDuraciones = durJuego; 
        longitudMelodia = sizeof(melodiaJuego)/sizeof(melodiaJuego[0]); 
      } else if (cancionActual == 4) { 
        pMelodia = melodiaVictoria; 
        pDuraciones = durVictoria; 
        longitudMelodia = sizeof(melodiaVictoria)/sizeof(melodiaVictoria[0]); 
      } else if (cancionActual == 5) { 
        pMelodia = melodiaDerrota; 
        pDuraciones = durDerrota; 
        longitudMelodia = sizeof(melodiaDerrota)/sizeof(melodiaDerrota[0]); 
      }
    }

    if (cancionActual == 3) {
      tempoBase = 1000 - (dificultadActual * 250); 
    } else {
      tempoBase = 1000;
    }

    int duracionNota = tempoBase / pDuraciones[notaActual];
    if (pMelodia[notaActual] == NOTE_REST) {
      noTone(pinBuzzer);
    } else {
      tone(pinBuzzer, pMelodia[notaActual], duracionNota);
    }

    int pausaEntreNotas = duracionNota * 1.30;
    
    int tiempoEsperado = 0;
    bool interrumpido = false;

    while (tiempoEsperado < pausaEntreNotas) {
      if (pitidoError) {
        noTone(pinBuzzer);
        tone(pinBuzzer, 150, 150);
        vTaskDelay(150 / portTICK_PERIOD_MS); 
        pitidoError = false;
        interrumpido = true;
        break;
      }

      if (estadoActual == JUGANDO && cancionActual != 3) break; 
      if (estadoActual == VICTORIA && cancionActual != 4) break;
      if (estadoActual == DERROTA && cancionActual != 5) break;

      vTaskDelay(15 / portTICK_PERIOD_MS);
      tiempoEsperado += 15;
    }

    noTone(pinBuzzer);

    if (!interrumpido || pitidoError == false) { 
        notaActual++;
    }

    if (notaActual >= longitudMelodia) {
      if (cancionActual == 4 || cancionActual == 5) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
      }
      notaActual = 0;
    }
  }
}

void IRAM_ATTR isrReset() {
  peticionReset = true;
}

void reiniciarEstadisticas() {
  puntuacionTotal = 0;
  equivocaciones = 0;
  for (int i = 0; i < 3; i++) {
    sumaTiemposReaccion[i] = 0;
    aciertosDificultad[i] = 0;
  }
  datosEnviados = false;
}

float leerDistancia() {
  digitalWrite(pinDisparadorEsp, LOW);
  delayMicroseconds(2);
  digitalWrite(pinDisparadorEsp, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinDisparadorEsp, LOW);

  unsigned long duracion = pulseIn(pinEcoEsp, HIGH, 50000);
  if (duracion == 0) return -1.0;
  
  return (duracion * 0.0343) / 2.0;
}

void encenderMotor() {
  digitalWrite(pinEnable1, LOW);
  delay(2);
}

void apagarMotor() {
  digitalWrite(pinEnable1, HIGH);
}

void moverMotores(float distanciaCm) {
  if (abs(distanciaCm) < 0.5) return; 

  bool direccion = (distanciaCm > 0) ? dirAbajo : dirArriba; 
  digitalWrite(pinDireccion1, direccion);

  int pasos = abs(distanciaCm * pasosPorVuelta); 

  for (int i = 0; i < pasos; i++) {
    if (direccion == dirArriba && digitalRead(pinFCMax) == LOW) {
      break; 
    }
    if (direccion == dirAbajo && digitalRead(pinFCMin) == LOW) {
      fcMinAccionado = true;
      break; 
    }

    digitalWrite(pinPaso1, HIGH);
    delayMicroseconds(1000); 
    digitalWrite(pinPaso1, LOW);
    delayMicroseconds(1000);
  }
}

void irAlturaMaxima() {
  digitalWrite(pinDireccion1, dirArriba);
  fcMinAccionado = false; 
  
  while (digitalRead(pinFCMax) == HIGH) { 
    digitalWrite(pinPaso1, HIGH);
    delayMicroseconds(1000); 
    digitalWrite(pinPaso1, LOW);
    delayMicroseconds(1000);
    
    yield();

    if (digitalRead(pinFCMin) == LOW) {
      fcMinAccionado = true;
      break; 
    }
  }
}

bool obtenerBotonFisico(int id) {
  return (pcfBotones.digitalRead(id) == LOW);
}

bool leerBoton(int id) {
  bool presionado = false;
  bool estadoActualBoton = obtenerBotonFisico(id);
  
  if (estadoActualBoton != ultimoEstadoFisico[id]) {
    ultimoTiempoAntirrebote[id] = millis();
  }
  
  if ((millis() - ultimoTiempoAntirrebote[id]) > 50) {
    if (estadoActualBoton != estadoEstable[id]) {
      estadoEstable[id] = estadoActualBoton;
      if (estadoEstable[id] == true) presionado = true;
    }
  }
  
  if (estadoActualBoton == false) estadoEstable[id] = false;
  ultimoEstadoFisico[id] = estadoActualBoton;
  return presionado;
}

void encenderLed(int id, bool estado) {
  if (id < 12) { 
    pcfLeds.digitalWrite(id, estado ? LOW : HIGH);
    estaLedEncendido[id] = estado;
  }
}

void apagarTodosLosLeds() {
  for (int i = 0; i < 12; i++) encenderLed(i, false);
  cantidadLedsActivos = 0;
}

void manejarCalibracion() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrando...");
  
  Serial.println("\n--- INICIO DE CALIBRACION ---");
  Serial.print("Estado FC Min: "); Serial.println(digitalRead(pinFCMin));
  Serial.print("Estado FC Max: "); Serial.println(digitalRead(pinFCMax));

  if (digitalRead(pinFCMin) == LOW && digitalRead(pinFCMax) == LOW) {
    Serial.println("ERROR: Ambos botones leen LOW. Revisa el cableado o la logica (NC vs NO)");
    lcd.setCursor(0, 1);
    lcd.print("Error Botones");
    delay(2000);
    return;
  }

  lcd.setCursor(0, 1);
  lcd.print("Buscando sentido");
  Serial.println("Fase 1: Buscando sentido de giro...");

  bool direccionPrueba = HIGH;
  digitalWrite(pinDireccion1, direccionPrueba);

  delay(10);

  encenderMotor();
  while (digitalRead(pinFCMin) == HIGH && digitalRead(pinFCMax) == HIGH) {
    digitalWrite(pinPaso1, HIGH);
    delayMicroseconds(1000); 
    digitalWrite(pinPaso1, LOW);
    delayMicroseconds(1000);
    yield();
  }
  apagarMotor();

  if (digitalRead(pinFCMin) == LOW) {
    dirAbajo = direccionPrueba;
    dirArriba = !direccionPrueba;
    Serial.println("Toco FC Minimo primero.");
  } else if (digitalRead(pinFCMax) == LOW) {
    dirArriba = direccionPrueba;
    dirAbajo = !direccionPrueba;
    Serial.println("Toco FC Maximo primero.");
  }

  lcd.setCursor(0, 1);
  lcd.print("Buscando Minimo ");
  Serial.println("Fase 2: Viajando al tope Minimo...");
  
  digitalWrite(pinDireccion1, dirAbajo);
  encenderMotor();
  while (digitalRead(pinFCMin) == HIGH) {
    digitalWrite(pinPaso1, HIGH);
    delayMicroseconds(1000); 
    digitalWrite(pinPaso1, LOW);
    delayMicroseconds(1000);
    yield();
  }
  apagarMotor();
  
  delay(1000);
  alturaMinima = leerDistancia();
  Serial.print("Altura Minima guardada: "); Serial.println(alturaMinima);

  lcd.setCursor(0, 1);
  lcd.print("Buscando Maximo ");
  Serial.println("Fase 3: Viajando al tope Maximo...");

  digitalWrite(pinDireccion1, dirArriba);
  encenderMotor();
  while (digitalRead(pinFCMax) == HIGH) {
    digitalWrite(pinPaso1, HIGH);
    delayMicroseconds(1000); 
    digitalWrite(pinPaso1, LOW);
    delayMicroseconds(1000);
    yield();
  }
  apagarMotor();
  
  delay(1000);
  alturaMaxima = leerDistancia();
  Serial.print("Altura Maxima guardada: "); Serial.println(alturaMaxima);
  fcMinAccionado = false; 

  if (alturaMaxima > 0 && alturaMinima > 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Calibracion OK");
    Serial.println("Calibracion Finalizada con exito.");
    delay(2000);

    distanciaInicial = alturaMaxima; 
    tiempoUltimoMovimiento = millis();
    ultimaDistanciaReposo = alturaMaxima;
    diferenciaAltura = 0;
    
    estadoActual = ESPERA;
    lcd.clear();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error sensor");
    Serial.println("Error: El sensor ultrasonico no registro bien la distancia.");
    delay(500);
  }
}

void mostrarMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Selecciona dif:");
  lcd.setCursor(0, 1);
  lcd.print(nombresDificultad[dificultadActual]);
}

void iniciarRonda() {
  apagarTodosLosLeds();
  ledsGenerados = 0;
  datosEnviados = false;
  estadoActual = CUENTA_REGRESIVA; 
  tiempoInicioEstado = millis();
  lcd.clear();
}

void manejarEspera() {
  static int conteoConfirmaciones = 0;
  static float distanciaReferencia = 0;
  static unsigned long tiempoUltimaLectura = 0;

  lcd.setCursor(0, 0);
  lcd.print("Esperando");
  lcd.setCursor(0, 1);
  lcd.print("jugador...");

  float distanciaActualMedida = leerDistancia();
  if (distanciaActualMedida > 0) {
    if (abs(distanciaActualMedida - ultimaDistanciaReposo) > 5.0) {
      tiempoUltimoMovimiento = millis();
      ultimaDistanciaReposo = distanciaActualMedida;
    }
  }

  if (millis() - tiempoUltimoMovimiento > 30000) {
    lcd.setCursor(0, 0);
    lcd.print("Activando modo");
    lcd.setCursor(0, 1);
    lcd.print("light sleep.");
    delay(1000);
    lcd.clear();
    
    while (true) {
      esp_sleep_enable_timer_wakeup(3000000); 
      esp_light_sleep_start();
      
      float distSleep = leerDistancia();
      if (distSleep > 0 && abs(distSleep - ultimaDistanciaReposo) > 5.0) {
        tiempoUltimoMovimiento = millis();
        ultimaDistanciaReposo = distSleep;
        lcd.print("Saliendo de modo");
        lcd.setCursor(0, 1);
        lcd.print("light Sleep.");
        lcd.clear();
        break; 
      }
    }
  }

  if (distanciaActualMedida > 5.0) {
    float diferencia = distanciaActualMedida - alturaMaxima;

    if (conteoConfirmaciones == 0) {
      if (abs(diferencia) > 5.0) {
        distanciaReferencia = distanciaActualMedida;
        conteoConfirmaciones = 1;
        tiempoUltimaLectura = millis();
      }
    } else {
      if (millis() - tiempoUltimaLectura >= 1000) {
        if (abs(distanciaActualMedida - distanciaReferencia) <= 5.0) {
          conteoConfirmaciones++;
          tiempoUltimaLectura = millis();

          if (conteoConfirmaciones == 3) {
            float alturaUsuario = alturaMaxima - distanciaReferencia;

            if (alturaUsuario <= 150.0) {
              diferenciaAltura = alturaMinima - alturaMaxima;
            } else {
              float nuevaAltura = alturaUsuario + distanciaSensorUsuario;
              diferenciaAltura = nuevaAltura - alturaMaxima;
            }

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Jugador Conf");
            lcd.setCursor(0, 1);
            lcd.print("Ajustando ");
            lcd.print(diferenciaAltura);
            encenderMotor();
            moverMotores(diferenciaAltura);
            apagarMotor();
            delay(2000);

            conteoConfirmaciones = 0;
            estadoActual = MENU;
            mostrarMenu();
          }
        } else {
          conteoConfirmaciones = 0;
        }
      }
    }
  }
  delay(100); 
}

void manejarReajuste() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Volviendo a la");
  lcd.setCursor(0, 1);
  lcd.print("Posicion inicial");
  
  if (fcMinAccionado) {
    irAlturaMaxima();
  } else {
    moverMotores(-diferenciaAltura);
  }

  diferenciaAltura = 0;
  fcMinAccionado = false; 
  
  tiempoUltimoMovimiento = millis();
  ultimaDistanciaReposo = leerDistancia();

  estadoActual = ESPERA;
  delay(1000);
  lcd.clear();
}

void manejarMenu() {
  if (leerBoton(0)) { 
    dificultadActual = (dificultadActual == 0) ? 2 : dificultadActual - 1;
    mostrarMenu();
  }
  if (leerBoton(2)) { 
    dificultadActual = (dificultadActual == 2) ? 0 : dificultadActual + 1;
    mostrarMenu();
  }
  if (leerBoton(1)) { 
    puntuacionTotal = 0;
    iniciarRonda();
  }
}

void manejarCuentaRegresiva() {
  unsigned long transcurrido = millis() - tiempoInicioEstado;
  int tiempoRestante = 3 - (transcurrido / 1000);

  if (tiempoRestante > 0) {
    lcd.setCursor(0, 0);
    lcd.print("Iniciando en: ");
    lcd.setCursor(0, 1);
    lcd.print(tiempoRestante);
    lcd.print("s");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("A jugar!");
    lcd.setCursor(0, 1);
    lcd.print("Puntos: ");
    lcd.print(puntuacionTotal);
    ultimoTiempoGeneracion = millis(); 
    estadoActual = JUGANDO;
  }
}

void manejarJuego() {
  unsigned long tiempoActual = millis();
  int intervalo = intervalosDificultad[dificultadActual];

  if (tiempoActual - ultimoTiempoGeneracion >= intervalo && ledsGenerados < 30) {
    int ledAleatorio;
    do {
      ledAleatorio = random(0, 12); 
    } while (estaLedEncendido[ledAleatorio]); 

    encenderLed(ledAleatorio, true);
    tiempoEncendidoLed[ledAleatorio] = tiempoActual;
    cantidadLedsActivos++;
    ledsGenerados++;
    ultimoTiempoGeneracion = tiempoActual;
  }

  if (cantidadLedsActivos >= 6) {
    estadoActual = DERROTA;
    tiempoInicioEstado = millis();
    apagarTodosLosLeds();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fallaste!");
    return;
  }

  if (ledsGenerados == 30 && cantidadLedsActivos == 0) {
    estadoActual = VICTORIA;
    tiempoInicioEstado = millis();
    apagarTodosLosLeds();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Victoria!");
    return;
  }

  for (int i = 0; i < 12; i++) { 
    if (leerBoton(i)) {
      if (estaLedEncendido[i]) {
        unsigned long tiempoReaccion = tiempoActual - tiempoEncendidoLed[i];
        encenderLed(i, false);
        cantidadLedsActivos--;
        
        sumaTiemposReaccion[dificultadActual] += tiempoReaccion;
        aciertosDificultad[dificultadActual]++;

        if (tiempoReaccion <= (intervalo * 0.5)) {
          puntuacionTotal += 10; 
        } else if (tiempoReaccion <= (intervalo * 0.9)) {
          puntuacionTotal += 5;  
        } 
      } else {
        puntuacionTotal -= 5;
        equivocaciones++;
        pitidoError = true;
      }
      lcd.setCursor(8, 1);
      lcd.print(puntuacionTotal);
      lcd.print("    "); 
    }
  }
}

void manejarVictoria() {
  unsigned long transcurrido = millis() - tiempoInicioEstado;
  int tiempoRestante = 5 - (transcurrido / 1000);

  if (dificultadActual == 2) {
    if (transcurrido > 3000) { estadoActual = FIN_JUEGO; tiempoInicioEstado = millis(); }
    return;
  }

  if (tiempoRestante > 0) {
    lcd.setCursor(0, 1);
    lcd.print("Continuar? ");
    lcd.print(tiempoRestante);
    lcd.print("s");
    if (leerBoton(1)) { 
      dificultadActual++;
      lcd.clear();
      iniciarRonda(); 
    }
  } else {
    lcd.clear();
    estadoActual = FIN_JUEGO;
    tiempoInicioEstado = millis();
  }
}

void manejarDerrota() {
  if (millis() - tiempoInicioEstado > 3000) {
    lcd.clear();
    estadoActual = FIN_JUEGO;
    tiempoInicioEstado = millis();
  }
}

void manejarFinDelJuego() {
  lcd.setCursor(0, 0);
  lcd.print("Puntaje Total:");
  lcd.setCursor(0, 1);
  lcd.print(puntuacionTotal);
  lcd.print(" pts");
  dificultadActual = 0;

  if (!datosEnviados) {
    Serial.println("\n[1] Despertando al ESP32 receptor...");
    Serial2.print("U"); 
    Serial2.flush();

    Serial.println("[2] Dando tiempo al receptor para salir del Light Sleep...");
    delay(250);

    int promF = aciertosDificultad[0] > 0 ? sumaTiemposReaccion[0] / aciertosDificultad[0] : 0;
    int promN = aciertosDificultad[1] > 0 ? sumaTiemposReaccion[1] / aciertosDificultad[1] : 0;
    int promD = aciertosDificultad[2] > 0 ? sumaTiemposReaccion[2] / aciertosDificultad[2] : 0;

    String datos = String(promF) + "," + String(promN) + "," + String(promD) + "," + 
                   String(puntuacionTotal) + "," + String(equivocaciones);

    Serial.print("[3] Enviando paquete de datos: ");
    Serial.println(datos);

    Serial2.println(datos);
    Serial2.flush();
    
    datosEnviados = true;
  }

  if (millis() - tiempoInicioEstado > 10000) {
    estadoActual = REAJUSTE;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  Wire.begin(21, 22);

  int estadoLcd = lcd.begin(16, 2);
  if (estadoLcd != 0) {
    Serial.println("Error al iniciar la LCD.");
    while(1);
  }

  pcfLeds.begin();
  pcfBotones.begin();

  pinMode(pinFCMax, INPUT_PULLUP);
  pinMode(pinFCMin, INPUT_PULLUP);

  pinMode(pinPaso1, OUTPUT);
  pinMode(pinDireccion1, OUTPUT);
  pinMode(pinEnable1, OUTPUT);

  pinMode(pinDisparadorEsp, OUTPUT); 
  pinMode(pinEcoEsp, INPUT); 

  pinMode(pinBotonReset, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinBotonReset), isrReset, FALLING);
  
  for(int i = 0; i < 12; i++) {
    pcfLeds.pinMode(i, OUTPUT);
    pcfLeds.digitalWrite(i, HIGH); 
  }
  
  for(int i = 0; i < 12; i++) pcfBotones.pinMode(i, INPUT);

  delay(200);
  for(int i = 0; i < 12; i++){
    ultimoEstadoFisico[i] = obtenerBotonFisico(i);
    estadoEstable[i] = ultimoEstadoFisico[i];
  }

  pinMode(pinBuzzer, OUTPUT);
  xTaskCreatePinnedToCore(
    tareaMusica,
    "TareaMusica",
    2048,
    NULL,
    1,
    NULL,
    0
  );

  apagarTodosLosLeds();
  apagarMotor();
  randomSeed(analogRead(34)); 
}

void loop() {
  if (peticionReset) {
    peticionReset = false;
    reiniciarEstadisticas();
    apagarTodosLosLeds();
    dificultadActual = 0;
    estadoActual = MENU;
    mostrarMenu();
  }

  switch (estadoActual) {
    case CALIBRACION:      manejarCalibracion(); break;
    case ESPERA:           manejarEspera(); break;
    case MENU:             manejarMenu(); break;
    case CUENTA_REGRESIVA: manejarCuentaRegresiva(); break;
    case JUGANDO:          manejarJuego(); break;
    case DERROTA:          manejarDerrota(); break;
    case VICTORIA:         manejarVictoria(); break;
    case FIN_JUEGO:        manejarFinDelJuego(); break;
    case REAJUSTE:         manejarReajuste(); break;
  }
}