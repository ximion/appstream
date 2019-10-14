# Meson definition for AppStream VAPI

glib.generate_vapi ('appstream',
    sources: [as_gir, 'AppStream-1.0-custom.vala'],
    packages: ['glib-2.0',
               'gio-2.0',
               'gobject-2.0'],
    install: true,
    metadata_dirs: ['.'],
    vapi_dirs: ['.']
)
