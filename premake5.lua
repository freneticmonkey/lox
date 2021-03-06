
workspace "lox"
configurations { 
   "Debug", 
   "Release" 
}
platforms {
   "windows",
   "macosx",
   "macosxARM",
   "linux64",
   "linuxARM64",
   -- etc.
}

-- enable tracing for debug builds
filter "configurations:Debug"
   symbols "On"
   defines { 
      "_DEBUG",
      "TRACY_ENABLE"
   }

filter "configurations:Release"
   optimize "On"
   defines { "NDEBUG" }

filter {}

-- Architecture is x86_64 by default
architecture "x86_64"

filter "system:windows"
   defines {
      "LOX_WINDOWS",
      "WIN32",
      "_WIN32"
   }

filter "system:macosx"
   defines {
      "LOX_MACOS"
   }

filter "platforms:macosxARM or linuxARM64"
   architecture "ARM"

filter "system:linux"
   buildoptions { 
      "-std=c11",
      "-D_DEFAULT_SOURCE",
      "-D_POSIX_C_SOURCE=200112L",
      "-mcmodel=large",
      "-fPIE",
      "-Werror=return-type",
      "-Werror=implicit-function-declaration"
   }
   defines {
      "LOX_LINUX"
   }

solution "lox"
   filename "lox"
   location "projects"

   startproject "lox"

project "lox"
   kind "ConsoleApp"
   language "C"
   targetdir( "build" )
   defines { 
   }
   flags {
      "MultiProcessorCompile"
   }
   debugdir "."

   filter {}
      links {
      }

      libdirs {
         "build"
      }

      sysincludedirs {
      }

      includedirs {
         "src"
      }

      files { 
         "src/**.h",
         "src/**.c"
      }

      -- ignore all testing files
      removefiles {
         "src/test/**",
         "src/**_test.*"
      }

   -- enable tracing for debug builds
   filter "configurations:Debug"
      links {
        --  "tracy"
      }
      sysincludedirs {
        --  "ext/tracy"
      }
   
   filter { "system:linux"}
      libdirs {
         os.findlib("m"),
         os.findlib("c")
      }
      links {
         "c",
         "dl",
         "m",
         "pthread",
      }

   filter { "system:windows" }
      defines {
         "_CRT_SECURE_NO_WARNINGS"
      }
      disablewarnings { 
         "4005"
      }
      -- Turn off edit and continue
      editAndContinue "Off"

   filter { "system:macosx"}
      links {
         "Cocoa.framework",
         "IOKit.framework",
         "c",
        --  "tracy",
      }

    --   filter "files:src/main.c"
    --      compileas "Objective-C"

project "test"
    kind "ConsoleApp"
    language "C"
    targetdir( "build" )
    defines { 
       "LOX_UNIT"
    }
 
    links {
       "munit"
    }
 
    libdirs {
       "build"
    }
 
    sysincludedirs {
       "ext"
    }
 
    includedirs { 
       "src"
    }
 
    files { 
       "src/**.h",
       "src/**.c",
       
    }

    -- ignore the lox main
    removefiles {
        "src/main.c"
     }
 
    filter { "system:macosx"}
       links {
          "c"
       }
    
    filter { "system:linux"}
       libdirs {
          os.findlib("m"),
          os.findlib("c")
       }
       links {
          "c",
          "m",
          "pthread",
       }

-- External Libraries

project "munit"
   kind "StaticLib"
   language "C"
   targetdir( "build" )

   files { 
      "ext/munit/**.h",
      "ext/munit/**.c"
   }