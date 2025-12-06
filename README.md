# Openbox

This is the Openbox window manager project.

## YAML Configuration Support

Openbox now supports configuration files written in YAML, in addition to the traditional XML format. If YAML support was enabled during compilation (checked via `pkg-config --exists yaml-0.1`), Openbox will prefer `rc.yaml` and `menu.yaml` over their `.xml` counterparts (`rc.xml`, `menu.xml`) if they exist in your configuration directory (e.g., `~/.config/openbox/`).

### Converting Existing XML Configurations to YAML

A conversion tool, `openbox-xml-to-yaml`, is installed with Openbox (typically in `/usr/bin/` or `/usr/local/bin/`). You can use this tool to convert your existing XML configuration files to the new YAML format.

**Usage:**

```bash
openbox-xml-to-yaml input.xml output.yaml
```

**Example:**

To convert your `rc.xml` to `rc.yaml`:

```bash
openbox-xml-to-yaml ~/.config/openbox/rc.xml ~/.config/openbox/rc.yaml
```

Similarly for `menu.xml`:

```bash
openbox-xml-to-yaml ~/.config/openbox/menu.xml ~/.config/openbox/menu.yaml
```

After conversion, Openbox will automatically detect and use the `.yaml` files. If you encounter any issues with the generated YAML, you can always revert to your `.xml` files.
