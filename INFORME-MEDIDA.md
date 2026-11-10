# Informe de medición — ¿Cuánto trabajo es traducir System Informer?

Fecha: 2026-11-10 · Base medida: `winsiderss/systeminformer` @ `09d3b9a` (2026-11-10)

## Cómo se midió

Script propio (`herramientas/extraer_cadenas.py`) sobre el código fuente completo:

- **628 ficheros C** (`.c/.h/.cpp/.inl`) y **26 ficheros `.rc`** escaneados.
- Se extrajeron los literales anchos `L"..."` del código y las cadenas de los `.rc`.
- Cada cadena se clasificó como **visible probable** (texto que un usuario puede
  ver en pantalla) o **técnica** (rutas, claves de registro, GUID, formatos
  puros, identificadores), porque las técnicas no se traducen nunca.
- Los catálogos completos, cadena a cadena y con su origen, están en `catalogo/`.

## Las cifras

| Ámbito | Cadenas únicas visibles | Palabras |
|---|---:|---:|
| Recursos `.rc` (diálogos de la app) | 831 | 2.056 |
| App principal (`SystemInformer/`) | 4.402 | 11.595 |
| Librería de interfaz (`phlib/`) | 1.047 | 1.882 |
| Plugins (`plugins/`) | 11.932 | 26.590 |
| **Total bruto** | **≈17.500** (con solapes entre ámbitos) | **≈41.000** |

Totales sin clasificar: 19.302 cadenas únicas en código y 1.200 en `.rc`.

## La letra pequeña (importante)

El total bruto **engaña**: gran parte no es interfaz, son tablas de datos que
casualmente se muestran en alguna ventana de propiedades:

- `plugins/NetworkTools/ports.c` aporta por sí solo **11.390 apariciones**: es la
  tabla puerto → nombre de servicio (IANA). Nombres estándar que **no se traducen**.
- Lo mismo pasa con `smbios.c` (cadenas SMBIOS), `efi_guid_list.h` (nombres de
  GUID EFI) o `AgentTools/schema.c`: datos técnicos, no textos de la aplicación.
- Dentro de la app, `phsettings.h` (516) son nombres de ajustes y `tokprp.c`
  (291) descripciones de privilegios de token: visibles solo en propiedades
  avanzadas, no en el uso diario.

## El alcance realista: tres escalones

1. **Interfaz diaria (v1 recomendada)** — la ventana principal, los menús, las
   opciones y las listas de procesos/servicios/red, más los diálogos del `.rc`:
   **≈2.000 cadenas únicas / ≈6.100 palabras**. Es lo que se ve el 99 % del tiempo.
2. **App + phlib completas** — ≈5.450 cadenas / ≈13.500 palabras. Añade las
   ventanas de propiedades avanzadas y los ajustes con nombre propio.
3. **Proyecto entero con plugins** — ni se plantea como objetivo: la mayoría
   es tabla de datos sin traducir y mantenimiento infinito.

Estimación de esfuerzo para el escalón 1, traduciendo interfaz con cuidado
(≈400–600 palabras/hora incluyendo revisar cada texto en su contexto):
**unas 10–15 horas de traducción**, repartibles en sesiones. El escalón 2 sería
el doble o triple.

## Recordatorios ya conocidos

- **Sin fontanería de idiomas**: el proyecto no tiene sistema de localización
  (issue #302 de upstream, abierto desde 2018). Cada cadena se traduce en su
  sitio: `.rc` para diálogos, código C para menús/columnas/mensajes.
- **Compilación**: en Windows, con Visual Studio (carga de trabajo de C++).
  El sandbox de Ciri no puede compilarlo; el código se entrega listo para
  compilar en el PC de Gothan.
- **El driver** (`KSystemInformer`) firmado de upstream no sirve para un
  binario modificado y uno propio no carga sin firma: el fork funcionará sin
  driver, perdiendo solo las funciones avanzadas que dependen de él.
- **Licencia**: GPL-3.0. El fork debe conservarla y publicar su código.

## Siguiente paso sugerido

Empezar por el `.rc` (831 cadenas, los diálogos) porque es el bloque más
cerrado y visible, seguir por los ficheros de la interfaz diaria según el
catálogo, y mantener `catalogo/` como memoria de traducción: cada cadena con
su traducción aprobada, para que las actualizaciones de upstream se puedan
re-aplicar sin empezar de cero.
