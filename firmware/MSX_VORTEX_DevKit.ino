// ============================================================
// MSX VORTEX v1.1 - Firmware ESP32 DevKit v1 (sin pantalla)
// Chat MSX <-> Groq con memoria, NVS, NTP
// UART 57600 8N1: RX=GPIO16, TX=GPIO17, RTS=GPIO18
// Carlos de la Fuente - Altaris Software 2026
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

#define RX_PIN     16
#define RTS_PIN    18
#define TX_PIN     17
int chatCount = 0;

// Defaults (se usan solo la primera vez, despues NVS manda)
// Cada usuario carga los suyos desde el MSX con /wifi y /api
const char* DEF_WIFI_SSID = "TU_RED_WIFI";
const char* DEF_WIFI_PASS = "TU_CLAVE_WIFI";
const char* DEF_API_KEY   = "TU_API_KEY_DE_GROQ";
const char* DEF_PERSONA   = "Sos MSX Vortex, asistente IA en una computadora MSX de los 80s. Responde SIEMPRE en espanol SIN acentos ni caracteres especiales. Solo ASCII 32-125. Se conciso pero completo. Sin markdown. Sin emojis.";

const char* API_HOST  = "api.groq.com";
const char* API_PATH  = "/openai/v1/chat/completions";
const char* DEF_MODELO = "openai/gpt-oss-120b";
const int MAX_PREGUNTA = 200;
const int MAX_RESPUESTA = 500;
const int MAX_HISTORIAL = 6;  // pares pregunta/respuesta en contexto

HardwareSerial msxSerial(1);
Preferences prefs;

// Config en RAM (cargada de NVS)
String cfgSsid, cfgPass, cfgApi, cfgPersona, cfgModelo;
String histP[MAX_HISTORIAL];  // preguntas
String histR[MAX_HISTORIAL];  // respuestas
int histN = 0;

void ledSet(uint8_t r, uint8_t g, uint8_t b) {
  // DevKit V1 sin LED RGB - funcion vacia
}

// ============ NVS: cargar y guardar configuracion ============


void cargarConfig() {
  prefs.begin("vortex", false);
  cfgSsid    = prefs.getString("ssid", DEF_WIFI_SSID);
  cfgPass    = prefs.getString("pass", DEF_WIFI_PASS);
  cfgApi     = prefs.getString("api", DEF_API_KEY);
  cfgPersona = prefs.getString("persona", DEF_PERSONA);
  cfgModelo  = prefs.getString("modelo", DEF_MODELO);
  histN = 0;   // el historial vive solo en RAM (no persiste entre apagados)
  prefs.end();
}

void guardarCredWifi(const String& s, const String& p) {
  prefs.begin("vortex", false);
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  prefs.end();
  cfgSsid = s; cfgPass = p;
}

void guardarApi(const String& a) {
  prefs.begin("vortex", false);
  prefs.putString("api", a);
  prefs.end();
  cfgApi = a;
}

void guardarModelo(const String& m) {
  prefs.begin("vortex", false);
  prefs.putString("modelo", m);
  prefs.end();
  cfgModelo = m;
}

void guardarPersona(const String& p) {
  prefs.begin("vortex", false);
  prefs.putString("persona", p);
  prefs.end();
  cfgPersona = p;
}

void guardarHistorial() {
  // Historial volatil: solo en RAM. No se escribe en flash.
}

void borrarHistorial() {
  histN = 0;
}

void agregarHistorial(const String& p, const String& r) {
  if (histN >= MAX_HISTORIAL) {
    // Correr todo una posicion (descartar el mas viejo)
    for (int i = 0; i < MAX_HISTORIAL - 1; i++) {
      histP[i] = histP[i+1];
      histR[i] = histR[i+1];
    }
    histN = MAX_HISTORIAL - 1;
  }
  histP[histN] = p;
  histR[histN] = r;
  histN++;
  guardarHistorial();
}

// ============ Normalizacion ASCII ============

