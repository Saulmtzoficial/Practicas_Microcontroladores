"""
============================================================
  PicoChat - Sistema de mensajeria local
  Practica 17 - Mecatronica, UASLP-FEPZM
  Plataforma: Raspberry Pi Pico W con MicroPython
============================================================

Funcionamiento general
----------------------
1. La Pico W levanta un Access Point WiFi (red local propia).
2. Inicia un servidor HTTP sobre sockets en el puerto 80.
3. Sirve una pagina web (index.html) a cualquier celular que
   se conecte a la red y abra http://192.168.4.1
4. Recibe mensajes por POST /enviar y los entrega por
   GET /mensajes?desde=<id>. El cliente hace polling cada 1.5 s
   para simular comunicacion en tiempo real.

Endpoints
---------
GET  /              -> pagina web (HTML+CSS+JS embebido)
GET  /mensajes      -> JSON con mensajes nuevos desde el id dado
POST /enviar        -> recibe JSON {usuario, texto} y lo guarda
"""

import network
import socket
import ujson as json
import time
import gc
from machine import Pin

# ============================================================
# CONFIGURACION
# ============================================================

WIFI_SSID    = "PicoChat"               # nombre de la red
WIFI_PASS    = "mecatronica2026"        # minimo 8 caracteres (WPA2)
PUERTO       = 80                       # puerto HTTP estandar
MAX_MENSAJES = 50                       # historial maximo en RAM

# LED on-board para realimentacion visual
led = Pin("LED", Pin.OUT)

# ============================================================
# ALMACENAMIENTO DE MENSAJES (en RAM)
# ============================================================
# Cada mensaje es un dict: {id, usuario, texto}
# El cliente agrega su propia marca de tiempo al recibirlo,
# porque la Pico W no tiene reloj de tiempo real (RTC) sin NTP.

mensajes     = []
contador_id  = 0


def agregar_mensaje(usuario, texto):
    """Anade un mensaje al historial, recortando si excede el limite."""
    global contador_id
    contador_id += 1

    msg = {
        "id":      contador_id,
        "usuario": usuario[:20],   # limitar para evitar abuso de memoria
        "texto":   texto[:200],
    }
    mensajes.append(msg)

    # Mantener solo los ultimos MAX_MENSAJES en memoria
    if len(mensajes) > MAX_MENSAJES:
        mensajes.pop(0)

    print("[MSG #{}] {}: {}".format(msg["id"], msg["usuario"], msg["texto"]))
    return msg


# ============================================================
# CONFIGURACION DE WIFI EN MODO ACCESS POINT
# ============================================================

def iniciar_ap():
    """Activa el modo Access Point y espera a que este listo."""
    ap = network.WLAN(network.AP_IF)
    ap.config(essid=WIFI_SSID, password=WIFI_PASS)
    ap.active(True)

    # Esperar a que el AP este realmente activo
    while not ap.active():
        time.sleep(0.1)

    ip = ap.ifconfig()[0]
    print("============================================")
    print(" Access Point activo")
    print(" SSID:     ", WIFI_SSID)
    print(" Password: ", WIFI_PASS)
    print(" IP:       ", ip)
    print(" URL:      http://{}".format(ip))
    print("============================================")
    return ap


# ============================================================
# PARSER HTTP MINIMO
# ============================================================

def parsear_peticion(datos):
    """
    Extrae metodo, ruta, query string y cuerpo de una peticion HTTP.
    Devuelve (metodo, ruta, query, cuerpo) o (None,)*4 si hay error.
    """
    try:
        texto  = datos.decode("utf-8")
        lineas = texto.split("\r\n")

        # Linea de peticion: "GET /ruta?query HTTP/1.1"
        partes = lineas[0].split(" ")
        if len(partes) < 2:
            return None, None, None, None
        metodo = partes[0]
        url    = partes[1]

        # Separar cuerpo (todo lo que va despues de la linea en blanco)
        if "\r\n\r\n" in texto:
            _, cuerpo = texto.split("\r\n\r\n", 1)
        else:
            cuerpo = ""

        # Separar ruta y query string
        if "?" in url:
            ruta, query = url.split("?", 1)
        else:
            ruta, query = url, ""

        return metodo, ruta, query, cuerpo

    except Exception as e:
        print("Error parseando peticion:", e)
        return None, None, None, None


