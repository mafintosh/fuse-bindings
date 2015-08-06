{
    "targets": [{
        "target_name": "fuse_bindings",
        "sources": ["fuse-bindings.cc", "abstractions.cc"],
        "conditions": [
            ['OS!="win"', {
                "include_dirs": [
                    "<!(node -e \"require('nan')\")",
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
                    "<!(node -e \"require('nan')\")",
                    "D:\\Git-Repos\\dokany\\dokan_fuse\\include"
                ],
                "link_settings": {
                    "libraries": [
                        "C:\\Program Files\\Dokan\\DokanLibrary\\dokanfuse.lib"
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
