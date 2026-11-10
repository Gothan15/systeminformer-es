# Glosario fijo — fase 2 (código C)

El mismo criterio de la fase 1 (.rc): estos términos se traducen SIEMPRE
igual, en todas las cadenas y todos los lotes.

| Inglés | Español |
|---|---|
| Process / Processes | Proceso / Procesos |
| Service / Services | Servicio / Servicios |
| Terminate | Finalizar |
| Kill | Matar |
| Suspend | Suspender |
| Resume | Reanudar |
| Restart | Reiniciar |
| Start | Iniciar |
| Stop | Detener |
| Pause | Pausar |
| Continue | Continuar |
| Refresh | Actualizar |
| Properties | Propiedades |
| Options | Opciones |
| Settings | Ajustes |
| Search / Find | Buscar |
| Run / Run As | Ejecutar / Ejecutar como |
| Plugins | Complementos |
| Handles | Identificadores |
| Threads | Subprocesos |
| Modules | Módulos |
| Memory | Memoria |
| Network | Red |
| Disk | Disco |
| Performance | Rendimiento |
| Highlighting | Resaltado |
| Symbols | Símbolos |
| Log | Registro |
| Graphs | Gráficos |
| Column / Columns | Columna / Columnas |
| User | Usuario |
| Owner | Propietario |
| Parent | Padre |
| Command line | Línea de comandos |
| Image (de un proceso) | Imagen |
| Session | Sesión |
| Unknown | Desconocido |
| None | Ninguno |
| Apply | Aplicar |
| Cancel | Cancelar |
| Save | Guardar |
| Reset | Restablecer |
| Browse... | Examinar... |
| Access denied | Acceso denegado |
| Are you sure you want to...? | ¿Está seguro de que desea...? |

## Reglas que no se negocian

1. **Especificadores de formato intactos**: `%s`, `%lu`, `%Ix`, `%I64u`,
   `%.2f%%`... deben aparecer en la traducción en el mismo orden y número.
2. **Saltos y tabuladores**: las cadenas del catálogo llevan saltos de
   línea y `\t` REALES dentro del texto; la traducción conserva el mismo
   número de cada uno, en la misma posición aproximada.
3. **Espacios de borde**: muchas cadenas son FRAGMENTOS que el programa
   concatena (`" and its descendants"`, `" processes."`). Los espacios
   al principio y al final se conservan EXACTOS: ni uno más ni uno menos.
4. **Teclas de acceso**: el `&` de los menús (`&Connect`) se conserva,
   delante de la misma letra si suena bien en español o de otra letra
   de la palabra traducida, pero siempre exactamente un `&` por cadena
   si el original lleva uno, y ninguno si no lo lleva.
5. **No se traduce lo técnico**: plantillas de línea de comandos
   (`-p %lu`, `/PROCESSID:`), rutas a ejecutables
   (`%SystemRoot%\explorer.exe ...`), fragmentos JSON (`","file":`),
   nombres de ajustes internos o identificadores. Esas cadenas se
   devuelven IDÉNTICAS al original.
6. **Nombres propios y marcas** (System Informer, Windows, Microsoft)
   no se traducen. Siglas asentadas (CPU, PID, DLL, UAC) se quedan.
7. Español neutro y técnico, trato de usted en preguntas al usuario
   (como en la fase 1: «¿Está seguro...?»), sin anglicismos evitables.
