/* 
  PROGRAMA PARA LA VISUALIZACION DE DATOS EN EL DASHBOARD THINGSPEAK
  El siguiente programa utiliza:
                              - Placa de desarrollo ESP8266 NodeMCU v2 CP2102
                              - Sensor Temperatura/Presion BMP280
                              - LED de envio de datos
  Ademas de el envio de datos para la visualizacion online, se utiilza la funcionalidad de TalkBack de ThingSpeak
  para poder recibir un comando de encendido o apagado de el LED BUILT_IN en este caso.

  La simulacion de valores de HUMEDAD se realizaron con la ayuda de la aplicacion 
  NASA/POWER CERES/MERRA2 Native Resolution Daily Data 
  Para el mes de: 06/01/2024 through 07/01/2024 
  RH2M     MERRA-2 Relative Humidity at 2 Meters (%) 

*/

//--------------------------------------- Librerias y Variables necesarias -------------------------------------------//
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <ESP8266WiFi.h>

#define led_envio (00) //---D3
#define led_error (14)  //---D5

//Variables de Sensor
Adafruit_BMP280 bmp; // Utilizando la comunicacion I2C
volatile float presionATM=0;
volatile float temperatura=0;

//Contraseñas y Datos confidenciales*****************
#include "paswords.h"

//Simulacion de la humedad
#include "historico_humedad.h" // From NASA POWER Project
volatile float humedad=0;
int humIndex = 0 ; //posicion en el arreglo 
int tamano_arreglo = sizeof( HUMEDAD_HISTORICA )/ sizeof(HUMEDAD_HISTORICA[0]); //Tamaño del arreglo

//Conexiones ThingSpeak
#include "ThingSpeak.h"
unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

unsigned long myTalkBackID = SECRET_TB_ID; 
const char * myTalkBackKey = SECRET_TB_KEY;

//Wifi Data
char ssid[] = SECRET_SSID;   // your network SSID (name)
char pass[] = SECRET_PASS;   // your network password
int keyIndex = 0;            // your network key index number (needed only for WEP)
WiFiClient  client;


//--------------------------------------- Setup -------------------------------------------//

void setup() {
 // Inicializamos comunicación serie
  Serial.begin(9600);

  //Led envio al canal
  pinMode(led_envio, OUTPUT);

  //Led de errores
  pinMode(led_error, OUTPUT);

  //Led Builtin
  pinMode(LED_BUILTIN, OUTPUT);  

 // Comenzamos el sensor BMP280
  unsigned status;
  status = bmp.begin(0x76);
  if (!status) {
  Serial.println(F("Error en BMP try a different address!"));
  }
                     
  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

    //Wifi Connection
    WiFi.mode(WIFI_STA);

    //ThingSpeak
    ThingSpeak.begin(client);

  conexionWIFI();
}

//--------------------------------------- Loop -------------------------------------------//
void loop() {

    conexionWIFI(); // Conexion al Wifi

    lecturaSensor(); //Lectura del sensor BMP280

    simulacionHumedad(); //Leo historico de humedad

    envioNubeTS(); //Envio al Canal de ThingSpeak

    recivoNubeTS(); //Si hay comandos los ejecuto
}

//------------------------------------------- Leer sensor BMP280 -----------------------------------------------//
void lecturaSensor(void){
  // Leemos la presion desde el BMP280
  presionATM = bmp.readPressure()/ 100;
  // Leemos la temperatura en grados centígrados 
  temperatura = bmp.readTemperature();

    //Muestro en Serial Monitor
    Serial.print(F("Temperature = "));
    Serial.print(temperatura);
    Serial.println(" ºC");

    Serial.print(F("Pressure = "));
    Serial.print(presionATM);
    Serial.println(" hPa");

  // Comprobamos si ha habido algún error en la lectura
  if (isnan(presionATM)|| isnan(temperatura)) {
    Serial.println("Error");
    
      digitalWrite(led_error, HIGH);  // Enciendo LED de error
      delay(10000);
      digitalWrite(led_error, LOW);
    return ;
  }

}

//------------------------------------------- Simulo Humedad con datos Historicos -----------------------------------------------//
void simulacionHumedad(void){
  if(humIndex < tamano_arreglo ){
    //Muestro un dato del arreglo por cada envio de datos a la nube.
    humedad = HUMEDAD_HISTORICA[humIndex]; 

    Serial.print(F("Humidity = "));
    Serial.print(humedad); 
    Serial.println("%");

    humIndex++ ; //Muestro siguiente dato en la cola
  }if (humIndex == tamano_arreglo){
    humIndex= 0; //Comienza desde el primer valor cuando termina de mostrar todos.
  }
}


