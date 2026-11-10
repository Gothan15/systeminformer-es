# System Informer en español

Fork de traducción al español de [System Informer](https://github.com/winsiderss/systeminformer)
(el antiguo Process Hacker), el monitor de procesos de código abierto.
El proyecto original solo se publica en inglés —su issue de traducciones
lleva abierto desde 2018—, así que este repositorio organiza el trabajo de
traducirlo con un alcance cerrado y medido.

## Estado: escalón 1 completo — interfaz diaria traducida

- **Fase 1 (diálogos .rc):** 831/831 cadenas, con el `.rc` parcheado en
  `traduccion/SystemInformer.es.rc`.
- **Fase 2 (código C):** 1.220/1.220 cadenas de la ventana principal,
  menús, opciones y listas, con las copias parcheadas del código en
  `traduccion/codigo/` (1.330 apariciones en 26 ficheros).

Entre las dos fases, la interfaz que se ve a diario está en español.
Queda pendiente compilar y probar en Windows, y el escalón 2
(propiedades avanzadas y `phlib`) si se quiere seguir. El detalle vive
en `traduccion/LEEME.md`.

La medición previa está en **[INFORME-MEDIDA.md](INFORME-MEDIDA.md)**;
el resumen corto:

- La interfaz que se ve a diario son **≈2.000 cadenas / ≈6.100 palabras**
  (diálogos del `.rc` + ventana principal, menús, opciones y listas).
- El total bruto del proyecto (≈17.500 cadenas) es un espejismo: la mayor
  parte son tablas de datos técnicos que no se traducen.

## Estructura

- `INFORME-MEDIDA.md` — la medición y el plan por escalones.
- `catalogo/` — todas las cadenas extraídas, con su archivo y línea de
  origen y una columna vacía para ir rellenando la traducción. Es la
  memoria de traducción del proyecto.
  - `cadenas-rc.csv` — diálogos y controles de los `.rc`.
  - `cadenas-codigo.csv` — menús, columnas y mensajes del código C.
  - `resumen.json` — las cifras de la medición.
- `herramientas/extraer_cadenas.py` — el script que genera el catálogo
  desde el código original (`python3 herramientas/extraer_cadenas.py`).
- `traduccion/` — la traducción aplicada: el `.rc` en español, las
  copias parcheadas del código C, el glosario fijo y las notas de cada
  fase en `LEEME.md`. Todo se genera desde el catálogo con las
  herramientas de `herramientas/` (`fusionar_rc.py` y
  `fusionar_codigo.py`); no se edita a mano.
- `upstream/` — clon del repositorio original, solo como referencia de
  medición (no forma parte de este repositorio).

## Licencia

System Informer es GPL-3.0; este fork conserva la misma licencia.
