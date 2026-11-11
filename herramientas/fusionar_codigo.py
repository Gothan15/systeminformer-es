#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fusionar_codigo.py — Fusiona los lotes de la fase 2 y aplica la traducción.

Qué hace:
  1. Lee los siete lotes (traduccion/lotes-codigo/lote-N.csv) con sus
     traducciones (lote-N-es.csv) más las reutilizadas de la fase 1, y
     valida la tanda completa:
       - mismos ids y mismo orden en original y traducción;
       - ninguna traducción vacía;
       - los especificadores de formato (%s, %I64u...) intactos;
       - el mismo número de saltos de línea, tabuladores y &;
       - los espacios del principio y del final EXACTOS (muchas cadenas
         son fragmentos que el programa concatena);
       - las cadenas técnicas que no deben cambiar (SID, rutas de
         registro, plantillas de comandos, JSON...) forzadas a idénticas.
  2. Rellena la columna traduccion_es de catalogo/cadenas-codigo.csv
     para las 1.220 cadenas de la interfaz diaria.
  3. Genera las copias parcheadas del código en traduccion/codigo/,
     sustituyendo los literales L"..." de los ficheros núcleo por su
     traducción ya escapada como literal C. No edita el upstream: son
     copias listas para revisar y compilar en el PC de Gothan.

Uso:
  python3 herramientas/fusionar_codigo.py
