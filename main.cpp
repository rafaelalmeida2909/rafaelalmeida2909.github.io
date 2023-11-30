#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
// Update these with values suitable for your network.

const char *ssid = "brisa-3002925";
const char *password = "4fxc8il7";
const char *mqtt_server = "test.mosquitto.org"; // test.mosquitto.org

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (100)
char msg[MSG_BUFFER_SIZE];

#define DHTPIN D6         // Pino digital D2 (GPIO4) conectado ao DHT11
#define DHTTYPE DHT11     // Tipo do sensor DHT11
DHT dht(DHTPIN, DHTTYPE); // Inicializando o objeto dht do tipo DHT passando como parâmetro o pino (DHTPIN) e o tipo do sensor (DHTTYPE)
float temperature = 0;    // variável para armazenar a temperatura

const int relayPin = D4;
bool relayState = false;
bool isPublishingRelayState = false;
unsigned long relayStartTime = 0;      // Variable to store the start time when the relay is turned on
const unsigned long oneHour = 60000; // 1 hour in milliseconds
unsigned long remainingTime = 0;
unsigned long elapsedTime = 0;

constexpr int button = D5;

LiquidCrystal_I2C lcd(0x27, 16, 2);

float readTemperature()
{
  // Aguarde alguns segundos para estabilizar o sensor
  delay(2000);

  // Leitura da temperatura do sensor DHT
  temperature = dht.readTemperature();

  // Verifique se a leitura é válida
  if (isnan(temperature))
  {
    return 0; // Ou outro valor padrão, indicando que a leitura falhou.
  }

  return temperature;
}

void setup_wifi()
{

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Função para publicar o estado do relé
void publishRelayStateWithDuration(unsigned long remainingTime)
{

  if (relayState)
  {
    snprintf(msg, MSG_BUFFER_SIZE, "Relay is ON. Time remaining: %lu minutes %lu seconds", remainingTime / 60000, (remainingTime / 1000) % 60);
  }
  else
  {
    snprintf(msg, MSG_BUFFER_SIZE, "Relay is OFF.");
  }

  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("device/relay", msg);
}

void turnOnRelay()
{
  if (relayState)
  {
    publishRelayStateWithDuration(remainingTime);
  }
  else
  {
    relayState = true;
    relayStartTime = millis(); // Inicie o temporizador quando o relé é ligado
    digitalWrite(relayPin, relayState);
    elapsedTime = millis() - relayStartTime;
    remainingTime = (elapsedTime > oneHour) ? 0 : (oneHour - elapsedTime);
    publishRelayStateWithDuration(remainingTime);
  }
}

void turnOffRelay()
{
  relayState = false;
  digitalWrite(relayPin, relayState);
  relayStartTime = 0; // Zera o temporizador quando o relé é desligado
  remainingTime = 0;
  publishRelayStateWithDuration(remainingTime);
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (size_t i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on/off the relay based on the received payload
  if (strcmp(topic, "device/relay") == 0)
  {
    if ((char)payload[0] == '1')
    {
      turnOnRelay();
    }
    else if ((char)payload[0] == '0')
    {
      turnOffRelay();
    }
  }
}

void reconnect()
{
  // Loop until we're reconnected

  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("device/temp", "MQTT Server is Connected");
      // ... and resubscribe
      client.subscribe("device/relay");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  lcd.init(); // initialize the lcd
  lcd.backlight();

  dht.begin(); // Inicializa o sensor DHT11
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  pinMode(button, INPUT_PULLUP);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{
  if (!isPublishingRelayState)
  {
    if (!client.connected())
    {
      reconnect();
    }
    client.loop();
  }

  auto readButton = digitalRead(button);
  if (readButton == LOW)
  {
    Serial.println("Button pressed");
    if (relayState)
    {
      turnOffRelay(); // Desligue o relé se estiver ligado
    }
    else
    {
      turnOnRelay(); // Ligue o relé se estiver desligado
    }
    delay(1000);
  }

  digitalWrite(relayPin, relayState);

  float temperature = dht.readTemperature();

  if (relayState)
  {
    elapsedTime = millis() - relayStartTime;
    remainingTime = (elapsedTime > oneHour) ? 0 : (oneHour - elapsedTime);
  }
  lcd.clear();
  lcd.printf("Coffee %s ", relayState ? "ON" : "OFF");
  lcd.printf("%2.f\xDF"
             "C",
             temperature);
  lcd.setCursor(0, 1); // Move o cursor para a segunda linha
  lcd.printf("Time: %lum %lus", remainingTime / 60000, (remainingTime / 1000) % 60);

  if (relayState && elapsedTime >= oneHour)
  {
    // Se o relé está ligado e o tempo decorrido é maior ou igual a 1 hora, desligue o relé
    relayState = false;
    digitalWrite(relayPin, relayState);
    relayStartTime = 0; // Zera o temporizador quando o relé é desligado
    client.publish("device/relay", "Relay is OFF. Timer expired.");
  }

  delay(2000);
  snprintf(msg, MSG_BUFFER_SIZE, "Temperature is %2.f%cC", temperature, 176);
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("device/temp", msg);

  delay(200);
  isPublishingRelayState = false;
}