---
name: plan-milestone
description: Usar para crear un plan nuevo de hito o tarea (/plan-milestone). Genera .claude/plans/<Hx>-<nombre>.md desde TEMPLATE.md; se niega a cerrarlo sin oráculo ejecutable.
---
1. Copia `.claude/plans/TEMPLATE.md` a `.claude/plans/<Hx>-<nombre>.md`.
2. Rellena todas las secciones: objetivo de una frase, pasos pequeños (archivos permitidos + comando de verificación), riesgos, escalado.
3. El oráculo debe ser un comando ejecutable con criterio numérico. Si no existe (p. ej. H6), NO cierres el plan: lista qué falta para definirlo y márcalo "abierto".
   - H6 (flujo de juego): candidato decidido por el usuario = prueba de humo sin interfaz (cargar nivel, recorrer con el autopiloto, comprobar la transición de estados hasta la meta). Concretarlo al llegar a H6, no antes.
4. Marca como "propuesta" cualquier umbral no contrastado con `docs/formats/`.
