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
                "variables": {
                    "dokan_install_path%": "<!(echo %DOKAN_INSTALL_DIR%)"
                },
                "defines": [
                    ["DOKAN_INSTALL_DIR",
                        "<!(node -e \"process.stdout.write(JSON.stringify(process.argv[1]))\" -- \"<(dokan_install_path)\")"]
                ],
                "include_dirs": [
                    "$(DOKAN_FUSE_INCLUDE)",
                    "$(INCLUDE)"
                ],
                "link_settings": {
                    "libraries": [
                        "<(dokan_install_path)/dokanfuse.lib"
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
