<!-- SPDX-FileCopyrightText: 2026 Pedro Soto -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Contributing / Contribuir

🇬🇧 [English](#english) · 🇪🇸 [Español](#español)

## English

Thanks for helping! This is a community project; every contribution is welcome.

### Hard rules (non-negotiable)
1. **Never commit game data**: no ISO, executable, textures, models, audio, video, save files, Ghidra projects or decompiler output. Not even small samples.
2. **No copied code.** Describe formats and behaviour in your own words in `docs/`. Do not paste decompiled functions.
3. **No personal data or secrets** in commits (paths, tokens, emails in files).

### Setup
```sh
git config core.hooksPath .githooks   # local guard: blocks the mistakes above before they reach history
tools/guard.sh                        # same checks over the whole tree (CI runs this too)
```

### Tips for a clean history
- Use your GitHub `noreply` address for commits if you don't want your email public: `git config user.email "ID+user@users.noreply.github.com"`.
- Keep pull requests small and focused; mention which game release (PAL/NTSC) you tested.
- Documentation lives in two languages (`docs/en/` and `docs/es/`). Update both if you can; if you only speak one, say so in the PR and someone will help translate.

### License
By contributing you agree your work is licensed under GPL-3.0-or-later.

## Español

¡Gracias por ayudar! Este es un proyecto comunitario; toda contribución es bienvenida.

### Reglas firmes (no negociables)
1. **Nunca subas datos del juego**: ninguna ISO, ejecutable, textura, modelo, audio, video, partida guardada, proyecto de Ghidra ni salida del decompilador. Ni siquiera muestras pequeñas.
2. **Nada de código copiado.** Describe formatos y comportamiento con tus propias palabras en `docs/`. No pegues funciones decompiladas.
3. **Sin datos personales ni secretos** en los commits (rutas, tokens, correos dentro de archivos).

### Configuración
```sh
git config core.hooksPath .githooks   # guarda local: bloquea los errores anteriores antes de que lleguen al historial
tools/guard.sh                        # las mismas comprobaciones sobre todo el árbol (el CI también las ejecuta)
```

### Consejos para un historial limpio
- Usa tu correo `noreply` de GitHub en los commits si no quieres publicar tu correo: `git config user.email "ID+usuario@users.noreply.github.com"`.
- Haz pull requests pequeños y enfocados; indica con qué versión del juego (PAL/NTSC) probaste.
- La documentación está en dos idiomas (`docs/en/` y `docs/es/`). Actualiza ambos si puedes; si sólo hablas uno, indícalo en el PR y alguien ayudará con la traducción.

### Licencia
Al contribuir aceptas que tu trabajo se licencie bajo GPL-3.0-or-later.
