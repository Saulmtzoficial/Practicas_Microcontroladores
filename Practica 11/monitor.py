"""
Practica 11 - Monitor de Temperatura
Requiere: pip install pyserial matplotlib
"""
import tkinter as tk
from tkinter import Canvas, Scrollbar
import matplotlib
matplotlib.use("TkAgg")
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import serial, serial.tools.list_ports
import threading
from collections import deque
from datetime import datetime

BG, BG_CARD = "#0A0E17", "#111827"
ACCENT, GREEN = "#00D4FF", "#00FF9C"
RED, YELLOW = "#FF3B5C", "#FFD166"
TEXT_PRI, TEXT_SEC = "#E8F4FD", "#6B8CAE"
TEMP_LIMITE = 70.0
MAX_PUNTOS = 120

class ThermalMonitor:
    def __init__(self, root):
        self.root = root
        self.root.title("THERMAL MONITOR - Practica 11")
        self.root.configure(bg=BG)
        self.root.geometry("1200x780")
        self.temps = deque(maxlen=MAX_PUNTOS)
        self.tiempos = deque(maxlen=MAX_PUNTOS)
        self.estado = "ESPERANDO"
        self.ser = None
        self.running = False
        self.puerto_var = tk.StringVar(value="--seleccionar--")
        self.baud_var = tk.StringVar(value="115200")
        
        # OJO: Estos metodos no venian en el texto, los comento para evitar error.
        # Descomentalos cuando agregues el resto de tu codigo.
        # self._build_ui()
        # self._actualizar_puertos()
        # self._actualizar_grafica()

    def _leer_serial(self):
        while self.running:
            try:
                raw = self.ser.readline()
                if not raw: continue
                linea = raw.decode("utf-8", errors="ignore").strip()
                if not linea.startswith("TEMP:"): continue
                
                partes = {}
                for tok in linea.split("|"):
                    if ":" in tok:
                        k, v = tok.split(":", 1)
                        partes[k] = v
                        
                temp = float(partes.get("TEMP", 0))
                estado = partes.get("ESTADO", "?")
                t_seg = int(partes.get("TIME", 0)) / 1000.0
                
                self.temps.append(temp)
                self.tiempos.append(t_seg)
                self.estado = estado
                self.root.after(0, self._actualizar_ui, temp, estado)
            except Exception as e:
                print(f"Error serial: {e}")
                break

    def _actualizar_ui(self, temp, estado):
        self.lbl_temp.config(
            text=f"{temp:.1f}",
            fg=RED if temp >= TEMP_LIMITE else ACCENT
        )
        alarma = (estado == "ALARMA")
        self.lbl_estado.config(
            text="Alarma - SOBRETEMPERATURA" if alarma else "Normal",
            fg=RED if alarma else GREEN
        )

    def _actualizar_grafica(self):
        if len(self.temps) >= 2:
            xs, ys = list(self.tiempos), list(self.temps)
            self.line.set_data(xs, ys)
            self.fill.remove()
            self.fill = self.ax.fill_between(
                xs, ys, alpha=0.10, color=ACCENT
            )
            self.ax.set_xlim(xs[0], max(xs[-1], xs[0] + 10))
            self.ax.set_ylim(max(0, min(ys) - 5), max(ys) + 5)
            self.canvas.draw_idle()
        self.root.after(500, self._actualizar_grafica)

if __name__ == "__main__":
    root = tk.Tk()
    ThermalMonitor(root)
    root.mainloop()