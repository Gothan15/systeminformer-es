# Traducción aplicada

## Estado

- **Fase 1 — Diálogos (.rc): 831 de 831 cadenas traducidas** y volcadas
  en el catálogo (`catalogo/cadenas-rc.csv`, columna `traduccion_es`).
  `SystemInformer.es.rc` — el archivo de recursos principal de la app
  con las traducciones aplicadas: 1.196 apariciones sustituidas, misma
  estructura y número de líneas que el original. Generado con
  `herramientas/fusionar_rc.py`; no editar a mano, corregir el catálogo
  y regenerar.
- **Fase 2 — Código C de la interfaz diaria: 1.220 de 1.220 cadenas
  traducidas** y volcadas en `catalogo/cadenas-codigo.csv`. Alcance:
  ventana principal y sus menús, páginas de procesos/servicios/red,
  acciones, opciones, notificaciones, tooltips y diálogos auxiliares
  (30 ficheros núcleo; la selección vive en
  `herramientas/preparar_lotes_codigo.py`). De las 1.220, **50
  reutilizan** la traducción ya aprobada en la fase 1 y 1.170 se
  tradujeron nuevas en 7 lotes con el glosario de `GLOSARIO.md`.
  - `codigo/` — copias parcheadas de los ficheros núcleo con los
    literales `L"..."` ya en español: **1.330 apariciones sustituidas
    en 26 ficheros**, mismo número de líneas que el original en todos.
    Generadas con `herramientas/fusionar_codigo.py`; no editar a mano,
    corregir el catálogo y regenerar. El detalle por fichero está en
    `codigo/resumen-aplicacion.json`.
  - En 4 ficheros (`settings.c`, `chdlg.c`, `colmgr.c`, `procgrp.c`)
    no se sustituyó nada: todas sus cadenas del alcance resultaron ser
    técnicas (nombres internos de ajustes, colores...) y quedan en
    inglés a propósito.

## Notas

- La línea `LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US` del .rc se ha
  dejado como está a propósito: cambiarla a español es una decisión del
  momento de compilar el fork, no de la traducción.
- Las cadenas técnicas (clases de ventana, claves de VERSIONINFO,
  `#include`, etiquetas `CPU n`, SID, rutas de registro, plantillas de
  comandos, fragmentos JSON...) quedan en su forma original: no son
  texto visible y tocarlas rompería el programa. La validación de la
  fusión las fuerza a idénticas automáticamente.
- Muchas cadenas del código son **fragmentos** que el programa
  concatena; las traducciones conservan los espacios del principio y
  del final exactos, y los especificadores de formato (`%s`, `%I64u`...)
  en el mismo orden y número. La validación automática lo comprueba
  todo antes de aplicar nada.
- Los diálogos de los plugins están traducidos en el catálogo .rc,
  pero aún no se ha generado su `.rc` traducido.
- `lotes/` y `lotes-codigo/` guardan los lotes de trabajo de cada
  tanda; es material intermedio y no se sube al repositorio (el
  catálogo es la memoria válida).

## Pendiente

- **Compilar y probar en Windows**: las copias parcheadas de `codigo/`
  y el `.rc` están listos para copiarse sobre un clon del upstream y
  compilar con Visual Studio en el PC de Gothan. Hasta entonces la
  fase 2 está traducida y validada, pero no probada en ejecución.
- **Escalón 2** (si se pide): páginas de propiedades avanzadas
  (procesos, identificadores, token...) y la librería `phlib`, unas
  4.200 cadenas más según el informe de medición.
