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
                    'fuse__include_dirs%': '<!(pkg-config fuse --cflags-only-I | sed s/-I//g)',
                    'fuse__library_dirs%': '',
                    'fuse__libraries%': '<!(pkg-config --libs-only-L --libs-only-l fuse)'
                },
                "include_dirs": [
                    "<@(fuse__include_dirs)"
                ],
                'library_dirs': [
                  '<@(fuse__library_dirs)',
                ],
                "link_settings": {
                    "libraries": [
                        "<@(fuse__libraries)"
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
