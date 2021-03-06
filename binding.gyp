#for node-gyp info see https://gyp.gsrc.io/docs/UserDocumentation.md
#NOTE: after changing this file, delete build folder to remove cached info
#NOTE: cflags needs node.gyp ver >= 3.6.2; see https://gist.github.com/TooTallNate/1590684
#NOTE: nvm uses an internal copy of node.gyp; https://github.com/nodejs/node-gyp/wiki/Updating-npm%27s-bundled-node-gyp
#to upgrade node.gyp:
#  [sudo] npm explore npm -g -- npm install node-gyp@latest
{
    'targets':
    [
        {
            'target_name': "gpu-canvas",
#            'type': 'executable',
            'sources': [ "src/GpuCanvas.cpp"],
#CAUTION: cflags requires node.gyp ver >= 3.6.2
#NOTE: node-gyp only reads *one* "cflags_cc+" here, otherwise node-gyp ignores it
            'cflags_cc': ["-w", "-Wall", "-pedantic", "-Wvariadic-macros", "-g", "-std=c++11"], #-std=c++0x
            'include_dirs': ["<!(node -e \"require('nan')\")"],
            'include_dirs+': [" <!(sdl2-config --cflags)"], #CAUTION: need "+" and leading space here
#            'libraries': ["-lGLESv2", "-lEGL", "-lm"],
#            'OTHER_LDFLAGS': ['-stdlib=libc++'],
#            'dependencies': [ 'deps/mpg123/mpg123.gyp:output' ],
#            "dependencies": [ "<!(node -p \"require('node-addon-api').gyp\")" ],
            'libraries': [" <!(sdl2-config --libs)"],
            'conditions':
            [
                [
#                    'OS=="linux-rpi"',
#                    '<!@(uname -p)=="armv7l"', #RPi 2
                    'target_arch=="arm"', #RPi 2
                    {
                        'defines': ["RPI_NO_X"] #don't want X Windows client
#                        'libraries+': ["-L/opt/vc/lib", "-lbcm_host"],
#                        'include_dirs+':
#                        [
#                            "/opt/vc/include",
#                            "/opt/vc/include/interface/vcos/pthreads",
#                            "/opt/vc/include/interface/vmcs_host/linux"
#                        ]
                    },
#                    'OS=="linux-pc"', #else
                    {
#                        "xcode_settings": {
#                        'defines': ["UNIV_LEN=32"], #for dev/testing
#                        'defines': ["SHADER_DEBUG"], #for dev/testing
                        'libraries': ["-lX11", "-lXxf86vm"] #, "-lXext", "-lXxF86vm"]
                    }
                ]
            ]
        }
    ]
}
#eof
