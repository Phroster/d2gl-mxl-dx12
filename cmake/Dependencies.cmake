include(FetchContent)
# The same compiler revisions used by the accepted DX12 release.
FetchContent_Declare(mxl_glslang
  GIT_REPOSITORY https://github.com/DiligentGraphics/glslang.git
  GIT_TAG 275822a6261ee689aadb1da5f09a0ec2f058685c
  GIT_SUBMODULES ""
)
FetchContent_Declare(mxl_spirv_cross
  GIT_REPOSITORY https://github.com/DiligentGraphics/SPIRV-Cross.git
  GIT_TAG 1a6169566c73d3da552748fc372fe2bbb856e46e
  GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(mxl_glslang mxl_spirv_cross)
configure_file("${CMAKE_CURRENT_LIST_DIR}/build-dependencies.json.in"
  "${CMAKE_BINARY_DIR}/mxl-build-dependencies.json" @ONLY)