String normalizarAscii(const String& s) {
  String out = "";
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == 0xC3 && i + 1 < s.length()) {
      unsigned char c2 = (unsigned char)s[i+1];
      switch (c2) {
        case 0xA1: case 0xA0: case 0xA2: case 0xA3: case 0xA4: out += 'a'; break;
        case 0x81: case 0x80: case 0x82: case 0x83: case 0x84: out += 'A'; break;
        case 0xA9: case 0xA8: case 0xAA: case 0xAB: out += 'e'; break;
        case 0x89: case 0x88: case 0x8A: case 0x8B: out += 'E'; break;
        case 0xAD: case 0xAC: case 0xAE: case 0xAF: out += 'i'; break;
        case 0x8D: case 0x8C: case 0x8E: case 0x8F: out += 'I'; break;
        case 0xB3: case 0xB2: case 0xB4: case 0xB5: case 0xB6: out += 'o'; break;
        case 0x93: case 0x92: case 0x94: case 0x95: case 0x96: out += 'O'; break;
        case 0xBA: case 0xB9: case 0xBB: case 0xBC: out += 'u'; break;
        case 0x9A: case 0x99: case 0x9B: case 0x9C: out += 'U'; break;
        case 0xB1: out += 'n'; break;
        case 0x91: out += 'N'; break;
      }
      i++; continue;
    }
    if (c == 0xC2 && i + 1 < s.length()) {
      unsigned char c2 = (unsigned char)s[i+1];
      if (c2 == 0xBF) out += '?';
      else if (c2 == 0xA1) out += '!';
      i++; continue;
    }
    if (c == 126) { out += '-'; }
    else if (c >= 32 && c < 126) out += (char)c;
    else if (c == '\r' || c == '\n') out += ' ';
  }
  return out;
}

// ============ Fecha y hora local (NTP) ============

String fechaHora() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 3000)) return "sin hora (NTP no sincronizo)";
  char buf[64];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &timeinfo);
  return String(buf);
}

// ============ Consulta a Groq con historial y fecha ============

String consultarIA(const String& pregunta) {
  WiFiClientSecure cli;
  cli.setInsecure();
  cli.setTimeout(25);
  if (!cli.connect(API_HOST, 443)) return "[no conecta con Groq]";

  JsonDocument req;
  req["model"] = cfgModelo;
  req["max_tokens"] = MAX_RESPUESTA;
  JsonArray msgs = req["messages"].to<JsonArray>();

  // System: personalidad + fecha/hora actual
  JsonObject sm = msgs.add<JsonObject>();
  sm["role"] = "system";
  String sys = cfgPersona + " Fecha y hora actual: " + fechaHora() + " (Argentina).";
  sm["content"] = sys;

  // Historial de conversacion (memoria)
  for (int i = 0; i < histN; i++) {
    JsonObject up = msgs.add<JsonObject>();
    up["role"] = "user";
    up["content"] = histP[i];
    JsonObject ar = msgs.add<JsonObject>();
    ar["role"] = "assistant";
    ar["content"] = histR[i];
  }

  // Pregunta actual
  JsonObject um = msgs.add<JsonObject>();
  um["role"] = "user";
  um["content"] = pregunta;

  String body;
  serializeJson(req, body);

  cli.print("POST "); cli.print(API_PATH); cli.println(" HTTP/1.1");
  cli.print("Host: "); cli.println(API_HOST);
  cli.println("Content-Type: application/json");
  cli.print("Authorization: Bearer "); cli.println(cfgApi);
  cli.print("Content-Length: "); cli.println(body.length());
  cli.println("Connection: close");
  cli.println();
  cli.print(body);

  String rawResp = "";
  unsigned long t0 = millis();
  while ((cli.connected() || cli.available()) && millis() - t0 < 25000) {
    while (cli.available()) { rawResp += (char)cli.read(); t0 = millis(); }
    delay(10);
  }
  cli.stop();

  int bodyStart = rawResp.indexOf("\r\n\r\n");
  String resp = (bodyStart >= 0) ? rawResp.substring(bodyStart + 4) : rawResp;

  bool esChunked = false;
  int primerCRLF = resp.indexOf("\r\n");
  if (primerCRLF > 0 && primerCRLF < 8) {
    esChunked = true;
    for (int i = 0; i < primerCRLF; i++) {
      char c = resp[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
        esChunked = false; break;
      }
    }
  }
  if (esChunked) {
    String dechunked = "";
    int idx = 0;
    while (idx < (int)resp.length()) {
      int endSize = resp.indexOf("\r\n", idx);
      if (endSize < 0) break;
      String sizeStr = resp.substring(idx, endSize);
      int chunkSize = (int)strtol(sizeStr.c_str(), NULL, 16);
      if (chunkSize == 0) break;
      int dataStart = endSize + 2;
      if (dataStart + chunkSize > (int)resp.length()) {
        dechunked += resp.substring(dataStart); break;
      }
      dechunked += resp.substring(dataStart, dataStart + chunkSize);
      idx = dataStart + chunkSize + 2;
    }
    resp = dechunked;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err) return String("[error: revisa /estado o tu API key]");
  if (doc["error"]) {
    return String("[error API: ") + doc["error"]["message"].as<String>().substring(0, 100) + "]";
  }
  return doc["choices"][0]["message"]["content"].as<String>();
}

