set(LLVM_LINK_COMPONENTS
  Option
  Support
  )

add_clang_tool(callreport
  CallReport.cpp

  DEPENDS
  ClangDriverOptions
  )

clang_target_link_libraries(callreport
  PRIVATE
  clangBasic
  clangTooling
  clangASTMatchers
  )

install(TARGETS callreport
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT clang-extras)
