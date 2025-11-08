doc_conf = configuration_data()
doc_conf.set('configdir', configdir_abs)

man_templates = [
  ['openbox.1.in', 'openbox.1'],
  ['openbox-session.1.in', 'openbox-session.1'],
  ['openbox-gnome-session.1.in', 'openbox-gnome-session.1'],
  ['openbox-kde-session.1.in', 'openbox-kde-session.1'],
  ['obxprop.1.in', 'obxprop.1'],
]

generated_man = []
foreach tmpl : man_templates
  generated_man += configure_file(
    input: tmpl[0],
    output: tmpl[1],
    configuration: doc_conf)
endforeach

install_man(generated_man)
