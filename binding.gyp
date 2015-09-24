{
    "targets": [{
        "target_name": "fuse_bindings",
        "sources": ["fuse-bindings.cc", "abstractions.cc"],
        "include_dirs": [
            "<!(node -e \"require('nan')\")"
        ],
        "conditions": [
            ['OS!="win"', {
                'variables':
                {
                    'fuse_includes%' : '<!(pkg-config fuse --cflags-only-I | sed s/-I//g)',
                    'fuse_libraries%': '',
                },
                "include_dirs": [
                    "<@(fuse_includes)"
                ],
                'library_dirs': [
                  '<@(fuse_libraries)',
                ],
                "link_settings": {
                    "libraries": [
                        "-lfuse"
                    ]
                }
            }],
            ['OS=="win"', {
                "include_dirs": [
                    "$(DOKAN_FUSE_INCLUDE)",
                    "$(INCLUDE)"
                ],
                "link_settings": {
                    "libraries": [
                        "<!(echo %DOKAN_INSTALL_DIR%)/dokanfuse.lib"
                    ]
                }
            }]
        ],
        "configurations": {
            "Debug": {
                "msvs_settings": {
                    "VCCLCompilerTool": {
                        "RuntimeLibrary": 2
                    }
                }
            },
            "Release": {
                "msvs_settings": {
                    "VCCLCompilerTool": {
                        "RuntimeLibrary": 2
                    }
                }
            }
        }
    }]
}
