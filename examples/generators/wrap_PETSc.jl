#
# Example script to wrap PETSc
#

using Clang.cindex
using Clang.wrap_c

PETSC_INCLUDE = abspath("/usr/include/petsc")
MPI_INCLUDE = "/usr/include/openmpi"
JULIA_ROOT=abspath(JULIA_HOME, "../../")

LLVM_VER = "3.4"
LLVM_BUILD_TYPE = "Release+Asserts"

LLVM_PATH = joinpath(JULIA_ROOT, "deps/llvm-$LLVM_VER")
clanginc_path = joinpath(LLVM_PATH, "build_$LLVM_BUILD_TYPE/tools/clang/include/clang")

petsc_hdrs = [joinpath(PETSC_INCLUDE, "petsc.h")]
                
clang_includes = map(x::ASCIIString->joinpath(LLVM_PATH, x), [
    "build_$LLVM_BUILD_TYPE/tools/clang/include/clang",
    "include/llvm",
    "include"
    ])
push!(clang_includes, PETSC_INCLUDE)
push!(clang_includes, MPI_INCLUDE)
clang_extraargs = ["-v"]
# clang_extraargs = ["-D", "__STDC_LIMIT_MACROS", "-D", "__STDC_CONSTANT_MACROS"]

# Callback to test if a header should actually be wrapped (for exclusion)
function should_wrap(hdr::ASCIIString, name::ASCIIString)
    return beginswith(dirname(hdr), PETSC_INCLUDE)
end

function lib_file(hdr::ASCIIString)
    return "petsc"
end

function output_file(hdr::ASCIIString)
    return "PETSc.jl"
end

function should_wrap_cu(name::ASCIIString, cursor)
    exc = false
    exc |= contains(name, "MPI")
    return !exc
end

const wc = wrap_c.init(; 
                        output_file = "libPETSc_h.jl",
                        common_file = "libPETSc_common.jl",
                        clang_includes = clang_includes,
                        clang_args = clang_extraargs,
                        header_wrapped = should_wrap, 
                        header_library = lib_file,
                        header_outputfile = output_file,
                        cursor_wrapped = should_wrap_cu)
push!(wc.cache_wrapped, "ompi_file_errhandler_fn")
function wrap_libPETSc(wc::WrapContext, wrap_hdrs)
    wrap_c.wrap_c_headers(wc, wrap_hdrs)
end