"""

import csv
import json
import re
import sys
from pathlib import Path

# Rutas del proyecto (el script vive en herramientas/).
RAIZ = Path(__file__).resolve().parent.parent
LOTES = RAIZ / "traduccion" / "lotes-codigo"
CATALOGO = RAIZ / "catalogo" / "cadenas-codigo.csv"
UPSTREAM = RAIZ / "upstream"
SALIDA_CODIGO = RAIZ / "traduccion" / "codigo"

# Especificadores de formato estilo printf/MSVC que deben sobrevivir
# intactos: %s, %lu, %.2f, %I64u, %Ix, %02lu, %%...
RE_FORMATO = re.compile(
    r"%(?:I64|I32|I)?[-+ #0-9.*]*(?:ll|hh|l|h|w)?[diuxXfFeEgGcsSpP%]"
)

# Literales anchos del código C, con sus escapes tal cual están escritos.
RE_LITERAL = re.compile(r'L"((?:[^"\\]|\\.)*)"')


def especificaciones(texto: str) -> list[str]:
    """Lista ordenada de especificadores de formato de una cadena."""
    return RE_FORMATO.findall(texto)


def borde(texto: str) -> tuple[str, str]:
    """Espacios del principio y del final (críticos en fragmentos)."""
    principio = texto[:len(texto) - len(texto.lstrip(" "))]
    final = texto[len(texto.rstrip(" ")):]
    return principio, final


def debe_quedar_igual(cadena: str) -> bool:
    """True si es una cadena técnica que jamás se traduce."""
    limpia = cadena.strip()
    # Identificadores de seguridad (SID) completos.
    if re.fullmatch(r"S-\d+(?:-\d+)+.*", limpia):
        return True
    # Rutas del registro: son claves reales de Windows.
    if "\\CurrentControlSet\\" in cadena or limpia.startswith(
            ("Software\\Microsoft", "System\\CurrentControlSet")):
        return True
    # Plantillas con rutas a ejecutables/librerías.
    if "\\" in cadena and (".exe" in cadena.lower()
                           or ".dll" in cadena.lower()):
        return True
    # Fragmentos JSON (p. ej. de las notificaciones toast).
    if limpia.startswith('"') and ('","' in cadena or '":' in cadena):
        return True
    # Plantillas de línea de comandos: empiezan por - o /, llevan
    # formatos y apenas tienen palabras (no son frases).
    if limpia[:1] in ("-", "/") and "%" in cadena:
        palabras = re.findall(r"[A-Za-z]{2,}", cadena)
        if len(palabras) <= 3:
            return True
    return False


def desescapar_c(texto: str) -> str:
    """El mismo desescapado que usa extraer_cadenas.py al catalogar.

    Así, el literal leído del código y la cadena del catálogo son la
    misma cosa y se pueden cruzar sin sorpresas.
    """
    return (
        texto.replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace("\\r", "\r")
        .replace('\\"', '"')
        .replace("\\\\", "\\")
    )


# Escapes de C VÁLIDOS que extraer_cadenas.py NO desescapa (solo toca
# \n \t \r \" \\). Si una cadena del catálogo lleva uno de estos, en el
# código venía escrito con UNA barra y es un escape de verdad: el sufijo
# \b de los atajos de menú (retroceso separador) o el \U00002699 de los
# iconos (nombre de carácter universal). Esos se devuelven tal cual.
RE_ESCAPE_VALIDO = re.compile(r"\\[bfvaeuUxX0-7]")


def escapar_c(original: str, traduccion: str) -> str:
    """Convierte la traducción en el interior de un literal C L"...".

    Detalle delicado (aprendido del error C4129/C2220 de la corrida
    n.º 3 de Actions): una barra seguida de letra en el catálogo puede
    ser dos cosas opuestas —
      a) un escape C válido no desescapado al catalogar (\\b, \\U...):
         se devuelve con su única barra, como estaba en el código;
      b) una barra REAL de una ruta o cuenta (ProgramData\\SystemInformer,
         NT AUTHORITY\\SYSTEM): al catalogar venía de «\\\\» en el código
         y hay que volver a duplicarla; si se escribe con una sola,
         el compilador lee \\S o \\C, no reconoce el escape y, con las
         advertencias tratadas como errores, la compilación muere.
    Por eso solo se protegen los escapes de la lista válida; cualquier
    otra barra se escapa (se duplica) como las demás.
    """
    resultado = traduccion
    protegidas: dict[str, str] = {}
    for i, secuencia in enumerate(
            sorted(set(RE_ESCAPE_VALIDO.findall(original)))):
        if secuencia in resultado:
            marca = f"\ue000{i}\ue001"
            protegidas[marca] = secuencia
            resultado = resultado.replace(secuencia, marca)
    resultado = (
        resultado.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\t", "\\t")
        .replace("\r", "\\r")
    )
    for marca, secuencia in protegidas.items():
        resultado = resultado.replace(marca, secuencia)
    return resultado


def main() -> int:
    errores: list[str] = []
    forzadas = 0
    mapa: dict[str, str] = {}  # cadena_en -> traduccion_es (fase completa)

    # --- 1. Leer y validar los siete lotes ---------------------------------
    for n in range(1, 8):
        with open(LOTES / f"lote-{n}.csv", encoding="utf-8-sig") as f:
            filas = list(csv.DictReader(f))
        with open(LOTES / f"lote-{n}-es.csv", encoding="utf-8-sig") as f:
            filas_es = list(csv.DictReader(f))
        if len(filas) != len(filas_es):
            errores.append(f"lote {n}: {len(filas)} originales vs "
                           f"{len(filas_es)} traducciones")
            continue
        for fila, fila_es in zip(filas, filas_es):
            if fila["id"] != fila_es["id"]:
                errores.append(f"lote {n}: id desalineado en {fila['id']}")
                continue
            en, es = fila["cadena_en"], fila_es["traduccion_es"]
            if not es.strip() and en.strip():
                errores.append(f"lote {n} id {fila['id']}: vacía ({en!r})")
                continue
            if debe_quedar_igual(en):
                if es != en:
                    es = en
                    forzadas += 1
            else:
                if especificaciones(en) != especificaciones(es):
                    errores.append(
                        f"lote {n} id {fila['id']}: formato distinto: "
                        f"{en!r} -> {es!r}")
                if en.count("\n") != es.count("\n") \
                        or en.count("\t") != es.count("\t"):
                    errores.append(
                        f"lote {n} id {fila['id']}: saltos/tabs: {en!r}")
                if en.count("&") != es.count("&"):
                    errores.append(
                        f"lote {n} id {fila['id']}: & distinto: {en!r}")
                if borde(en) != borde(es):
                    errores.append(
                        f"lote {n} id {fila['id']}: espacios de borde: "
                        f"{en!r} -> {es!r}")
            mapa[en] = es
    print(f"Lotes alineados: {len(mapa)} cadenas")

    # --- 2. Sumar las reutilizadas de la fase 1 ----------------------------
    with open(LOTES / "reutilizadas.csv", encoding="utf-8-sig") as f:
        for fila in csv.DictReader(f):
            if fila["cadena_en"] in mapa \
                    and mapa[fila["cadena_en"]] != fila["traduccion_es"]:
                errores.append(
                    f"reutilizada en conflicto: {fila['cadena_en']!r}")
            mapa[fila["cadena_en"]] = fila["traduccion_es"]
    print(f"Total fase 2 (con reutilizadas): {len(mapa)}")
    print(f"Correcciones automáticas (técnicas forzadas): {forzadas}")
    if errores:
        print(f"\nERRORES ({len(errores)}):")
        for e in errores[:40]:
            print(" -", e)
        return 1
    print("Validación superada sin errores.")

    # --- 3. Rellenar el catálogo de código ----------------------------------
    with open(CATALOGO, encoding="utf-8-sig") as f:
        filas_cat = list(csv.DictReader(f))
    # Al leer con utf-8-sig la marca BOM desaparece y la clave queda
    # limpia como «cadena_en».
    rellenadas = 0
    for fila in filas_cat:
        if fila["cadena_en"] in mapa:
            fila["traduccion_es"] = mapa[fila["cadena_en"]]
            rellenadas += 1
    with open(CATALOGO, "w", newline="", encoding="utf-8-sig") as f:
        w = csv.DictWriter(
            f, fieldnames=["cadena_en", "traduccion_es", "ambito",
                           "apariciones", "archivo", "linea"])
        w.writeheader()
        w.writerows(filas_cat)
    print(f"Catálogo actualizado: {rellenadas} filas con traducción.")

    # --- 4. Generar las copias parcheadas del código ------------------------
    with open(LOTES / "resumen-preparacion.json", encoding="utf-8") as f:
        archivos = json.load(f)["archivos_nucleo"]
    resumen_archivos: dict[str, int] = {}
    total_aplicadas = 0
    for archivo in archivos:
        ruta = UPSTREAM / archivo
        if not ruta.is_file():
            resumen_archivos[archivo] = 0
            continue
        texto = ruta.read_text(encoding="utf-8", errors="replace")
        aplicadas = 0

        def sustituir(coincidencia: re.Match) -> str:
            nonlocal aplicadas
            en = desescapar_c(coincidencia.group(1))
            es = mapa.get(en)
            if es is None or es == en:
                return coincidencia.group(0)
            aplicadas += 1
            return 'L"' + escapar_c(en, es) + '"'

        nuevo = RE_LITERAL.sub(sustituir, texto)
        # La sustitución nunca debe cambiar el número de líneas: si lo
        # hiciera, un salto real se habría colado sin escapar.
        if nuevo.count("\n") != texto.count("\n"):
            print(f"¡Ojo! {archivo}: cambió el número de líneas")
            return 1
        if aplicadas:
            destino = SALIDA_CODIGO / archivo
            destino.parent.mkdir(parents=True, exist_ok=True)
            destino.write_text(nuevo, encoding="utf-8")
        resumen_archivos[archivo] = aplicadas
        total_aplicadas += aplicadas
    with open(SALIDA_CODIGO / "resumen-aplicacion.json", "w",
              encoding="utf-8") as f:
        json.dump({"apariciones_sustituidas": total_aplicadas,
                   "por_archivo": resumen_archivos},
                  f, ensure_ascii=False, indent=2)
    print(f"Copias parcheadas: {total_aplicadas} apariciones sustituidas "
          f"en {sum(1 for v in resumen_archivos.values() if v)} ficheros.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
