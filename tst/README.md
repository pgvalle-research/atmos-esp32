Assumes this directory structure:

```
.
├── atmos
│   ├── atmos
│   └── tst     <-- we are here
│   └── f-streams
```

Default:

```bash
LUA_PATH="../../f-streams/?/init.lua;../?.lua;../?/init.lua;;" lua all.lua
```

Worktree:

```bash
LUA_PATH="../../../../f-streams/?/init.lua;../?.lua;../?/init.lua;;" lua all.lua
```

Absolute:

```bash
LUA_PATH="/XXX/f-streams/?/init.lua;/XXX/atmos/XXX/?.lua;/XXX/atmos/XXX/?/init.lua;;" lua all.lua
```
