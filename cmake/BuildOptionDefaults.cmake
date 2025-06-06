# ~~~
# This allows h.nrnversion(6) to print only the configuration differences.
# The <optionname>_DEFAULT values should only be changed in this file
# and not on the command line.
# ~~~
set(NRN_ENABLE_DOCS_DEFAULT OFF)
set(NRN_ENABLE_DOCS_WITH_EXTERNAL_INSTALLATION_DEFAULT OFF)
set(NRN_ENABLE_SHARED_DEFAULT ON)
set(NRN_ENABLE_INTERVIEWS_DEFAULT ON)
set(NRN_ENABLE_MECH_DLL_STYLE_DEFAULT ON)
set(NRN_ENABLE_DISCRETE_EVENT_OBSERVER_DEFAULT ON)
set(NRN_ENABLE_PYTHON_DEFAULT ON)
set(NRN_ENABLE_THREADS_DEFAULT ON)
set(NRN_ENABLE_MPI_DEFAULT ON)
set(NRN_ENABLE_MUSIC_DEFAULT OFF)
set(NRN_ENABLE_RX3D_DEFAULT ON)
set(NRN_ENABLE_NMODL_DEFAULT OFF)
set(NRN_ENABLE_CORENEURON_DEFAULT OFF)
set(NRN_ENABLE_BACKTRACE_DEFAULT OFF)
set(NRN_ENABLE_TESTS_DEFAULT OFF)
set(NRN_ENABLE_MODEL_TESTS_DEFAULT "")
set(NRN_ENABLE_PERFORMANCE_TESTS_DEFAULT ON)
set(NRN_ENABLE_PYTHON_DYNAMIC_DEFAULT OFF)
set(NRN_LINK_AGAINST_PYTHON_DEFAULT ${MINGW})
set(NRN_ENABLE_MPI_DYNAMIC_DEFAULT OFF)
set(NRN_ENABLE_MOD_COMPATIBILITY_DEFAULT OFF)
set(NRN_ENABLE_REL_RPATH_DEFAULT OFF)
set(NRN_AVOID_ABSOLUTE_PATHS_DEFAULT OFF)
set(NRN_NMODL_CXX_FLAGS_DEFAULT "-O0")
set(NRN_SANITIZERS_DEFAULT "")
set(NRN_ENABLE_MATH_OPT_DEFAULT OFF)
set(NRN_ENABLE_DIGEST_DEFAULT OFF)
set(NRN_ENABLE_ARCH_INDEP_EXP_POW_DEFAULT OFF)

# Some distributions may set the prefix. To avoid errors, unset it
set(NRN_PYTHON_DYNAMIC_DEFAULT "")
set(NRN_MPI_DYNAMIC_DEFAULT "")
set(NRN_RX3D_OPT_LEVEL_DEFAULT "0")

# Some CMAKE variables we would like to see, if they differ from the following.
set(CMAKE_BUILD_TYPE_DEFAULT RelWithDebInfo)
set(CMAKE_INSTALL_PREFIX_DEFAULT "/usr/local")
set(CMAKE_C_COMPILER_DEFAULT "gcc")
set(CMAKE_CXX_COMPILER_DEFAULT "g++")
set(PYTHON_EXECUTABLE_DEFAULT "")
set(IV_LIB_DEFAULT "")

# For wheel deployment
set(NRN_BINARY_DIST_BUILD_DEFAULT OFF)
set(NRN_WHEEL_STATIC_READLINE_DEFAULT OFF)

# we add some coreneuron options in order to check support like GPU
set(NRN_OPTION_NAME_LIST
    NRN_ENABLE_SHARED
    NRN_ENABLE_INTERVIEWS
    NRN_ENABLE_MECH_DLL_STYLE
    NRN_ENABLE_DISCRETE_EVENT_OBSERVER
    NRN_ENABLE_PYTHON
    NRN_ENABLE_MUSIC
    NRN_ENABLE_THREADS
    NRN_ENABLE_MPI
    NRN_ENABLE_RX3D
    NRN_ENABLE_CORENEURON
    NRN_ENABLE_TESTS
    NRN_ENABLE_MODEL_TESTS
    NRN_ENABLE_PYTHON_DYNAMIC
    NRN_LINK_AGAINST_PYTHON
    NRN_ENABLE_MPI_DYNAMIC
    NRN_MODULE_INSTALL_OPTIONS
    NRN_PYTHON_DYNAMIC
    NRN_MPI_DYNAMIC
    NRN_RX3D_OPT_LEVEL
    NRN_SANITIZERS
    CMAKE_BUILD_TYPE
    CMAKE_INSTALL_PREFIX
    CMAKE_C_COMPILER
    CMAKE_CXX_COMPILER
    PYTHON_EXECUTABLE
    IV_LIB
    CORENRN_ENABLE_GPU
    CORENRN_ENABLE_SHARED)

# For profiling
set(NRN_ENABLE_PROFILING_DEFAULT OFF)
set(NRN_PROFILER_DEFAULT "caliper")
