# Traducción aplicada

## Estado

- **Diálogos (.rc): 831 de 831 cadenas traducidas** y volcadas en el
  catálogo (`catalogo/cadenas-rc.csv`, columna `traduccion_es`).
- `SystemInformer.es.rc` — el archivo de recursos principal de la app
  (`SystemInformer/SystemInformer.rc`) con las traducciones aplicadas:
  1.196 apariciones sustituidas, misma estructura y número de líneas que
  el original. Generado con `herramientas/fusionar_rc.py`; no editar a
  mano, corregir el catálogo y regenerar.

## Notas

- La línea `LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US` del .rc se ha
  dejado como está a propósito: cambiarla a español es una decisión del
  momento de compilar el fork, no de la traducción.
- Las cadenas técnicas (clases de ventana, claves de VERSIONINFO,
  `#include`, etiquetas `CPU n`...) quedan en su forma original: no son
  texto visible y tocarlas rompería el recurso.
- Los diálogos de los plugins están traducidos en el catálogo, pero aún
  no se ha generado su `.rc` traducido.
- `lotes/` guarda los lotes de trabajo de la traducción por tandas; es
  material intermedio y no se sube al repositorio (el catálogo es la
  memoria válida).

## Siguiente bloque

Las cadenas del código C (menús, columnas y mensajes de la ventana
principal) están en `catalogo/cadenas-codigo.csv`: unas 1.200 forman la
interfaz diaria según el informe de medición.
