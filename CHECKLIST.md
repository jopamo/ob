# Project Migration Checklist

- [ ] Remove remaining XML parsing in complex actions (`if`, `cyclewindows`, `directionalwindows`)
- [ ] Update documentation/manpages to describe YAML configuration
- [x] Drop libxml2 dependency from build once XML paths are gone
- [ ] Add regression tests for YAML config loading and action option parsing
- [ ] Provide sample `config.yaml` and `menu.yaml` templates in `data/`
