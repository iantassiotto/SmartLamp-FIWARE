/* ============================================================================
 * CP4 - Smart Lamp | FIWARE Descomplicado
 * ----------------------------------------------------------------------------
 * Liga e desliga o LED do ESP32 via comandos MQTT enviados pelo FIWARE
 * (Orion -> IoT Agent -> Mosquitto) e publica o estado do LED e a leitura
 * de luminosidade do LDR nos tópicos do broker.
 *
 * Entidade FIWARE: urn:ngsi-ld:Lamp:200   |   device_id: lamp200
 * Turma: 1ESPG - FIAP
 *
 * Autor original: Fábio Henrique Cabrini
 * Rev1: 26-08-2023 - Port para ESP32 e leitura de luminosidade - Lucas Demetrius Augusto
 * Rev2: 28-08-2023 - Ajustes para o FIWARE Descomplicado - Fábio Henrique Cabrini
 * Rev3: 01-11-2023 - Refinamento do código - Fábio Henrique Cabrini
 * Rev4: 13-09-2026 - Adaptação para a entidade lamp200, confirmação de
 *                    execução de comando (cmdexe) e publicação não bloqueante
 *                    Editor: João Gabriel - RM573685
 *
 * Integrantes:
 *   IAN TASSIOTTO    - RM570812
 *   HYAN HIDEO       - RM571005
 *   JOÃO GABRIEL     - RM573685
 *   ELIZEU QUEIROZ   - RM569433
 *   GUILHERME CAIANO - RM570491
 * ==========================================================================*/

#include <WiFi.h>
#include <PubSubClient.h>

// Wokwi: "Wokwi-GUEST" / senha vazia.  FIAP: "FIAP-IOT" / "F!@p25.IOT"
const char* default_SSID     = "Wokwi-GUEST";
const char* default_PASSWORD = "";

// IP público da instância EC2
const char* default_BROKER_MQTT = "XX.XXX.XXX.XX"; // Substituir pelo IP público da sua VM
const int   default_BROKER_PORT = 1883;

const char* default_TOPICO_SUBSCRIBE = "/TEF/lamp001/cmd";
const char* default_TOPICO_PUBLISH_1 = "/TEF/lamp001/attrs";
const char* default_TOPICO_PUBLISH_2 = "/TEF/lamp001/attrs/l";
const char* default_TOPICO_CMDEXE    = "/TEF/lamp001/cmdexe";

const char* default_ID_MQTT = "fiware_001";
const char* topicPrefix     = "lamp001";

const int default_D4 = 2;
const int LDR_PIN    = 34;

const unsigned long INTERVALO_PUBLICACAO = 2000;

char* SSID             = const_cast<char*>(default_SSID);
char* PASSWORD         = const_cast<char*>(default_PASSWORD);
char* BROKER_MQTT      = const_cast<char*>(default_BROKER_MQTT);
int   BROKER_PORT      = default_BROKER_PORT;
char* TOPICO_SUBSCRIBE = const_cast<char*>(default_TOPICO_SUBSCRIBE);
char* TOPICO_PUBLISH_1 = const_cast<char*>(default_TOPICO_PUBLISH_1);
char* TOPICO_PUBLISH_2 = const_cast<char*>(default_TOPICO_PUBLISH_2);
char* TOPICO_CMDEXE    = const_cast<char*>(default_TOPICO_CMDEXE);
char* ID_MQTT          = const_cast<char*>(default_ID_MQTT);
int   D4               = default_D4;

WiFiClient espClient;
PubSubClient MQTT(espClient);

char EstadoSaida = '0';
unsigned long ultimaPublicacao = 0;

void mqtt_callback(char* topic, byte* payload, unsigned int length);
void reconectWiFi();
void reconnectMQTT();
void VerificaConexoesWiFIEMQTT();
void EnviaEstadoOutputMQTT();
void handleLuminosity();
void InitOutput();
void initSerial();
void initWiFi();
void initMQTT();

