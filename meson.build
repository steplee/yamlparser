project('irn', ['cpp'],
    default_options : [
      'cpp_std=c++17',
      # 'buildtype=debugoptimized',
      'buildtype=debug',
      'optimization=0',
      ])

python3 = import('python').find_installation('python3')
python3_path = python3.path()

# pybind11_config_ret = run_command(python3_path, ['-m', 'pybind11', '--includes'])

# gtest = subproject('gtest', required: true)
# gtest = dependency('gtest', required: true)
# gtest_dep = gtest.get_variable('gtest_dep')
# gtest_main_dep = gtest.get_variable('gtest_main_dep')

yaml_dep = declare_dependency(include_directories: ['.'])

test_app = executable('tests', 'tests.cc', 'compile_implementation.cc',
  dependencies: [
      yaml_dep,
      meson.get_compiler('cpp').find_library('libstdc++fs')
  ])

# Run from the build directory: meson compile createTestCases
run_target('createTestCases',
  # command: [python3_path, '../pysrc/create_tests.py', '-o', meson.build_root()])
  command: [python3_path, '../pysrc/create_tests.py', '-o', '.'])

run_target('runTests',
  command: [test_app, meson.build_root()])
