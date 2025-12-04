# Openbox YAML Configuration

This fork of Openbox replaces the traditional XML configuration (`rc.xml`, `menu.xml`) with a modern, easier-to-read YAML format (`config.yaml`, `menu.yaml`).

## Configuration File Location

The main configuration file is located at:
`~/.config/openbox/config.yaml`

The menu file is located at:
`~/.config/openbox/menu.yaml`

## Structure

The `config.yaml` file is divided into several sections:

- `theme`
- `desktops`
- `focus`
- `placement`
- `keyboard`
- `mouse`
- `applications` (Window Rules)
- `menu`
- `resistance`
- `resize`
- `margins`
- `dock`

### Theme

Controls window appearance settings.

```yaml
theme:
  name: "Mikachu"
  titleLayout: "NLIMC" # N=Icon, L=Label, I=Iconify, M=Maximize, C=Close
  keepBorder: yes
  animateIconify: yes
  windowListIconSize: 12
  font:
    - place: ActiveWindow
      name: Source Code Pro
      size: 10
      weight: Bold
      slant: Normal
    - place: InactiveWindow
      name: Source Code Pro
      size: 10
```

### Desktops

Manage virtual desktops.

```yaml
desktops:
  number: 4
  names:
    - "Work"
    - "Web"
    - "Media"
    - "Games"
  popupTime: 1000
```

### Focus

Focus behavior.

```yaml
focus:
  followMouse: no
  focusNew: yes
  raiseOnFocus: no
  focusDelay: 200
```

### Placement

Window placement policy.

```yaml
placement:
  policy: Mouse # or 'Smart'
  center: yes
  monitor: Active # or 'Mouse', or a number (0, 1...)
```

### Keyboard (Keybinds)

Define keyboard shortcuts.

```yaml
keyboard:
  chainQuitKey: "C-g"
  keybind:
    - key: "W-Return"
      action:
        - name: Execute
          command: "urxvt"
    - key: "A-F4"
      action:
        - name: Close
    - key: "A-Tab"
      action:
        - name: NextWindow
    - key: "W-Left"
      action:
        - name: GoToDesktop
          to: left
          wrap: no
```

### Mouse

Define mouse bindings and settings.

```yaml
mouse:
  dragThreshold: 1
  doubleClickTime: 500
  context:
    - name: Frame
      mousebind:
        - button: "A-Left"
          action: Press
          do:
            - Focus
            - Raise
        - button: "A-Left"
          action: Drag
          do:
            - Move
    - name: Titlebar
      mousebind:
        - button: "Left"
          action: DoubleClick
          do:
            - ToggleMaximize
```

### Applications (Window Rules)

Apply specific settings to matching windows.

**Note on VTE Terminals / Missing Decorations:**
Some applications (like VTE-based terminals, Chrome/Chromium) may request "no decorations" via system hints (MWM Hints). Openbox generally respects these hints. If your window is missing decorations, check if the application has a setting to "Use System Title Bar" or similar.

You can try to force decorations using the `decor` setting, but note that if an application strictly requests no decorations via MWM hints, Openbox might still honor that request over the configuration in some cases.

```yaml
applications:
  application:
    - class: "Firefox"
      desktop: 2
      maximized: yes
    - name: "urxvt"
      decor: no   # Force disable decorations
    - role: "browser"
      fullscreen: yes
```

Supported rule properties:
- `match_class`, `match_name`, `match_role` (for matching)
- `desktop` (number)
- `maximized` (yes/no)
- `fullscreen` (yes/no)
- `skip_taskbar` (yes/no)
- `skip_pager` (yes/no)
- `follow` (yes/no - focus window when it appears)
- `decor` (yes/no - enable/disable window decorations)

## Menu

Menu settings.

```yaml
menu:
  file: "menu.yaml"
  hideDelay: 200
  middle: no
  submenuShowDelay: 100
  showIcons: yes
```

## Example `menu.yaml`

```yaml
- label: "Terminal"
  action: Execute
  command: "x-terminal-emulator"
- label: "Web Browser"
  action: Execute
  command: "x-www-browser"
- label: "Openbox"
  submenu:
    - label: "Restart"
      action: Restart
    - label: "Exit"
      action: Exit
```
