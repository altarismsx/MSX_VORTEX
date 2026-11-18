============================================================
   MSX VORTEX - PROYECTO COMPLETO (ESP32 DevKit v1)
   Chat con Inteligencia Artificial para MSX
   Carlos de la Fuente - Altaris Software 2026
   Version 1.1 - agosto 2026
============================================================

Version economica del Modulo Vortex, sobre placa ESP32
DevKit v1 (ESP-WROOM-32), sin pantalla.

Todos los archivos de codigo vienen SIN credenciales:
cada usuario configura su WiFi y su clave desde el MSX.

------------------------------------------------------------
NOVEDADES DE ESTA VERSION (1.1)
------------------------------------------------------------
- Modelo de IA actualizado a openai/gpt-oss-120b
  (el anterior, Llama 3.3, fue dado de baja por Groq).
- NUEVO comando /modelo: permite cambiar el modelo de IA
  desde el propio MSX, sin reprogramar el modulo.
- El comando /estado ahora muestra tambien el modelo activo.
- Nuevo PDF con la lista de modelos disponibles y su uso.

------------------------------------------------------------
CONTENIDO DEL PAQUETE
------------------------------------------------------------

/firmware/
   MSX_VORTEX_DevKit.ino ...... Codigo fuente del firmware
      del ESP32. Credenciales en blanco (TU_RED_WIFI,
      TU_CLAVE_WIFI, TU_API_KEY_DE_GROQ). Se compila tal
      cual y se configura todo despues desde el MSX.

/basic/
   VORTEX13.BAS ............... Programa de chat para el MSX
   VORTEXR.BAS ............... Version residente (tecla SELECT)

/docs/
   Vortex_DevKit_Manual_ES.pdf .... Manual de usuario (espanol)
   Vortex_DevKit_Manual_PT.pdf .... Manual de usuario (portugues)
   Vortex_DevKit_Manuale_IT.pdf ... Manual de usuario (italiano)
   MSX_VORTEX_DevKit_Manual_ES.pdf  Manual tecnico (espanol)
   MSX_VORTEX_DevKit_Manual_PT.pdf  Manual tecnico (portugues)
   Vortex_Comandos_y_Modelos.pdf .. Comandos y modelos de IA

COMO_OBTENER_CLAVE.txt ...... Guia para la clave (espanol)
HOW_TO_GET_KEY_EN.txt ....... Guia para la clave (ingles)
COME_OTTENERE_CHIAVE_IT.txt . Guia para la clave (italiano)
COMO_OBTER_CHAVE_PT.txt ..... Guia para la clave (portugues)

------------------------------------------------------------
QUE HACE FALTA PARA USARLO
------------------------------------------------------------
1. Una placa ESP32 DevKit v1.
2. Grabar el firmware (carpeta /firmware).
3. Una ficha Centronics y 2 resistencias de 1 kOhm para
   armar el cable al MSX (ver manual tecnico).
4. Cargar el programa de chat en el MSX (carpeta /basic).
5. Una clave de acceso gratis (ver COMO_OBTENER_CLAVE.txt).

------------------------------------------------------------
CONEXIONES (resumen - detalle en el manual tecnico)
------------------------------------------------------------
Ficha Centronics del MSX  ->  ESP32 DevKit v1

   pin 1  (PSTB)  ->  GPIO16 (RX2)  + resistencia 1k
   pin 2  (PDB0)  ->  GPIO18        + resistencia 1k
   pin 11 (BUSY)  ->  GPIO17 (TX2)  directo
   pin 14 (GND)   ->  GND           directo

Las 2 lineas que ENTRAN al ESP (pin 1 y pin 2) llevan
resistencia de 1 kOhm. El ESP32 no tolera 5V.
Alimentar la placa por USB (el puerto de impresora del
MSX no da energia).

------------------------------------------------------------
GRABAR EL FIRMWARE
------------------------------------------------------------
Compilar con Arduino IDE (nucleo ESP32, libreria ArduinoJson)
como placa "ESP32 Dev Module", o grabar el binario compilado:

   python -m esptool --chip esp32 --port COMx
     --baud 921600 write-flash 0x0 firmware.bin

Si queda en bucle de reinicio, primero borrar:
   python -m esptool --chip esp32 --port COMx erase-flash

------------------------------------------------------------
PRIMER USO (resumido)
------------------------------------------------------------
1. Conectar el modulo al puerto de impresora del MSX.
2. Alimentar por USB. Esperar ~15 seg (conecta al WiFi).
3. En el MSX: cargar y ejecutar el programa de chat.
4. Configurar:  /wifi TuRed TuClave   y   /api tu_clave
5. Escribir preguntas y listo.

Sin pantalla, para ver si conecto: escribir /estado.

------------------------------------------------------------
COMANDOS
------------------------------------------------------------
/ayuda  /wifi red clave  /api clave  /modelo nombre
/perso texto  /nueva  /estado  /hora  /ver  dir  /salir

Ver el PDF "Vortex_Comandos_y_Modelos" para el detalle de
cada comando y la lista de modelos de IA disponibles.

------------------------------------------------------------
NOTAS
------------------------------------------------------------
- El historial de conversacion se borra al apagar el modulo.
  La red, la clave, la personalidad y el modelo quedan
  guardados.
- Los comandos mantienen sus nombres en espanol (es lo que
  entiende el firmware), tambien en los manuales en otros
  idiomas.
- Cada usuario usa su propia clave de acceso (gratis).

============================================================
   Desarrollado por Carlos de la Fuente
   Altaris Software 2026
============================================================