//------------------------------------------- WiFi  -----------------------------------------------//
void conexionWIFI (void){
    // Conectar o Reconectar al WiFi
  if (WiFi.status() != WL_CONNECTED) {
      Serial.print("Attempting to connect to SSID: ");
      Serial.println(SECRET_SSID);
      while (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
        Serial.print(".");
        delay(5000);
      }
    Serial.println("\nConnected.");
  }
}
//------------------------------------------- Envio ThingSpeak -----------------------------------------------//
void envioNubeTS (void){
    
    //Creo los graficos en ThingSpeakk, en este caso son 3:
    ThingSpeak.setField (1,temperatura);
    ThingSpeak.setField (2,presionATM);
    ThingSpeak.setField (3,humedad);
  int httpCode = ThingSpeak.writeFields(myChannelNumber,myWriteAPIKey); //Envio de datos

  // Para enviar un solo dato al ThingSpeak Channel
  //int httpCode = ThingSpeak.writeField(myChannelNumber, 1, humedad , myWriteAPIKey);

  if (httpCode == 200) {
    Serial.println("Channel write successful.");
  }
  else {
    Serial.println("Problem writing to channel. HTTP error code " + String(httpCode));
  }

  // Wait 10 seconds to update the channel again
  digitalWrite(led_envio, HIGH);  // Enviendo LED de envio de datos (HIGH is the voltage level)
  delay(10000);
  digitalWrite(led_envio, LOW);
}
//------------------------------------------- Escucha a ThingSpeakk -----------------------------------------------//

//Recibo comandos desde ThingSpeak TalkBack
void recivoNubeTS(void){

  // Creo el  TalkBack URI o URL
  String tbURI = String("/talkbacks/") + String(myTalkBackID) + String("/commands/execute");
  
  // Crea el cuerpo del mensaje para el POST a partir de los valores
  String postMessage =  String("api_key=") + String(myTalkBackKey);   
  
  //Crea una cadena para cualquier comando en la cola
  String newCommand = String();

  //Comandos que analizo en este caso
  const char * led_on = "ON";
  const char * led_off = "OFF" ;
  const char *nothing = "";

  // Hace la POST en ThingSpeak
  int x = httpPOST(tbURI, postMessage, newCommand);
  client.stop();

  //Guardo el comando en mensaje para poder compararlo
  char message_recive[50];
  sprintf(message_recive, "%s", newCommand.c_str());
  //Serial.println(message_recive) ;

    int len;
    char comando[30];
    int endMessage;
      sscanf(message_recive, "%i\n%s\n%i", &len, comando, &endMessage);//Divido el comando
      sprintf(comando, "%s", comando); // Agrega el \0 al final para que sea String
 
  // Checkeo el resultado
  if(x == 200){ //OK
    Serial.println("checking queue..."); 
    // Analizo el comando devuelto por TalkBack
    if(len != 0){
      
      Serial.println("Latest command from queue: ");
      Serial.print(comando) ;

      if(strcmp(led_on,comando) == 0){ //comando = "ON"
        digitalWrite(LED_BUILTIN, LOW);  //Polaridad Inversa (Anodo Comun)
      }

      if(strcmp(led_off,comando) == 0){ //comando = "OFF"
        digitalWrite(LED_BUILTIN, HIGH); // Polaridad Inversa
      }

    }
    else{
      Serial.println("Nothing new.");  
    }
    
  }
  else{
    Serial.println("Problem checking queue. HTTP error code " + String(x));
  }

  delay(5000); // Espera 5 segundos para ver si hay otro comando
}

//------------------------------------------- Funcion POST para ver proximo comando en cola ThingSpeak -----------------------------------------------//
int httpPOST(String uri, String postMessage, String &response){

  bool connectSuccess = false;
  connectSuccess = client.connect("api.thingspeak.com",80);

  if(!connectSuccess){
      return -301;   // Moved Permanently
  }
  
  postMessage += "&headers=false";
  
  String Headers =  String("POST ") + uri + String(" HTTP/1.1\r\n") +
                    String("Host: api.thingspeak.com\r\n") +
                    String("Content-Type: application/x-www-form-urlencoded\r\n") +
                    String("Connection: close\r\n") +
                    String("Content-Length: ") + String(postMessage.length()) +
                    String("\r\n\r\n");

  client.print(Headers);
  client.print(postMessage);

  long startWaitForResponseAt = millis();
  while(client.available() == 0 && millis() - startWaitForResponseAt < 5000){
      delay(100);
  }

  if(client.available() == 0){       
    return -304; // Didn't get server response in time
  }

  if(!client.find(const_cast<char *>("HTTP/1.1"))){
      return -303; // Couldn't parse response (didn't find HTTP/1.1)
  }
  
  int status = client.parseInt();
  if(status != 200){ //Si no hay conexion exitosa
    return status;
  }

  if(!client.find(const_cast<char *>("\n\r\n"))){
    return -303;
  }

  String tempString = String(client.readString());
  response = tempString;
  
  return status;

}

