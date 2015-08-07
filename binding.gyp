{
    "targets": [{
        "target_name": "fuse_bindings",
        "sources": ["fuse-bindings.cc", "abstractions.cc"],
        "include_dirs": [
            "<!(node -e \"require('nan')\")"
        ],
        "conditions": [
            ['OS!="win"', {
                "include_dirs": [
                    "<!@(pkg-config fuse --cflags-only-I | sed s/-I//g)"
                ],
                "link_settings": {
                    "libraries": [
                        "<!@(pkg-config --libs-only-L --libs-only-l fuse)"
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
