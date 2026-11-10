#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
preparar_lotes_codigo.py — Prepara la fase 2: cadenas del código C.

Qué hace:
  1. Selecciona del catálogo de código (catalogo/cadenas-codigo.csv) solo
     las cadenas cuya primera aparición está en los ficheros de la
     INTERFAZ DIARIA: ventana principal y sus páginas de procesos,
     servicios y red, acciones sobre procesos/servicios, opciones,
     notificaciones, tooltips y diálogos auxiliares que se abren por
     código. Es el alcance que el INFORME-MEDIDA cifró en ≈1.200 cadenas.
     Quedan fuera, a propósito, las páginas de propiedades avanzadas
     (tokprp, hndlprp...), las tablas de datos y los plugins enteros:
     eso es el escalón 2 y se hará aparte si Gothan lo pide.
  2. Las cadenas que ya se tradujeron en la fase 1 (diálogos .rc) con el
     mismo texto inglés reutilizan su traducción aprobada y NO vuelven
     a traducirse: van a reutilizadas.csv.
  3. El resto se reparte en lotes consecutivos del catálogo (que está
     ordenado alfabéticamente, así cadenas parecidas caen en el mismo
     lote y se traducen de forma coherente) en traduccion/lotes-codigo/.

Uso:
  python3 herramientas/preparar_lotes_codigo.py
"""

import csv
import json
import math
from pathlib import Path

# Rutas del proyecto (el script vive en herramientas/).
RAIZ = Path(__file__).resolve().parent.parent
CAT_CODIGO = RAIZ / "catalogo" / "cadenas-codigo.csv"
CAT_RC = RAIZ / "catalogo" / "cadenas-rc.csv"
SALIDA = RAIZ / "traduccion" / "lotes-codigo"

# Número de lotes de traducción en paralelo (como en la fase 1, tandas
# de ~170 cadenas: traducibles en una sesión con el contexto a la vista).
NUM_LOTES = 7

# Ficheros de la interfaz diaria (primera aparición en el catálogo).
# Entre paréntesis, qué pinta cada uno en la ventana que ve el usuario.
ARCHIVOS_NUCLEO = {
    "SystemInformer/mainwnd.c",       # ventana principal y sus menús
    "SystemInformer/main.c",          # arranque y mensajes iniciales
    "SystemInformer/mwpgproc.c",      # página de procesos (gráficos)
    "SystemInformer/mwpgsrv.c",       # página de servicios
    "SystemInformer/mwpgnet.c",       # página de red
    "SystemInformer/proctree.c",      # árbol de procesos y sus columnas
    "SystemInformer/srvlist.c",       # lista de servicios
    "SystemInformer/netlist.c",       # lista de conexiones de red
    "SystemInformer/actions.c",       # acciones: finalizar, suspender...
    "SystemInformer/options.c",       # ventana de opciones
    "SystemInformer/settings.c",      # ajustes y sus mensajes
    "SystemInformer/miniinfo.c",      # ventana mini de información
    "SystemInformer/notifico.c",      # iconos de notificación
    "SystemInformer/notiftoast.cpp",  # notificaciones toast
    "SystemInformer/about.c",         # diálogo «Acerca de» por código
    "SystemInformer/itemtips.c",      # tooltips de los procesos
    "SystemInformer/log.c",           # registro de eventos
    "SystemInformer/chcol.c",         # selector de columnas
    "SystemInformer/colmgr.c",        # gestor de columnas
    "SystemInformer/runas.c",         # «Ejecutar como»
    "SystemInformer/affinity.c",      # afinidad de CPU
    "SystemInformer/plugman.c",       # gestor de complementos
    "SystemInformer/hndlmenu.c",      # menú de identificadores
    "SystemInformer/findobj.c",       # buscar objeto/ventana
    "SystemInformer/envdlg.c",        # variables de entorno
    "SystemInformer/chdlg.c",         # diálogos de elección
    "SystemInformer/chproc.c",        # elegir proceso
    "SystemInformer/infodlg.c",       # diálogo de información
    "SystemInformer/mtgndlg.c",       # diálogo de mitigaciones
    "SystemInformer/procgrp.c",       # grupos de procesos
}


def main() -> int:
    # --- 1. Traducciones ya aprobadas en la fase 1 (.rc) -------------------
    with open(CAT_RC, encoding="utf-8-sig") as f:
        ya_traducidas = {
            fila["cadena_en"]: fila["traduccion_es"]
            for fila in csv.DictReader(f)
            if fila["traduccion_es"]
        }

    # --- 2. Seleccionar la interfaz diaria del catálogo de código ---------
    with open(CAT_CODIGO, encoding="utf-8-sig") as f:
        filas = [f_ for f_ in csv.DictReader(f)
                 if f_["archivo"] in ARCHIVOS_NUCLEO]
    print(f"Cadenas de la interfaz diaria: {len(filas)}")

    # --- 3. Separar reutilizadas y pendientes ------------------------------
    reutilizadas = [f_ for f_ in filas if f_["cadena_en"] in ya_traducidas]
    pendientes = [f_ for f_ in filas if f_["cadena_en"] not in ya_traducidas]
    print(f"Reutilizadas de la fase 1: {len(reutilizadas)}")
    print(f"Pendientes de traducir: {len(pendientes)}")

    SALIDA.mkdir(parents=True, exist_ok=True)

    # Las reutilizadas se guardan con su traducción para que el script de
    # fusión las aplique al catálogo igual que las nuevas.
    with open(SALIDA / "reutilizadas.csv", "w", newline="",
              encoding="utf-8-sig") as f:
        w = csv.writer(f)
        w.writerow(["cadena_en", "traduccion_es"])
        for fila in reutilizadas:
            w.writerow([fila["cadena_en"], ya_traducidas[fila["cadena_en"]]])

    # --- 4. Repartir las pendientes en lotes consecutivos ------------------
    tam = math.ceil(len(pendientes) / NUM_LOTES)
    for n in range(NUM_LOTES):
        trozo = pendientes[n * tam:(n + 1) * tam]
        if not trozo:
            continue
        with open(SALIDA / f"lote-{n + 1}.csv", "w", newline="",
                  encoding="utf-8-sig") as f:
            w = csv.writer(f)
            w.writerow(["id", "cadena_en", "archivo", "linea"])
            # Id correlativo DENTRO del lote, para alinear original y
            # traducción fila a fila al fusionar.
            for i, fila in enumerate(trozo, start=1):
                w.writerow([i, fila["cadena_en"], fila["archivo"],
                            fila["linea"]])
        print(f"lote-{n + 1}.csv: {len(trozo)} cadenas")

    # --- 5. Resumen de la preparación ---------------------------------------
    resumen = {
        "archivos_nucleo": sorted(ARCHIVOS_NUCLEO),
        "cadenas_interfaz_diaria": len(filas),
        "reutilizadas_fase_1": len(reutilizadas),
        "pendientes": len(pendientes),
        "lotes": NUM_LOTES,
    }
    with open(SALIDA / "resumen-preparacion.json", "w",
              encoding="utf-8") as f:
        json.dump(resumen, f, ensure_ascii=False, indent=2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
