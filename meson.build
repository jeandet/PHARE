project('PHARE', 'cpp',default_options : ['cpp_std=c++17'], license: 'GPL3', version: '0.0.1')

cmake = import('cmake')
opt_var = cmake.subproject_options()
opt_var.add_cmake_defines({'ENABLE_MPI': false, 'ENABLE_HDF5': false})
SAMRAI_proj = cmake.subproject('SAMRAI', options: opt_var)


subdir('src')