def parsear_query(query):
    """Convierte 'a=1&b=hola' en {'a':'1', 'b':'hola'}."""
    params = {}
    if query:
        for par in query.split("&"):
            if "=" in par:
                k, v = par.split("=", 1)
                params[k] = v.replace("+", " ").replace("%20", " ")
    return params


# ============================================================
# CONSTRUCCION DE RESPUESTAS HTTP
# ============================================================

def respuesta_html(cuerpo):
    return ("HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n\r\n" + cuerpo)


def respuesta_json(data):
    cuerpo = json.dumps(data)
    return ("HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n\r\n" + cuerpo)


def respuesta_404():
    return ("HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n\r\n"
            "404 - Recurso no encontrado")


# ============================================================
# CARGA DE LA PAGINA HTML EN MEMORIA
# ============================================================
# Se carga UNA sola vez al arrancar para no leer el archivo
# en cada peticion (mejora notablemente el rendimiento).

def cargar_html():
    with open("index.html", "r") as f:
        return f.read()


PAGINA = cargar_html()
print("index.html cargado:", len(PAGINA), "bytes")


# ============================================================
# ROUTER: decide que hacer segun la ruta solicitada
# ============================================================

def manejar_peticion(metodo, ruta, query, cuerpo):
    params = parsear_query(query)

    # --- Pagina principal -----------------------------------
    if ruta == "/" and metodo == "GET":
        return respuesta_html(PAGINA)

    # --- Obtener mensajes nuevos ----------------------------
    if ruta == "/mensajes" and metodo == "GET":
        try:
            desde = int(params.get("desde", "0"))
        except ValueError:
            desde = 0
        nuevos = [m for m in mensajes if m["id"] > desde]
        return respuesta_json({"mensajes": nuevos, "ultimo": contador_id})

    # --- Enviar un mensaje nuevo ----------------------------
    if ruta == "/enviar" and metodo == "POST":
        try:
            data    = json.loads(cuerpo)
            usuario = (data.get("usuario") or "Anonimo").strip()
            texto   = (data.get("texto") or "").strip()
            if usuario and texto:
                msg = agregar_mensaje(usuario, texto)
                led.toggle()    # parpadeo visible al recibir un mensaje
                return respuesta_json({"ok": True, "id": msg["id"]})
            return respuesta_json({"ok": False, "error": "datos invalidos"})
        except Exception as e:
            return respuesta_json({"ok": False, "error": str(e)})

    # --- Ruta desconocida -----------------------------------
    return respuesta_404()


# ============================================================
# BUCLE PRINCIPAL DEL SERVIDOR
# ============================================================

def iniciar_servidor():
    addr = socket.getaddrinfo("0.0.0.0", PUERTO)[0][-1]
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(addr)
    s.listen(5)
    print("Servidor HTTP escuchando en puerto", PUERTO)

    while True:
        cli = None
        try:
            cli, direccion = s.accept()
            cli.settimeout(3.0)     # evitar bloqueos prolongados

            datos = cli.recv(2048)
            if datos:
                metodo, ruta, query, cuerpo = parsear_peticion(datos)
                if ruta is not None:
                    respuesta = manejar_peticion(metodo, ruta, query, cuerpo)
                    cli.sendall(respuesta.encode("utf-8"))

        except Exception as e:
            print("Error de conexion:", e)
        finally:
            if cli is not None:
                try:
                    cli.close()
                except:
                    pass
            gc.collect()   # liberar memoria entre peticiones


# ============================================================
# ARRANQUE
# ============================================================

if __name__ == "__main__":
    iniciar_ap()
    led.on()                # LED fijo = AP arriba y servidor escuchando
    try:
        iniciar_servidor()
    except KeyboardInterrupt:
        print("Servidor detenido manualmente")
        led.off()