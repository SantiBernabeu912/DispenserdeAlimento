#include <WiFi.h>        // Librería para manejar WiFi en el ESP32
#include <WebServer.h>   // Librería para crear un servidor web sencillo


// 👉 Datos de tu red WiFi (cámbialos por los tuyos)
const char* ssid = "Telecentro-b114";       // Nombre de tu red WiFi (SSID)
const char* password = "MKKKNLAVTG4M";    // Contraseña de tu WiFi


// 👉 Creamos el servidor en el puerto 80 (HTTP usa el puerto 80 por defecto)
WebServer server(80);


// 👉 Esta función define qué se muestra cuando entras a la página principal "/"
void handleRoot() {
  server.send(200, "text/html", "<h1>luquez infiel</h1>");
}


void setup() {
  Serial.begin(115200);  // Inicia comunicación serie para depuración
  delay(1000);


  // 👉 Conexión al WiFi
  WiFi.begin(ssid, password);        
  Serial.print("Conectando a WiFi");


  // Espera hasta que se conecte
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }


  Serial.println("\n✅ Conectado a WiFi!");
  Serial.print("📡 Dirección IP del ESP32: ");
  Serial.println(WiFi.localIP());   // 👈 Acá ves la IP que debes poner en el navegador


  // 👉 Configuramos qué pasa cuando entramos a "/"
  server.on("/", handleRoot);


  // 👉 Iniciamos el servidor web
  server.begin();
  Serial.println("🌐 Servidor web iniciado");
}


void loop() {
  // 👉 Atendemos las peticiones de los clientes
  server.handleClient();
}