// ============ Comandos especiales ============
// Devuelve la respuesta al comando, o "" si no era comando

String procesarComando(const String& msg) {
  String m = msg;
  m.trim();
  String ml = m;
  ml.toLowerCase();

  if (ml.startsWith("/wifi ")) {
    // formato: /wifi ssid password
    String resto = m.substring(6);
    resto.trim();
    int sp = resto.indexOf(' ');
    if (sp < 0) return "uso: /wifi nombre password";
    String s = resto.substring(0, sp);
    String p = resto.substring(sp + 1);
    p.trim();
    guardarCredWifi(s, p);
    WiFi.disconnect();
    delay(500);
    WiFi.begin(cfgSsid.c_str(), cfgPass.c_str());
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(300);
    if (WiFi.status() == WL_CONNECTED)
      return "wifi guardado y conectado a " + s + " ip " + WiFi.localIP().toString();
    return "wifi guardado pero no conecto. revisa los datos con /wifi";
  }

  if (ml.startsWith("/api ")) {
    String a = m.substring(5);
    a.trim();
    if (a.length() < 10) return "uso: /api tu_api_key_de_groq";
    guardarApi(a);
    return "api key guardada correctamente";
  }

  if (ml.startsWith("/modelo ")) {
    String md = m.substring(8);
    md.trim();
    if (md.length() < 3) return "uso: /modelo nombre_del_modelo";
    guardarModelo(md);
    return "modelo guardado: " + md;
  }

  if (ml == "/modelo") {
    return "modelo actual: " + cfgModelo;
  }

  if (ml.startsWith("/perso ")) {
    String p = m.substring(7);
    p.trim();
    if (p.length() < 5) return "uso: /perso descripcion de la personalidad";
    String base = p + " Responde SIEMPRE en espanol SIN acentos. Solo ASCII 32-125. Sin markdown.";
    guardarPersona(base);
    borrarHistorial();
    return "personalidad guardada. historial reiniciado.";
  }

  if (ml == "/nueva") {
    borrarHistorial();
    return "conversacion nueva. historial borrado.";
  }

  if (ml == "/estado") {
    String r = "vortex v1.0 - ";
    if (WiFi.status() == WL_CONNECTED) {
      r += "wifi " + cfgSsid + " ok, ip " + WiFi.localIP().toString();
    } else {
      r += "wifi desconectado";
    }
    r += ", historial " + String(histN) + "/" + String(MAX_HISTORIAL);
    r += ", modelo " + cfgModelo;
    r += ", hora " + fechaHora();
    return r;
  }

  if (ml == "/hora") {
    return fechaHora() + " (Argentina)";
  }

  if (ml == "/ayuda" || ml == "/help") {
    return "comandos: /wifi ssid pass - /api key - /modelo nombre - /perso texto - /nueva - /estado - /hora - /ayuda";
  }

  return "";  // no era comando
}

