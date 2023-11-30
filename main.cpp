#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
// Update these with values suitable for your network.

const char* ssid = "FAMILIA2andar";
const char* password = "6303002560";
const char* mqtt_server = "test.mosquitto.org";  // test.mosquitto.org

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(100)
char msg[MSG_BUFFER_SIZE];

#define DHTPIN D2 //Pino digital D2 (GPIO4) conectado ao DHT11
#define DHTTYPE DHT11 //Tipo do sensor DHT11
DHT dht(DHTPIN, DHTTYPE); //Inicializando o objeto dht do tipo DHT passando como parâmetro o pino (DHTPIN) e o tipo do sensor (DHTTYPE)
float temperature = 0; //variável para armazenar a temperatura

const int relayPin = D4;
bool relayState = false;
bool isPublishingRelayState = false;

constexpr int button = D5;

float readTemperature() {
  // Aguarde alguns segundos para estabilizar o sensor
  delay(2000);

  // Leitura da temperatura do sensor DHT
  temperature = dht.readTemperature();
  
  // Verifique se a leitura é válida
  if (isnan(temperature)) {
    return 0; // Ou outro valor padrão, indicando que a leitura falhou.
  }

  return temperature;
}


void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
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
void publishRelayState() {
  snprintf(msg, MSG_BUFFER_SIZE, "Relay is %s", relayState ? "ON" : "OFF");
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("device/relay", msg);
  // Define a variável de controle como true para indicar que o ESP está publicando
  isPublishingRelayState = true;
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (size_t i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the relay if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(relayPin, HIGH);   // Turn the relay on (Note that HIGH is the voltage level
    relayState = true;
    publishRelayState();

  } else if ((char)payload[0] == '0') {
    digitalWrite(relayPin, LOW);  // Turn the relay off by making the voltage HIGH
    relayState = false;
    publishRelayState();
  }

}

void reconnect() {
  // Loop until we're reconnected

  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("device/temp", "MQTT Server is Connected");
      // ... and resubscribe
      client.subscribe("device/relay");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  dht.begin(); //Inicializa o sensor DHT11
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  pinMode(button, INPUT_PULLUP);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!isPublishingRelayState) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  }

  auto readButton = digitalRead(button);
  if (readButton == LOW) {
    Serial.println("Button pressed");
    relayState = !relayState;
    digitalWrite(relayPin, relayState);
    publishRelayState();
    delay(1000);
  }
  digitalWrite(relayPin, relayState);

  float temperature = dht.readTemperature();

  delay(2000);
  snprintf(msg, MSG_BUFFER_SIZE, "Temperature is %2.f %cC", temperature, 176);
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("device/temp", msg);


  delay(200);
  isPublishingRelayState = false;
}