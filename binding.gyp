{
  "targets": [
    {
      "target_name": "system-font-info",
      "dependencies": [
        "<!(node -p \"require('node-addon-api').targets\"):node_addon_api_except_all"
      ],
      "defines": [
        "NAPI_VERSION=8"
      ],
      "sources": [
        "<!@(node scripts/gyp-sources.js src/core src/sfnt)"
      ],
      "cflags_cc": [
        "-std=c++20",
        "-fvisibility=hidden"
      ],
      "conditions": [
        [
          "OS=='mac'",
          {
            "sources": [
              "<!@(node scripts/gyp-sources.js src/platform/darwin)"
            ],
            "link_settings": {
              "libraries": [
                "-framework CoreText",
                "-framework CoreFoundation",
                "-framework CoreGraphics",
                "-framework AppKit",
                "-framework Foundation"
              ]
            },
            "xcode_settings": {
              "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
              "CLANG_ENABLE_OBJC_ARC": "YES",
              "GCC_SYMBOLS_PRIVATE_EXTERN": "YES",
              "MACOSX_DEPLOYMENT_TARGET": "11.0",
              "OTHER_CFLAGS": [
                "-Wall",
                "-Wextra",
                "-Wno-unused-parameter"
              ]
            }
          }
        ],
        [
          "OS=='win'",
          {
            "sources": [
              "<!@(node scripts/gyp-sources.js src/platform/win32)"
            ],
            "libraries": [
              "dwrite.lib"
            ],
            "msvs_settings": {
              "VCCLCompilerTool": {
                "AdditionalOptions": [
                  "/std:c++20",
                  "/utf-8"
                ]
              }
            }
          }
        ],
        [
          "OS=='linux'",
          {
            "sources": [
              "<!@(node scripts/gyp-sources.js src/platform/linux)"
            ]
          }
        ]
      ]
    }
  ]
}
