#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fusionar_rc.py — Fusiona los lotes traducidos y genera el .rc en español.

Qué hace:
  1. Lee los cinco lotes originales (traduccion/lotes/lote-N.csv) y sus
     traducciones (lote-N-es.csv), y valida la tanda completa:
       - mismos ids y mismo orden en original y traducción;
       - ninguna traducción vacía;
       - los especificadores de formato (%s, %d...) intactos;
       - las secuencias \\n / \\t y el número de & (teclas de acceso) iguales;
       - las cadenas que NO deben cambiar (#include, "CPU 12", clases de
         ventana, claves de VERSIONINFO...) forzadas a quedar idénticas.
  2. Rellena la columna traduccion_es de catalogo/cadenas-rc.csv.
  3. Aplica las traducciones sobre el .rc principal de la app y escribe
     traduccion/SystemInformer.es.rc.

Uso:
  python3 herramientas/fusionar_rc.py
"""

import csv
import re
import sys
from pathlib import Path

# Rutas del proyecto (el script vive en herramientas/).
RAIZ = Path(__file__).resolve().parent.parent
LOTES = RAIZ / "traduccion" / "lotes"
CATALOGO = RAIZ / "catalogo" / "cadenas-rc.csv"
RC_ORIGINAL = RAIZ / "upstream" / "SystemInformer" / "SystemInformer.rc"
RC_SALIDA = RAIZ / "traduccion" / "SystemInformer.es.rc"

# Especificadores de formato estilo printf que deben sobrevivir intactos.
RE_FORMATO = re.compile(r"%[-+ #0-9.*]*[diuxXfFeEgGcsSpP%]")

# Claves internas del bloque VERSIONINFO: traducirlas rompería el recurso.
CLAVES_VERSION = {
    "ProductName", "ProductVersion", "InternalName", "LegalCopyright",
    "OriginalFilename", "CompanyName", "FileDescription", "FileVersion",
    "Comments", "StringFileInfo", "VarFileInfo", "Translation",
}

# Clases de ventana estándar o propias: son identificadores, no texto.
CLASES_VENTANA = {
    "Static", "SysLink", "SysListView32", "SysTabControl32", "SysTreeView32",
    "msctls_progress32", "PhColorBox", "PhGraph", "PhHexEdit", "PhTabNew",
    "PhTreeNew", "PingGraphLayout",
}


def debe_quedar_igual(cadena: str) -> bool:
    """True si la cadena es un identificador técnico que no se traduce."""
    if cadena.startswith("#include"):
        return True
    if re.fullmatch(r"CPU \d+", cadena):
        return True
    if re.fullmatch(r"[0-9A-Fa-f]{8}", cadena):  # id. de idioma/pág. de códigos
        return True
    return cadena in CLAVES_VERSION or cadena in CLASES_VENTANA


def especificaciones(texto: str) -> list[str]:
    """Lista ordenada de especificadores de formato de una cadena."""
    return RE_FORMATO.findall(texto)


def main() -> int:
    errores: list[str] = []
    forzadas = 0

    # --- 1. Leer y alinear los cinco lotes --------------------------------
    # Los ids son correlativos (1..831) en el orden del catálogo.
    originales: dict[int, str] = {}
    traducciones: dict[int, str] = {}
    for n in range(1, 6):
        with open(LOTES / f"lote-{n}.csv", encoding="utf-8-sig") as f:
            filas = list(csv.DictReader(f))
        with open(LOTES / f"lote-{n}-es.csv", encoding="utf-8-sig") as f:
            filas_es = list(csv.DictReader(f))
        if len(filas) != len(filas_es):
            errores.append(f"lote {n}: {len(filas)} originales vs "
                           f"{len(filas_es)} traducciones")
            continue
        for fila, fila_es in zip(filas, filas_es):
            id_ = int(fila["id"])
            if id_ != int(fila_es["id"]):
                errores.append(f"lote {n}: id desalineado en {id_}")
                continue
            originales[id_] = fila["cadena_en"]
            traducciones[id_] = fila_es["traduccion_es"]

    print(f"Cadenas alineadas: {len(traducciones)}")

    # --- 2. Validar cada pareja original/traducción ------------------------
    for id_, en in originales.items():
        es = traducciones.get(id_, "")
        if not es.strip():
            errores.append(f"id {id_}: traducción vacía ({en!r})")
            continue
        if debe_quedar_igual(en):
            if es != en:
                # Corrección automática: lo técnico manda sobre el lote.
                traducciones[id_] = en
                forzadas += 1
            continue
        if especificaciones(en) != especificaciones(es):
            errores.append(f"id {id_}: formato distinto: {en!r} -> {es!r}")
        if en.count("\\n") != es.count("\\n") or en.count("\\t") != es.count("\\t"):
            errores.append(f"id {id_}: secuencias \\n/\\t distintas: {en!r}")
        if en.count("&") != es.count("&"):
            errores.append(f"id {id_}: número de & distinto: {en!r} -> {es!r}")

    print(f"Correcciones automáticas (técnicas forzadas): {forzadas}")
    if errores:
        print(f"\nERRORES ({len(errores)}):")
        for e in errores[:30]:
            print(" -", e)
        return 1
    print("Validación superada sin errores.")

    # --- 3. Rellenar el catálogo -------------------------------------------
    with open(CATALOGO, encoding="utf-8-sig") as f:
        filas_cat = list(csv.DictReader(f))
    por_cadena = {en: traducciones[i] for i, en in originales.items()}
    sin_match = 0
    for fila in filas_cat:
        es = por_cadena.get(fila["cadena_en"])
        if es is None:
            sin_match += 1
        else:
            fila["traduccion_es"] = es
    with open(CATALOGO, "w", newline="", encoding="utf-8-sig") as f:
        w = csv.DictWriter(f, fieldnames=["cadena_en", "traduccion_es",
                                          "apariciones", "archivo", "linea"])
        w.writeheader()
        w.writerows(filas_cat)
    print(f"Catálogo actualizado ({len(filas_cat)} filas, "
          f"{sin_match} sin correspondencia).")

    # --- 4. Generar el .rc traducido de la app principal -------------------
    # Sustitución por token completo entrecomillado: en los .rc las comillas
    # internas se escapan duplicándolas (""). Sustituir el token entero
    # evita tocar cadenas que sean prefijo de otras.
    texto = RC_ORIGINAL.read_text(encoding="utf-8", errors="replace")
    aplicadas = 0
    for en, es in por_cadena.items():
        token_en = '"' + en.replace('"', '""') + '"'
        if token_en in texto:
            token_es = '"' + es.replace('"', '""') + '"'
            aplicadas += texto.count(token_en)
            texto = texto.replace(token_en, token_es)
    RC_SALIDA.parent.mkdir(parents=True, exist_ok=True)
    RC_SALIDA.write_text(texto, encoding="utf-8")
    print(f"{RC_SALIDA.name}: {aplicadas} apariciones sustituidas.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
