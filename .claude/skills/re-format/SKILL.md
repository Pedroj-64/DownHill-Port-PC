---
name: re-format
description: Usar para investigar un formato binario del juego o validar una hipótesis de formato (/re-format). Procedimiento hipótesis, script, validación contra los 54 niveles, doc es/en.
---
1. Hipótesis: invoca `re-analyst`; exige tabla offset/tamaño/tipo/confianza y marca lo desconocido.
2. Script: escribe un script de validación en `tools/` (stdlib, sin datos del juego dentro).
3. Validación: ejecútalo contra los 54 niveles; solo con resultado limpio deja de ser hipótesis.
4. Doc: `doc-writer` crea/actualiza el doc en `docs/formats/` en es/en citando la dirección `FUN_xxxxxxxx`; actualiza `docs/formats/INDEX.md` si es un formato nuevo.