void setup() {
    initSerial();
    InitOutput();
    initWiFi();
    initMQTT();
    delay(2000);
    MQTT.publish(TOPICO_PUBLISH_1, "s|off");
}

void initSerial() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("=== CP4 Smart Lamp - lamp001 ===");
}

void initWiFi() {
    delay(10);
    Serial.println("------ Conexao WI-FI ------");
    Serial.print("Conectando-se na rede: ");
    Serial.println(SSID);
    reconectWiFi();
}

void initMQTT() {
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);
    MQTT.setCallback(mqtt_callback);
    Serial.print("Broker MQTT: ");
    Serial.print(BROKER_MQTT);
    Serial.print(":");
    Serial.println(BROKER_PORT);
}

void loop() {
    VerificaConexoesWiFIEMQTT();
    MQTT.loop();

    // Publicação periódica sem bloquear o loop, para não atrasar os comandos
    if (millis() - ultimaPublicacao >= INTERVALO_PUBLICACAO) {
        ultimaPublicacao = millis();
        EnviaEstadoOutputMQTT();
        handleLuminosity();
    }
}

void reconectWiFi() {
    if (WiFi.status() == WL_CONNECTED)
        return;

    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Conectado na rede: ");
    Serial.println(SSID);
    Serial.print("IP obtido: ");
    Serial.println(WiFi.localIP());

    digitalWrite(D4, LOW);
}

void VerificaConexoesWiFIEMQTT() {
    if (!MQTT.connected())
        reconnectMQTT();
    reconectWiFi();
}

void reconnectMQTT() {
    while (!MQTT.connected()) {
        Serial.print("* Conectando ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) {
            Serial.println("Conectado ao broker MQTT!");
            MQTT.subscribe(TOPICO_SUBSCRIBE);
            Serial.print("Inscrito no topico: ");
            Serial.println(TOPICO_SUBSCRIBE);
        } else {
            Serial.println("Falha. Nova tentativa em 2s...");
            delay(2000);
        }
    }
}

// O IoT Agent publica "lamp200@on|" em /TEF/lamp200/cmd.
// A resposta em /TEF/lamp200/cmdexe evita que o comando fique PENDING no Orion.
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    Serial.print("- Mensagem recebida: ");
    Serial.println(msg);

    String onTopic  = String(topicPrefix) + "@on|";
    String offTopic = String(topicPrefix) + "@off|";

    if (msg.equals(onTopic)) {
        digitalWrite(D4, HIGH);
        EstadoSaida = '1';
        MQTT.publish(TOPICO_CMDEXE, "lamp200@on|ok");
        MQTT.publish(TOPICO_PUBLISH_1, "s|on");
        Serial.println("- Comando ON executado");
    }

    if (msg.equals(offTopic)) {
        digitalWrite(D4, LOW);
        EstadoSaida = '0';
        MQTT.publish(TOPICO_CMDEXE, "lamp200@off|ok");
        MQTT.publish(TOPICO_PUBLISH_1, "s|off");
        Serial.println("- Comando OFF executado");
    }
}

void EnviaEstadoOutputMQTT() {
    if (EstadoSaida == '1') {
        MQTT.publish(TOPICO_PUBLISH_1, "s|on");
        Serial.println("- Led Ligado");
    } else {
        MQTT.publish(TOPICO_PUBLISH_1, "s|off");
        Serial.println("- Led Desligado");
    }
}

void handleLuminosity() {
    int sensorValue = analogRead(LDR_PIN);
    int luminosity  = map(sensorValue, 0, 4095, 100, 0);

    Serial.print("Valor da luminosidade: ");
    Serial.println(luminosity);

    MQTT.publish(TOPICO_PUBLISH_2, String(luminosity).c_str());
}

void InitOutput() {
    pinMode(D4, OUTPUT);
    pinMode(LDR_PIN, INPUT);

    // Pisca 5 vezes para sinalizar o boot
    boolean toggle = false;
    for (int i = 0; i <= 10; i++) {
        toggle = !toggle;
        digitalWrite(D4, toggle);
        delay(200);
    }
    digitalWrite(D4, LOW);
}
