#include <WiFi.h>              // Biblioteca para gestionar la conectividad WiFi del ESP32
#include <WebServer.h>         // Biblioteca que permite crear un servidor web en el ESP32
#include <DHT.h>               // Biblioteca para interactuar con el sensor DHT (Humedad y Temperatura)
#include <OneWire.h>           // Biblioteca para comunicación con sensores OneWire, como el DS18B20
#include <DallasTemperature.h> // Biblioteca para el sensor de temperatura DS18B20

// Configuración del WiFi
const char *ssid = "xxxx";     // Nombre de la red WiFi
const char *password = "xxxx"; // Contraseña de la red WiFi

// Configuración del servidor web en el puerto 80
WebServer server(80); // Instancia del servidor web en el puerto 80

// Configuración del DHT11
#define DHTPIN 14         // Pin GPIO del ESP32 donde está conectado el DHT11
#define DHTTYPE DHT11     // Especifica el tipo de sensor DHT
DHT dht(DHTPIN, DHTTYPE); // Se crea una instancia de la clase DHT

// Configuración del DS18B20
#define ONE_WIRE_BUS 15              // Pin GPIO del ESP32 donde está conectado el DS18B20
OneWire oneWire(ONE_WIRE_BUS);       // Configuración del bus OneWire para el sensor de temperatura
DallasTemperature sensors(&oneWire); // Creación de la instancia DallasTemperature

// Configuración del sensor de caudal YF-S401
#define FLOW_SENSOR_PIN 4      // Pin GPIO donde está conectado el sensor de caudal
volatile int pulseCount = 0;   // Variable que almacena el número de pulsos del sensor de caudal
float calibrationFactor = 7.5; // Factor de calibración para convertir pulsos a caudal (L/min)

unsigned long oldTime = 0; // Variable para almacenar el último tiempo registrado
float flowRate = 0;        // Caudal en litros por minuto

// Función de interrupción para contar los pulsos del caudalímetro
void IRAM_ATTR countPulses()
{
  pulseCount++; // Incrementa el contador de pulsos cada vez que se detecta una interrupción
}

void setup()
{
  // Inicializar la comunicación serial
  Serial.begin(115200); // Configura la tasa de baudios para la comunicación serial (115200 bps)

  // Inicializar el sensor DHT11
  dht.begin(); // Comienza la lectura del sensor de humedad y temperatura DHT11
  Serial.println("Inicializando DHT11...");

  // Inicializar el sensor DS18B20
  sensors.begin(); // Inicia la comunicación con el sensor DS18B20
  Serial.println("Inicializando DS18B20...");

  // Configurar el pin del sensor de caudal como entrada
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);                                       // Configura el pin como entrada con resistencia pull-up interna
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), countPulses, RISING); // Configura una interrupción para contar pulsos cuando hay un flanco ascendente

  // Conectar a la red WiFi
  Serial.println("Conectando a la red WiFi...");
  WiFi.begin(ssid, password); // Conecta el ESP32 a la red WiFi

  // Espera hasta que el ESP32 se conecte a la red
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Conectando...");
  }

  Serial.println("Conectado a la red WiFi");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP()); // Imprime la dirección IP asignada al ESP32

  // Configurar las rutas del servidor web para la API REST
  server.on("/api/data", handleApiData); // Configura la ruta "/api/data" para devolver datos en formato JSON
  server.begin();                        // Inicia el servidor web
  Serial.println("Servidor API iniciado");
}

void loop()
{
  // Ejecutar el servidor web
  server.handleClient(); // Gestiona las solicitudes entrantes al servidor web

  // Leer datos del DHT11
  float h = dht.readHumidity();     // Lee la humedad desde el sensor DHT11
  float t1 = dht.readTemperature(); // Lee la temperatura desde el sensor DHT11

  // Leer datos del DS18B20
  sensors.requestTemperatures();         // Envía una solicitud de temperatura al sensor DS18B20
  float t2 = sensors.getTempCByIndex(0); // Obtiene la temperatura del DS18B20 en grados Celsius

  // Calcular el caudal
  unsigned long currentTime = millis(); // Obtiene el tiempo actual en milisegundos desde que el ESP32 comenzó a correr
  if (currentTime - oldTime > 1000)
  {                                              // Realiza el cálculo de caudal cada segundo
    flowRate = (pulseCount / calibrationFactor); // Calcula el caudal en litros por minuto
    pulseCount = 0;                              // Reinicia el contador de pulsos para la próxima medición
    oldTime = currentTime;
  }

  // Verifica si hubo error en la lectura del DHT11
  if (isnan(h) || isnan(t1))
  {
    Serial.println("Error al leer el DHT11"); // Imprime un mensaje de error si no se pudo leer el DHT11
  }
  else
  {
    // Muestra los valores de humedad y temperatura leídos
    Serial.print("Humedad: ");
    Serial.print(h);
    Serial.print(" %  |  Temperatura (DHT11): ");
    Serial.print(t1);
    Serial.println(" °C");
  }

  delay(2000); // Añade un pequeño retraso para evitar lecturas demasiado frecuentes
}

// Función para manejar la API REST que devuelve los datos en formato JSON
void handleApiData()
{
  float h = dht.readHumidity();          // Lee la humedad
  float t1 = dht.readTemperature();      // Lee la temperatura del DHT11
  sensors.requestTemperatures();         // Solicita temperatura del DS18B20
  float t2 = sensors.getTempCByIndex(0); // Lee la temperatura del DS18B20

  // Construye la cadena JSON con los valores leídos
  String json = "{";
  json += "\"humidity\":" + String(h) + ",";
  json += "\"temperature_dht11\":" + String(t1) + ",";
  json += "\"temperature_ds18b20\":" + String(t2) + ",";
  json += "\"flow_rate\":" + String(flowRate);
  json += "}";

  // Configura la cabecera para permitir conexiones desde cualquier origen
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json); // Envía la respuesta en formato JSON
}