// ============ Setup y loop principal ============

bool primerMensaje = true;

void setup() {
  Serial.begin(115200);

  delay(1500);
  Serial.println();
  Serial.println("=== MSX VORTEX DevKit (sin pantalla) ===");

  cargarConfig();
  Serial.println("Config NVS cargada");


  msxSerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
  pinMode(RTS_PIN, INPUT_PULLUP);

  ledSet(255, 100, 0);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfgSsid.c_str(), cfgPass.c_str());

  unsigned long t0 = millis();
  bool blink = false;
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) {
    delay(300);
    blink = !blink;
    ledSet(blink ? 255 : 0, blink ? 100 : 0, 0);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
    // NTP: hora argentina (UTC-3)
    configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");
    ledSet(0, 0, 30);
  } else {
    Serial.println("WiFi no conecto (usa /wifi desde el MSX)");
    ledSet(30, 0, 0);
  }
}

void loop() {
  ledSet(WiFi.status() == WL_CONNECTED ? 0 : 30, 0, WiFi.status() == WL_CONNECTED ? 30 : 0);


  // Recibir pregunta hasta 0x7E
  String pregunta = "";
  bool primerByte = true;
  unsigned long lastByte = millis();

  while (true) {
    if (msxSerial.available()) {
      uint8_t b = msxSerial.read();
      if (b == 0x0E) {
        pregunta = "";
        primerByte = true;
        while (msxSerial.available()) msxSerial.read();
        ledSet(0, 0, 30);
        continue;
      }
      if (primerByte) { ledSet(30, 0, 0); primerByte = false; }
      lastByte = millis();
      if (b == 0x7E) break;
      if (pregunta.length() < MAX_PREGUNTA) pregunta += (char)b;
    }
    if (!primerByte && millis() - lastByte > 60000) break;
  }

  pregunta.trim();
  Serial.print("Pregunta: '"); Serial.print(pregunta); Serial.println("'");

  String respuesta;

  if (primerMensaje && pregunta.length() == 0) {
    respuesta = "hola, soy vortex. escribi tu pregunta o /ayuda para ver comandos.";
    primerMensaje = false;
  } else if (pregunta.length() == 0) {
    respuesta = "escribi algo antes de enter.";
  } else {
    // Ver si es comando especial
    String cmdResp = procesarComando(pregunta);
    if (cmdResp.length() > 0) {
      respuesta = cmdResp;
    } else {
      // Consulta normal a la IA
      if (WiFi.status() != WL_CONNECTED) {
        respuesta = "sin wifi. usa /wifi nombre password para configurar.";
      } else {
        ledSet(30, 0, 30);

        respuesta = consultarIA(pregunta);
        chatCount++;
        ledSet(30, 15, 0);
        respuesta = normalizarAscii(respuesta);
        // Guardar en historial solo consultas reales exitosas
        if (!respuesta.startsWith("[")) {
          agregarHistorial(pregunta, respuesta);
        }
      }
    }
  }

  Serial.print("Respuesta: '"); Serial.print(respuesta.substring(0, 80)); Serial.println("...'");

  ledSet(0, 30, 0);

  // Enviar respetando RTS con pacing
  for (size_t i = 0; i < respuesta.length(); i++) {
    unsigned long t0 = millis();
    while (digitalRead(RTS_PIN) == HIGH) {
      if (millis() - t0 > 10000) break;
    }
    msxSerial.write((uint8_t)respuesta[i]);
    msxSerial.flush();
    delayMicroseconds(2000);
  }
  delay(250);
  unsigned long t1 = millis();
  while (digitalRead(RTS_PIN) == HIGH) {
    if (millis() - t1 > 10000) break;
  }
  msxSerial.write(0x7E);
  msxSerial.flush();
  delay(50);
  msxSerial.write(0x7E);
  msxSerial.flush();
}
