# Configuration System Migration (XML to YAML)

## Phase 1: Config Story & File Layout
- [x] Define YAML layout (canonical config)
- [x] Plan XDG config path (`$XDG_CONFIG_HOME/openbox/config.yaml`)
- [x] Plan menu config path (`$XDG_CONFIG_HOME/openbox/menu.yaml` - optional)

## Phase 2: Internal Config Model
- [x] Create `ob_config.h`
- [x] Create `ob_config.c`
- [x] Define `struct ob_config` and sub-structs (`ob_theme_config`, `ob_desktops_config`, etc.)
- [x] Implement `ob_config_init_defaults`
- [x] Implement `ob_config_free`
- [x] Implement `ob_config_validate`

## Phase 3: Refactor Runtime to be Format Neutral
- [x] Refactor Desktops subsystem (`ob_desktops_apply`)
- [x] Refactor Theme subsystem (`ob_theme_apply`)
- [x] Refactor Focus/Placement subsystem (`ob_focus_apply`, `ob_placement_apply`)
- [x] Refactor Keybindings subsystem (`ob_keyboard_apply`)
- [x] Refactor Mouse subsystem (`ob_mouse_apply`)
- [x] Refactor Window Rules subsystem (`ob_rules_apply`)
- [x] Refactor Menu subsystem (`ob_menu_apply`)
- [x] Update startup flow to load config into struct and then apply

## Phase 4: YAML Loader Implementation
- [x] Choose YAML library (e.g., `libyaml`)
- [x] Implement `ob_config_load_yaml`
- [x] Implement parsing logic for all config sections
- [x] Implement basic type and enum validation during parsing

## Phase 5: Strip XML from the Project
- [x] Delete XML parsing files (Progress: actions now parse YAML options; config/menu XML still present)
- [x] Remove libxml2 from build system (Blocked until config/menu XML gone)
- [x] Remove DTD and XML schema files
- [ ] Update documentation/man pages to refer to YAML (Progress: openbox(1) updated)
- [ ] Update logging messages

## Phase 6: Expose Tooling Modes
- [x] Implement `--check-config`
- [x] Implement `--dump-config`
- [ ] Implement `--print-schema` (Optional)

## Phase 7: External XML -> YAML Migration Helper (Optional)
- [ ] Create separate repo/tool for migration (Python/C)
