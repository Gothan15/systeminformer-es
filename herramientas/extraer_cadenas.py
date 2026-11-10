#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
extraer_cadenas.py — Medición de cadenas traducibles de System Informer.

Qué hace:
  Recorre el código fuente de System Informer (clon en ./upstream) y extrae
  todas las cadenas de texto que podrían necesitar traducción al español:

    1. Las cadenas de los archivos de recursos (.rc): diálogos y controles.
    2. Los literales anchos L"..." del código C: menús, columnas, mensajes
       y etiquetas que la aplicación construye por código.

  Cada cadena se clasifica como "visible probable" (texto que un usuario
  puede llegar a ver en pantalla) o "técnica" (rutas, claves de registro,
  formatos puros, GUID, identificadores...), para medir el trabajo real
  sin inflarlo con cadenas que jamás se traducen.

Salida (en ./catalogo):
  - cadenas-rc.csv      Cadenas únicas de los .rc, con apariciones y origen.
  - cadenas-codigo.csv  Cadenas únicas del código C, por ámbito.
  - resumen.json        Todas las cifras de la medición.

Uso:
  python3 herramientas/extraer_cadenas.py
"""

import csv
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

# ---------------------------------------------------------------------------
# Rutas: el script vive en herramientas/ y el proyecto en la carpeta padre.
# ---------------------------------------------------------------------------
RAIZ = Path(__file__).resolve().parent.parent
UPSTREAM = RAIZ / "upstream"
SALIDA = RAIZ / "catalogo"

# Ámbitos a escanear: (nombre, carpeta dentro de upstream).
# La app principal y su librería de interfaz forman el alcance inicial;
# los plugins se cuentan aparte porque son piezas opcionales.
AMBITOS = [
    ("app", "SystemInformer"),
    ("phlib", "phlib"),
    ("plugins", "plugins"),
]

EXTENSIONES_CODIGO = {".c", ".h", ".cpp", ".inl"}

# ---------------------------------------------------------------------------
# Expresiones regulares de extracción.
#
# En C, los literales anchos se escriben L"texto" y pueden llevar escapes
# con barra invertida (\" \\ \n ...). En los .rc, las comillas internas se
# duplican ("") y no hay literales con prefijo L.
# ---------------------------------------------------------------------------
RE_LITERAL_ANCHO = re.compile(r'L"((?:[^"\\]|\\.)*)"')
RE_CADENA_RC = re.compile(r'"((?:[^"]|"")*)"')

# Patrones para reconocer cadenas técnicas que nunca se traducen.
RE_GUID = re.compile(
    r"^\{?[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
    r"[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\}?$"
)
RE_IDENTIFICADOR = re.compile(r"^[A-Z][A-Z0-9_]*$")          # TIPO_CONSTANTE
RE_ESPECIFICADOR = re.compile(r"%[-+ #0-9.*]*[diuoxXfFeEgGcsSpP%]")
RE_EXTENSION_ARCHIVO = re.compile(r"\.(exe|dll|sys|mui|bin|dat)$", re.IGNORECASE)


def desescapar_c(texto: str) -> str:
    """Convierte los escapes habituales de C en los caracteres reales."""
    return (
        texto.replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace("\\r", "\r")
        .replace('\\"', '"')
        .replace("\\\\", "\\")
    )


def es_tecnica(texto: str) -> bool:
    """Decide si una cadena es técnica (no se traduce nunca)."""
    limpio = texto.strip()
    if len(limpio) < 2:
        return True  # vacías, sueltas o de un solo carácter
    if not re.search(r"[A-Za-z]", limpio):
        return True  # sin letras: números, signos, formatos puros
    if RE_GUID.match(limpio):
        return True  # identificadores GUID
    if RE_IDENTIFICADOR.match(limpio):
        return True  # constantes en mayúsculas con guiones bajos
    if limpio.startswith(("http://", "https://")):
        return True  # direcciones web
    if "\\" in limpio and " " not in limpio:
        return True  # rutas y claves de registro (no llevan espacios)
    if RE_EXTENSION_ARCHIVO.search(limpio) and " " not in limpio:
        return True  # nombres de archivo sueltos (algo.exe, algo.dll)
    # Si al quitar los especificadores de formato (%s, %d...) no queda
    # ninguna letra, la cadena es solo un formato, no un texto.
    sin_formato = RE_ESPECIFICADOR.sub("", limpio)
    if not re.search(r"[A-Za-z]", sin_formato):
        return True
    return False


def contar_palabras(texto: str) -> int:
    """Cuenta palabras con letras (para estimar el esfuerzo de traducción)."""
    return len(re.findall(r"[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]+", texto))


def extraer_de_archivo(ruta: Path, patron: re.Pattern, es_rc: bool):
    """Devuelve (cadena, línea) por cada coincidencia del patrón."""
    try:
        contenido = ruta.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return
    for numero_linea, linea in enumerate(contenido.splitlines(), start=1):
        for coincidencia in patron.finditer(linea):
            cruda = coincidencia.group(1)
            if es_rc:
                # En los .rc las comillas internas se escriben duplicadas.
                texto = cruda.replace('""', '"')
            else:
                texto = desescapar_c(cruda)
            yield texto, numero_linea


def main() -> int:
    if not UPSTREAM.is_dir():
        print(f"No existe {UPSTREAM}: clona primero el repositorio original.")
        return 1

    SALIDA.mkdir(parents=True, exist_ok=True)

    # Contadores generales de la medición.
    resumen = {
        "ficheros_codigo_escaneados": 0,
        "ficheros_rc_escaneados": 0,
        "ambitos": {},
    }

    # Catálogos: cadena -> [apariciones, primer archivo, primera línea].
    catalogo_rc: dict[str, list] = {}
    catalogo_codigo: dict[str, list] = {}

    # Apariciones visibles por archivo, para saber dónde se concentra
    # la interfaz (útil al definir el alcance de una primera versión).
    visibles_por_archivo: Counter = Counter()

    for ambito, carpeta in AMBITOS:
        base = UPSTREAM / carpeta
        if not base.is_dir():
            continue

        datos = {
            "ficheros": 0,
            "literales_totales": 0,
            "unicas_visibles": 0,
            "palabras_visibles": 0,
        }
        unicas_visibles_ambito: set[str] = set()

        # --- Código C: literales anchos L"..." ---------------------------
        for ruta in sorted(base.rglob("*")):
            if ruta.suffix.lower() not in EXTENSIONES_CODIGO or not ruta.is_file():
                continue
            datos["ficheros"] += 1
            resumen["ficheros_codigo_escaneados"] += 1
            relativo = str(ruta.relative_to(UPSTREAM))
            for texto, linea in extraer_de_archivo(ruta, RE_LITERAL_ANCHO, es_rc=False):
                datos["literales_totales"] += 1
                registro = catalogo_codigo.setdefault(texto, [0, ambito, relativo, linea])
                registro[0] += 1
                if not es_tecnica(texto):
                    unicas_visibles_ambito.add(texto)
                    visibles_por_archivo[relativo] += 1

        # --- Recursos .rc -------------------------------------------------
        for ruta in sorted(base.rglob("*.rc")):
            resumen["ficheros_rc_escaneados"] += 1
            relativo = str(ruta.relative_to(UPSTREAM))
            for texto, linea in extraer_de_archivo(ruta, RE_CADENA_RC, es_rc=True):
                registro = catalogo_rc.setdefault(texto, [0, relativo, linea])
                registro[0] += 1
                if ambito == "app" and not es_tecnica(texto):
                    visibles_por_archivo[relativo] += 1

        datos["unicas_visibles"] = len(unicas_visibles_ambito)
        datos["palabras_visibles"] = sum(
            contar_palabras(c) for c in unicas_visibles_ambito
        )
        resumen["ambitos"][ambito] = datos

    # --- Cifras de los .rc (solo diálogos de la app principal y plugins) --
    rc_visibles = {c: r for c, r in catalogo_rc.items() if not es_tecnica(c)}
    resumen["rc"] = {
        "cadenas_unicas_totales": len(catalogo_rc),
        "unicas_visibles": len(rc_visibles),
        "palabras_visibles": sum(contar_palabras(c) for c in rc_visibles),
    }

    # --- Totales del código C ---------------------------------------------
    codigo_visibles = {c: r for c, r in catalogo_codigo.items() if not es_tecnica(c)}
    resumen["codigo"] = {
        "cadenas_unicas_totales": len(catalogo_codigo),
        "unicas_visibles": len(codigo_visibles),
        "palabras_visibles": sum(contar_palabras(c) for c in codigo_visibles),
    }
    resumen["archivos_con_mas_cadenas_visibles"] = [
        {"archivo": a, "apariciones_visibles": n}
        for a, n in visibles_por_archivo.most_common(15)
    ]

    # --- Escritura de los catálogos ---------------------------------------
    # utf-8-sig para que Excel en Windows abra bien las eñes y acentos.
    with open(SALIDA / "cadenas-rc.csv", "w", newline="", encoding="utf-8-sig") as f:
        escritor = csv.writer(f)
        escritor.writerow(["cadena_en", "traduccion_es", "apariciones", "archivo", "linea"])
        for cadena, (n, archivo, linea) in sorted(catalogo_rc.items()):
            if not es_tecnica(cadena):
                escritor.writerow([cadena, "", n, archivo, linea])

    with open(SALIDA / "cadenas-codigo.csv", "w", newline="", encoding="utf-8-sig") as f:
        escritor = csv.writer(f)
        escritor.writerow(
            ["cadena_en", "traduccion_es", "ambito", "apariciones", "archivo", "linea"]
        )
        for cadena, (n, ambito, archivo, linea) in sorted(catalogo_codigo.items()):
            if not es_tecnica(cadena):
                escritor.writerow([cadena, "", ambito, n, archivo, linea])

    with open(SALIDA / "resumen.json", "w", encoding="utf-8") as f:
        json.dump(resumen, f, ensure_ascii=False, indent=2)

    # --- Resumen por consola -----------------------------------------------
    print(json.dumps(resumen, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
