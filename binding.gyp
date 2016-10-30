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
                "variables": {
                    'dokan__install_dir%': '$(DokanLibrary1)/include/fuse'
                },
                "include_dirs": [
                    "<(dokan__install_dir)",
                    "$(INCLUDE)"
                ],
                "link_settings": {
                    "libraries": [
                        "<(dokan__library)"
                    ]
                },
                "conditions": [
                    ['target_arch=="x64"', {
                        "variables": { 'dokan__library%': '$(DokanLibrary1_LibraryPath_x64)/dokanfuse1' }
                    }, {
                        "variables": { 'dokan__library%': '$(DokanLibrary1_LibraryPath_x86)/dokanfuse1' }
                    }]
                ]
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
