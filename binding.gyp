{
  "targets": [
    {
      "target_name": "fuse_bindings",
      "sources": [ "fuse-bindings.cc", "abstractions.cc" ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "<!@(pkg-config fuse --cflags-only-I | sed s/-I//g)"
      ],
      "link_settings": {
        "libraries": [
          "<!@(pkg-config --libs-only-L --libs-only-l fuse)"
        ]
      }
    }
  ]
}
