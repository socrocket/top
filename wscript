#! /usr/bin/env python
# -*- coding: utf-8 -*-
# vim: set expandtab:ts=4:sw=4:setfiletype python
#*********************************************************************
# Copyright 2010, Institute of Computer and Network Engineering,
#                 TU-Braunschweig
# All rights reserved
# Any reproduction, use, distribution or disclosure of this program,
# without the express, prior written consent of the authors is 
# strictly prohibited.
#
# University of Technology Braunschweig
# Institute of Computer and Network Engineering
# Hans-Sommer-Str. 66
# 38118 Braunschweig, Germany
#
# ESA SPECIAL LICENSE
#
# This program may be freely used, copied, modified, and redistributed
# by the European Space Agency for the Agency's own requirements.
#
# The program is provided "as is", there is no warranty that
# the program is correct or suitable for any purpose,
# neither implicit nor explicit. The program and the information in it
# contained do not necessarily reflect the policy of the European 
# Space Agency or of TU-Braunschweig.
#*********************************************************************
# Title:      wscript
#
# ScssId:
#
# Origin:     HW-SW SystemC Co-Simulation SoC Validation Platform
#
# Purpose:    this file contains the build system for the TLM library
#
# Method:     $ ./waf configure; ./waf # to build it
#
# Modified on $Date$
#          at $Revision$
#          by $Author$
#
# Principal:  European Space Agency
# Author:     VLSI working group @ IDA @ TUBS
# Maintainer: Rolf Meyer
# Reviewed:
#*********************************************************************
APPNAME = 'SoCRocket'
VERSION = '$Revision$'
top = '.'
out = 'build'

import os, sys

def build(self):
    from tools.waf.logger import Logger
    logger = Logger("%s/build.log" % self.out_dir)
    sys.stdout = logger
    sys.stderr = logger
    
    from waflib.Tools import waf_unit_test  
    from tools.waf.common import get_subdirs
    self.recurse(get_subdirs())

    self.install_files('${PREFIX}/include', self.path.ant_glob(['**/*.h', '**/*.tpp'], excl=['**/signalkit/**', '**/tests/**', '**/extern/**', '**/contrib/**', '**/platform/**', '**/software/**', '**/.svn/**', '**/.git/**']))
    self.install_files('${PREFIX}/', ['waf', 'wscript', 'platforms/wscript'], relative_trick=True)
    self.install_files('${PREFIX}/', self.path.ant_glob('tools/**', excl=['**/*.pyc', '**/.svn/**', '**/.git/**']), relative_trick=True)
    self.install_files('${PREFIX}/', self.path.ant_glob('templates/**', excl=['**/*~', '**/.svn/**', '**/.git/**']), relative_trick=True)
    self.install_files('${PREFIX}/include', self.path.ant_glob('contrib/grambasockets/*.h', excl=['**/*~', '**/.svn/**', '**/.git/**']))
    self.add_post_fun(waf_unit_test.summary)
    #self.add_post_fun(lcov_summary)

def setprops(self):
    """Set SVN Properties on all source files"""
    from tools.waf.common import setprops
    setprops();

from waflib.Build import BuildContext
def docs(bld):
  """build source code documentation in 'build/docs' if doxygen is installed"""
  if bld.env.DOXYGEN and bld.env.DOXYGEN != "":
    import subprocess
    subprocess.call(["doxygen", "Doxyfile"]) 
  else:
    print "To use the ./waf docs comand please install doxygen"
    sys.exit(0)

class Docs(BuildContext):
    cmd = 'docs'
    fun = 'docs'

def coverage(self):
    """If configured with -G and lcov and gcof are installed it generates a code coverage report"""
    from subprocess import call, STDOUT
    if self.env["gcov"] and self.env["gcov"] != "" and self.env["lcov"] and self.env["lcov"] != "":
        print call([self.env['lcov'], '-b', '.', '-t', 'SoCRocket', '-o', 'lcov_all.info', '-d', 'models', '-d', '../models', '-c'], shell=False, cwd=out, stderr=STDOUT)
        print call(["%s -r lcov_all.info 'amba*' 'ahbin*' 'ahbout*' 'ahbprof*' 'apbuart*' 'c++*' 'green*' 'boost*' 'TLM*' 'sysc*' 'test*' 'extern*' 'tools*' 'utils*' 'usr*' > lcov.info" % self.env['lcov']], shell=True, cwd=out, stderr=STDOUT)
        if self.env['genhtml'] and self.env['genhtml'] != "":
            print call([self.env['genhtml'], '-s', '--demangle-cpp', '-o', 'coverage', 'lcov.info'], shell=False, cwd=out, stderr=STDOUT)
            print "Code coverage report generated: %s/build/coverage/index.html" % (self.path.abspath())
    else:
      print "To use the coverage pleas install gcov and lcov and configure the library with -G"
      sys.exit(0)


class Coverage(BuildContext):
    cmd = 'coverage'
    fun = 'coverage'

def generate(bld):
  """If PyQt4 is installed opens a Wizard to configure a platform"""
  from tools.generator.wizard import main
  main(bld.options.template, bld.options.configuration)

class Generate(BuildContext):
    cmd = 'generate'
    fun = 'generate'

def macclean(self):
  """Clean garbage files from the source tree"""
  from subprocess import call, STDOUT
  print call(['find', '.', '(', '-name', '*.DS_Store', '-o', '-name', '*~', '-o', '-name', '.*~', ')', '-print', '-delete'], shell=False, stderr=STDOUT)

class Macclean(BuildContext):
    cmd = 'macclean'
    fun = 'macclean'

def check_trap_linking(ctx, libName, libPaths, symbol):
    for libpath in libPaths:
        libFile = os.path.join(libpath, ctx.env['cxxshlib_PATTERN'].split('%s')[0] + libName + ctx.env['cxxshlib_PATTERN'].split('%s')[1])
        if os.path.exists(libFile):
            libDump = os.popen(ctx.env.NM + ' -r ' + libFile).readlines()
            for line in libDump:
                if line.find(symbol) != -1:
                    return True
            break
        libFile = os.path.join(libpath, ctx.env['cxxstlib_PATTERN'].split('%s')[0] + libName + ctx.env['cxxstlib_PATTERN'].split('%s')[1])
        if os.path.exists(libFile):
            libDump = os.popen(ctx.env.NM + ' -r ' + libFile).readlines()
            for line in libDump:
                if line.find(symbol) != -1:
                    return True
            break
    return False


def configure(ctx):
    import os.path
    import glob
    
    #############################################################
    # Check for doxygen
    #############################################################
    ctx.find_program('doxygen', var='DOXYGEN', mandatory=False, okmsg="ok")
    #ctx.check_tool('doxygen', tooldir='tools/waf')
 
    ctx.find_program('nm', mandatory=1, var='NM')
    #############################################################
    # Small hack to adjust common usage of CPPFLAGS
    #############################################################
    for flag in ctx.env['CPPFLAGS']:
        if flag.startswith('-D'):
            ctx.env.append_unique('DEFINES', flag[2:])

    # Check for standard tools
    ctx.check_waf_version(mini='1.6.0')

    # Check for standard tools
    ctx.load('compiler_cc')
    ctx.load('compiler_cxx')
    if ctx.env.CC_VERSION:
        if int(ctx.env.CC_VERSION[0]) > 3:
            ctx.msg('Checking for compiler version', 'ok - ' + '.'.join(ctx.env.CC_VERSION))
        else:
            ctx.fatal('Compiler Version' + '.'.join(ctx.env.CC_VERSION) + ' too old: at least version 4.x required')

    sparc_env = ctx.env.copy()
    # Check for python
    ctx.load('python')
    ctx.check_python_version((2,4,0))
    ctx.check_python_module('PyQt4')

    if ctx.options.static_build:
        ctx.env['SHLIB_MARKER'] = ''
        ctx.env['STLIB_MARKER'] = '-static'


    ##############################################################
    # Since I want to build fast simulators, if the user didn't
    # specify any flags I set optimized flags
    #############################################################
    if not ctx.env['CXXFLAGS'] and not ctx.env['CCFLAGS']:
        testFlags = ['-O1', '-march=native', '-pipe', '-finline-functions', '-ftracer', '-fomit-frame-pointer', '-fpermissive']
        if ctx.check_cxx(cxxflags=testFlags, msg='Checking for g++ optimization flags', mandatory=False) and ctx.check_cc(cflags=testFlags, msg='Checking for gcc optimization flags'):
            ctx.env.append_unique('CXXFLAGS', testFlags)
            ctx.env.append_unique('CCFLAGS', testFlags)
            if not ctx.options.debug:
              ctx.env.append_unique('DEFINES', 'NDEBUG')
        else:
            testFlags = ['-O1', '-pipe', '-finline-functions', '-fomit-frame-pointer', '-fpermissive']
            if ctx.check_cxx(cxxflags=testFlags, msg='Checking for g++ optimization flags') and ctx.check_cc(cflags=testFlags, msg='Checking for gcc optimization flags'):
                ctx.env.append_unique('CXXFLAGS', testFlags)
                ctx.env.append_unique('CCFLAGS', testFlags)
                if not ctx.options.debug:
                    ctx.env.append_unique('DEFINES', 'NDEBUG')

    if ctx.env['CFLAGS']:
        ctx.check_cc(cflags=ctx.env['CFLAGS'], mandatory=True, msg='Checking for C compilation flags')
    if ctx.env['CCFLAGS'] and ctx.env['CCFLAGS'] != ctx.env['CFLAGS']:
        ctx.check_cc(cflags=ctx.env['CCFLAGS'], mandatory=True, msg='Checking for C compilation flags')
    if ctx.env['CXXFLAGS']:
        ctx.check_cxx(cxxflags=ctx.env['CXXFLAGS'], mandatory=True, msg='Checking for C++ compilation flags')
    if ctx.env['LINKFLAGS']:
        ctx.check_cxx(linkflags=ctx.env['LINKFLAGS'], mandatory=True, msg='Checking for link flags')

    #############################################################
    # Check support for debugging
    #############################################################
    if ctx.options.debug:
        if not '-g' in ctx.env['CCFLAGS']:
            ctx.env.append_unique('CCFLAGS', '-g')
        if not '-Wall' in ctx.env['CCFLAGS']:
            ctx.env.append_unique('CCFLAGS', '-Wall')
        if not '-g' in ctx.env['CXXFLAGS']:
            ctx.env.append_unique('CXXFLAGS', '-g')
        if not '-Wall' in ctx.env['CXXFLAGS']:
            ctx.env.append_unique('CXXFLAGS', '-Wall')
        if not ctx.options.verbosity:
            ctx.env.append_unique('DEFINES', 'GLOBALVERBOSITY=4')

    #############################################################
    # Check support for profilers
    #############################################################
    if ctx.options.enable_gprof and ctx.options.enable_vprof:
        ctx.fatal('Only one profiler among gprof and vprof can be enabled at the same time')
    if ctx.options.enable_gcov:
        ctx.find_program('gcov', var='gcov', mandatory=False, errmsg='gcov not found! Post execution preperation is disabled.')
        ctx.find_program('lcov', var='lcov', mandatory=False, errmsg='lcov not found! Post execution summeration is disabled.')
        ctx.find_program('genhtml', var='genhtml', mandatory=False)
        ctx.env.append_unique('CCFLAGS', '-fprofile-arcs')
        ctx.env.append_unique('CCFLAGS', '-ftest-coverage')
        ctx.env.append_unique('CCFLAGS', '--coverage')
        ctx.env.append_unique('CXXFLAGS', '-fprofile-arcs')
        ctx.env.append_unique('CXXFLAGS', '-ftest-coverage')
        ctx.env.append_unique('CXXFLAGS', '--coverage')
        ctx.env.append_unique('LINKFLAGS', '-fprofile-arcs')
        if '-O2' in ctx.env['CCFLAGS']:
            ctx.env['CCFLAGS'].remove('-O2')
        if '-O3' in ctx.env['CCFLAGS']:
            ctx.env['CCFLAGS'].remove('-O3')
        if '-O2' in ctx.env['CXXFLAGS']:
            ctx.env['CXXFLAGS'].remove('-O2')
        if '-O3' in ctx.env['CXXFLAGS']:
            ctx.env['CXXFLAGS'].remove('-O3')
        ctx.options.enable_gprof = True
    if ctx.options.enable_gprof:
        if not '-g' in ctx.env['CCFLAGS']:
            ctx.env.append_unique('CCFLAGS', '-g')
        if not '-g' in ctx.env['CXXFLAGS']:
            ctx.env.append_unique('CXXFLAGS', '-g')
        if '-fomit-frame-pointer' in ctx.env['CCFLAGS']:
            ctx.env['CCFLAGS'].remove('-fomit-frame-pointer')
        if '-fomit-frame-pointer' in ctx.env['CXXFLAGS']:
            ctx.env['CXXFLAGS'].remove('-fomit-frame-pointer')
        ctx.env.append_unique('CCFLAGS', '-pg')
        ctx.env.append_unique('CXXFLAGS', '-pg')
        ctx.env.append_unique('LINKFLAGS', '-pg')
        ctx.env.append_unique('STLINKFLAGS', '-pg')
    if ctx.options.enable_vprof:
        if not '-g' in ctx.env['CCFLAGS']:
            ctx.env.append_unique('CCFLAGS', '-g')
        if not '-g' in ctx.env['CXXFLAGS']:
            ctx.env.append_unique('CXXFLAGS', '-g')
        # I have to check for the vprof and papi libraries and for the
        # vmonauto_gcc.o object file
        vmonautoPath = ''
        if not ctx.options.vprofdir:
            ctx.check_cxx(lib='vmon', uselib_store='VPROF', mandatory=True)
            for directory in searchDirs:
                if 'vmonauto_gcc.o' in os.listdir(directory):
                    vmonautoPath = os.path.abspath(os.path.expanduser(os.path.expandvars(directory)))
                    break;
        else:
            ctx.check_cxx(lib='vmon', uselib_store='VPROF', mandatory=True, libpath = os.path.abspath(os.path.expanduser(os.path.expandvars(ctx.options.vprofdir))))
            ctx.env.append_unique('RPATH', ctx.env['LIBPATH_VPROF'])
            ctx.env.append_unique('LIBPATH', ctx.env['LIBPATH_VPROF'])
            vmonautoPath = ctx.env['LIBPATH_VPROF'][0]
        ctx.env.append_unique('LIB', 'vmon')

        if not ctx.options.papidir:
            ctx.check_cxx(lib='papi', uselib_store='PAPI', mandatory=True)
        else:
            ctx.check_cxx(lib='papi', uselib_store='PAPI', mandatory=True, libpath = os.path.abspath(os.path.expanduser(os.path.expandvars(ctx.options.papidir))))
            ctx.env.append_unique('RPATH', ctx.env['LIBPATH_PAPI'])
            ctx.env.append_unique('LIBPATH', ctx.env['LIBPATH_PAPI'])
        ctx.env.append_unique('LIB', 'papi')

        # now I check for the vmonauto_gcc.o object file
        taskEnv = ctx.env.copy()
        taskEnv.append_unique('LINKFLAGS', os.path.join(vmonautoPath, 'vmonauto_gcc.o'))
        ctx.check_cxx(fragment='int main(){return 0;}', uselib='VPROF', mandatory=True, env=taskEnv)
        ctx.env.append_unique('LINKFLAGS', os.path.join(vmonautoPath, 'vmonauto_gcc.o'))
    
    ########################################
    # Setting the host endianess
    ########################################
    if sys.byteorder == "little":
        ctx.env.append_unique('DEFINES', 'LITTLE_ENDIAN_BO')
        ctx.msg('Checking for host endianness', 'little')
    else:
        ctx.env.append_unique('DEFINES', 'BIG_ENDIAN_BO')
        ctx.msg('Checking for host endianness', 'big')
    ########################################
    # Check for boost libraries
    ########################################
    ctx.load('boost')
    # Try to load options from env if not given
    if not ctx.options.boost_includes or ctx.options.boost_includes == "":
      ctx.options.boost_includes = os.environ.get("BOOST_DIR",None)
    if not ctx.options.boost_libs or ctx.options.boost_libs == "":
      ctx.options.boost_libs = os.environ.get("BOOST_LIB",None)

    boostLibs = 'thread regex date_time program_options filesystem unit_test_framework system'
    boostErrorMessage = 'Unable to find ' + boostLibs + ' boost libraries of at least version 1.35, please install them and/or specify their location with the --boost-includes and --boost-libs configuration options. It can also happen that you have more than one boost version installed in a system-wide location: in this case remove the unnecessary versions.'
    
    boostLibs = 'thread regex date_time program_options filesystem system'

    ctx.check_boost(lib=boostLibs, static=ctx.options.static_build, mandatory=True, errmsg = boostErrorMessage)
    if int(ctx.env.BOOST_VERSION.split('_')[1]) < 35:
        ctx.fatal(boostErrorMessage)
    if not ctx.options.static_build:
        ctx.env.append_unique('RPATH', ctx.env['LIBPATH_BOOST_THREAD'])

    #######################################################
    # Determining gcc search dirs
    #######################################################
    compilerExecutable = ''
    if len(ctx.env['CXX']):
        compilerExecutable = ctx.env['CXX'][0]
    elif len(ctx.env['CC']):
        compilerExecutable = ctx.env['CC'][0]
    else:
        ctx.fatal('CC or CXX environment variables not defined: Error, is the compiler correctly detected?')

    result = os.popen(compilerExecutable + ' -print-search-dirs')
    searchDirs = []
    localLibPath = os.path.join('/', 'usr', 'lib64')
    if os.path.exists(localLibPath):
        searchDirs.append(localLibPath)
    localLibPath = os.path.join('/', 'usr', 'local', 'lib')
    if os.path.exists(localLibPath):
        searchDirs.append(localLibPath)
    localLibPath = os.path.join('/', 'sw', 'lib')
    if os.path.exists(localLibPath):
        searchDirs.append(localLibPath)
    gccLines = result.readlines()
    for curLine in gccLines:
        startFound = curLine.find('libraries: =')
        if startFound != -1:
            curLine = curLine[startFound + 12:-1]
            searchDirs_ = curLine.split(':')
            for i in searchDirs_:
                if os.path.exists(i) and not os.path.abspath(i) in searchDirs:
                    searchDirs.append(os.path.abspath(i))
            break
    ctx.msg('Determining gcc search path', 'ok')

    ########################################
    # Parsing command options
    ########################################
    if not ctx.options.enable_tools:
        ctx.env.append_unique('DEFINES', 'DISABLE_TOOLS')
    if ctx.options.static_build:
        ctx.env['FULLSTATIC'] = True
    if ctx.options.enable_history:
        ctx.env.append_unique('DEFINES', 'ENABLE_HISTORY')
    # We need toc export the GRLIB Paths to vsim subshells for co-simulation
    if ctx.options.grlibdir:
      ctx.env['GRLIB_HOME'] = ctx.options.grlibdir
    if ctx.options.grlibtech:
      ctx.env['GRLIB_TECH'] = ctx.options.grlibtech
    # Set verbosity level
    if ctx.options.verbosity:
      ctx.env.append_unique('DEFINES', 'GLOBALVERBOSITY=%s' % ctx.options.verbosity)


    ########################################
    # Adding the custom preprocessor macros
    ########################################
    if ctx.options.define_tsim_compatibility:
        ctx.env.append_unique('DEFINES', 'TSIM_COMPATIBILITY')

    ########################################
    # Load unit test framework
    ########################################
    ctx.load('waf_unit_test')

    ########################################
    # Load modelsim and check for dependenies
    ########################################
    ctx.load('modelsim', tooldir='tools/waf')
    ctx.load('systools', tooldir='tools/waf')
    
    ###########################################################
    # Check for ELF library and headers
    ###########################################################
    ctx.check(header_name='cxxabi.h', features='cxx cprogram', mandatory=True)
    ctx.check_cxx(function_name='abi::__cxa_demangle', header_name="cxxabi.h", mandatory=True)
    if ctx.options.elfdir:
        elfIncPath=[os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.elfdir, 'include')))),
                    os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.elfdir, 'include', 'libelf'))))]
        elfLibPath=os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.elfdir, 'lib'))))
        if not os.path.exists(os.path.join(elfLibPath, ctx.env['cxxstlib_PATTERN'] % 'elf')) and  not os.path.exists(os.path.join(elfLibPath, ctx.env['cxxshlib_PATTERN'] % 'elf')):
            ctx.fatal('Unable to find libelf in specified path ' + elfLibPath)
        elfHeaderFound = False
        for path in elfIncPath:
            if os.path.exists(os.path.join(path, 'libelf.h')) and os.path.exists(os.path.join(path, 'gelf.h')):
                elfHeaderFound = True
                break
        if not elfHeaderFound:
            ctx.fatal('Unable to find libelf.h and/or gelf.h headers in specified path ' + str(elfIncPath))
        if ctx.check_cxx(lib='elf', uselib_store='ELF_LIB', mandatory=False, libpath = elfLibPath):
            ctx.check(header_name='libelf.h', uselib='ELF_LIB', uselib_store='ELF_LIB', features='cxx cprogram', mandatory=True, includes = elfIncPath)
            ctx.check(header_name='gelf.h', uselib='ELF_LIB', uselib_store='ELF_LIB', features='cxx cprogram', mandatory=True, includes = elfIncPath)
        foundShared = glob.glob(os.path.join(elfLibPath, ctx.env['cxxshlib_PATTERN'] % 'elf'))
        if foundShared:
            ctx.env.append_unique('RPATH', elfLibPath)
    else:
        if ctx.check_cxx(lib='elf', uselib_store='ELF_LIB', mandatory = True):
            ctx.check(header_name='libelf.h', uselib='ELF_LIB', uselib_store='ELF_LIB', features='cxx cprogram', mandatory=True)
            ctx.check(header_name='gelf.h', uselib='ELF_LIB', uselib_store='ELF_LIB', features='cxx cprogram', mandatory=True)
    if 'elf' in ctx.env['LIB_ELF_LIB']:
        pass
        ctx.check_cxx(fragment='''
            #include <libelf.h>
            
            int main(int argc, char *argv[]){
                void * funPtr = (void *)elf_getphdrnum;
                return 0;
            }
        ''', msg='Checking for function elf_getphdrnum', use='ELF_LIB', mandatory=True, errmsg='Error, elf_getphdrnum not present in libelf; try to update to a newest version')


    #########################################################
    # Check for the winsock library
    #########################################################
    if sys.platform == 'cygwin':
        ctx.check_cxx(lib='ws2_32', uselib_store='WINSOCK', mandatory=True)

    ##################################################
    # Check for pthread library/flag
    ##################################################
    if not ctx.check_cxx(linkflags='-pthread') or not ctx.check_cc(cxxflags='-pthread') or sys.platform == 'cygwin':
        ctx.env.append_unique('LIB', 'pthread')
    else:
        ctx.env.append_unique('LINKFLAGS', '-pthread')
        ctx.env.append_unique('CXXFLAGS', '-pthread')
        ctx.env.append_unique('CFLAGS', '-pthread')
        ctx.env.append_unique('CCFLAGS', '-pthread')

    ##################################################
    # Is SystemC compiled? Check for SystemC library
    # Notice that we can't rely on lib-linux, therefore I have to find the actual platform...
    ##################################################
    # First I set the clafgs needed by TLM 2.0  and GreenSocs for including systemc dynamic process
    # creation and so on
    ctx.env.append_unique('DEFINES', '_REENTRANT')
    ctx.env.append_unique('DEFINES', 'USE_STATIC_CASTS')
    ctx.env.append_unique('DEFINES', 'SC_INCLUDE_DYNAMIC_PROCESSES')
    syscpath = None
    if ctx.options.systemcdir:
        syscpath = ([os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.systemcdir, 'include'))))])

    sysclib = ''
    if syscpath:
        sysclib = glob.glob(os.path.join(os.path.abspath(os.path.join(syscpath[0], '..')), 'lib-*'))
    ctx.check_cxx(lib='systemc', uselib_store='SYSTEMC', mandatory=True, libpath=sysclib, errmsg='not found, use --systemc option')

    ######################################################
    # Check if systemc is compiled with quick threads or not
    ######################################################
    if not os.path.exists(os.path.join(syscpath[0] , 'sysc' , 'qt')):
        ctx.env.append_unique('DEFINES', 'SC_USE_PTHREADS')
    elif sys.platform == 'cygwin':
        ctx.fatal('SystemC under cygwin must be compiled with PThread support: recompile it using the "make pthreads" command')

    ##################################################
    # Check for SystemC header and test the library
    ##################################################
    if not sys.platform == 'cygwin':
        systemCerrmsg='Error, at least version 2.2.0 required'
    else:
        systemCerrmsg='Error, at least version 2.2.0 required.\nSystemC also needs patching under cygwin:\nplease controll that lines 175 and 177 of header systemc.h are commented;\nfor more details refer to http://www.ht-lab.com/howto/sccygwin/sccygwin.html\nhttp://www.dti.dk/_root/media/27325_SystemC_Getting_Started_artikel.pdf'
    ctx.check_cxx(header_name='systemc.h', use='SYSTEMC', uselib_store='SYSTEMC', mandatory=True, includes=syscpath)
    ctx.check_cxx(fragment='''
        #include <systemc.h>

        #ifndef SYSTEMC_VERSION
        #error SYSTEMC_VERSION not defined in file sc_ver.h
        #endif

        #if SYSTEMC_VERSION < 20070314
        #error Wrong SystemC version
        #endif

        extern "C" {
            int sc_main(int argc, char** argv) {
                wif_trace_file trace("");
                trace.set_time_unit(1, SC_NS);
                return 0;
            };
        }
''', msg='Checking for SystemC version', use='SYSTEMC', mandatory=True, errmsg=systemCerrmsg)

    ##################################################
    # Check for TLM header
    ##################################################
    tlmPath = ''
    if ctx.options.tlmdir:
        tlmPath = os.path.normpath(os.path.abspath(os.path.expanduser(os.path.expandvars(ctx.options.tlmdir))))
    if not tlmPath.endswith('include'):
        tlmPath = os.path.join(tlmPath, 'include')
    tlmPath = [os.path.join(tlmPath, 'tlm')]

    ctx.check_cxx(header_name='tlm.h', use='SYSTEMC', uselib_store='TLM', mandatory=True, includes=tlmPath, errmsg='not found, use --tlm option')
    ctx.check_cxx(fragment='''
        #include <systemc.h>
        #include <tlm.h>

        #ifndef TLM_VERSION_MAJOR
        #error TLM_VERSION_MAJOR undefined in the TLM library
        #endif
        #ifndef TLM_VERSION_MINOR
        #error TLM_VERSION_MINOR undefined in the TLM library
        #endif
        #ifndef TLM_VERSION_PATCH
        #error TLM_VERSION_PATCH undefined in the TLM library
        #endif

        #if TLM_VERSION_MAJOR < 2
        #error Wrong TLM version; required 2.0
        #endif

        extern "C" int sc_main(int argc, char **argv){
            return 0;
        }
''', msg='Check for TLM version', use='SYSTEMC TLM', mandatory=True, errmsg='Error, at least version 2.0 required')

    ##################################################
    # Check for TRAP runtime libraries and headers
    ##################################################
    trapRevisionNum = 772
    trapDirLib = ''
    trapDirInc = ''
    trapLibErrmsg = 'not found, use --trapgen option. It might also be that the trap library is compiled '
    trapLibErrmsg += 'against libraries (bfd/libelf, boost, etc.) different from the ones being used now; in case '
    trapLibErrmsg += 'try recompiling trap library.'
    if ctx.options.trapdir:
        trapDirLib = os.path.abspath(os.path.expandvars(os.path.expanduser(os.path.join(ctx.options.trapdir, 'lib'))))
        trapDirInc = os.path.abspath(os.path.expandvars(os.path.expanduser(os.path.join(ctx.options.trapdir, 'include'))))
        ctx.check_cxx(lib='trap', use='ELF_LIB BOOST SYSTEMC', uselib_store='TRAP', mandatory=True, libpath=trapDirLib, errmsg=trapLibErrmsg)
        foundShared = glob.glob(os.path.join(trapDirLib, ctx.env['cxxshlib_PATTERN'] % 'trap'))
        if foundShared:
            ctx.env.append_unique('RPATH', ctx.env['LIBPATH_TRAP'])


        if not check_trap_linking(ctx, 'trap', ctx.env['LIBPATH_TRAP'], 'elf_begin') and 'bfd' not in ctx.env['LIB_ELF_LIB']:
            ctx.fatal('TRAP library not linked with libelf library: BFD library needed (you might need to re-create the processor specifying a GPL license) or compile TRAP using its LGPL flavour ')

        ctx.check_cxx(header_name='trap.hpp', use='TRAP ELF_LIB BOOST SYSTEMC', uselib_store='TRAP', mandatory=True, includes=trapDirInc)
        ctx.check_cxx(fragment='''
            #include "trap.hpp"

            #ifndef TRAP_REVISION
            #error TRAP_REVISION not defined in file trap.hpp
            #endif

            #if TRAP_REVISION < ''' + str(trapRevisionNum) + '''
            #error Wrong version of the TRAP runtime: too old
            #endif

            int main(int argc, char * argv[]){return 0;}
''', msg='Check for TRAP version', use='TRAP ELF_LIB BOOST SYSTEMC', mandatory=True, includes=trapDirInc, errmsg='Error, at least revision ' + str(trapRevisionNum) + ' required')
    else:
        ctx.check_cxx(lib='trap', use='ELF_LIB BOOST SYSTEMC', uselib_store='TRAP', mandatory=True, errmsg=trapLibErrmsg)


        if not check_trap_linking(ctx, 'trap', ctx.env['LIBPATH_TRAP'], 'elf_begin') and 'bfd' not in ctx.env['LIB_ELF_LIB']:
            ctx.fatal('TRAP library not linked with libelf library: BFD library needed (you might need to re-create the processor specifying a GPL license) or compile TRAP using its LGPL flavour ')

        ctx.check_cxx(header_name='trap.hpp', use='TRAP ELF_LIB BOOST SYSTEMC', uselib_store='TRAP', mandatory=True)
        ctx.check_cxx(fragment='''
            #include "trap.hpp"

            #ifndef TRAP_REVISION
            #error TRAP_REVISION not defined in file trap.hpp
            #endif

            #if TRAP_REVISION < ''' + str(trapRevisionNum) + '''
            #error Wrong version of the TRAP runtime: too old
            #endif

            int main(int argc, char * argv[]){return 0;}
''', msg='Check for TRAP version', use='TRAP ELF_LIB BOOST SYSTEMC', mandatory=1, errmsg='Error, at least revision ' + str(trapRevisionNum) + ' required')

    ##################################################
    # Check for Lua Library and Headers
    ##################################################
    if ctx.options.luadir:
      lualib = glob.glob(os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.luadir, "lib")))))
      luadir = glob.glob(os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.luadir, "include")))))
    else:
      lualib = "/usr/lib"
      luadir = "/usr/include/lua5.1"

    ctx.check_cxx(
      lib          = 'lua5.1',
      uselib_store = 'LUA',
      mandatory    = True,
      libpath      = lualib,
      errmsg       = "LUA Library not found. Use --lua option or set $LUA.",
      okmsg        = "ok"
    ) 
    ctx.check_cxx(
      header_name  = 'lua.h',
      uselib_store = 'LUA',
      mandatory    = True,
      includes     = luadir,
      uselib       = 'LUA',
      okmsg        = "ok"
    ) 
    
    ctx.check_cxx(
      msg          = "Checking for Lua 5.1.4 or higher",
      mandatory    = True,
      execute      = True,
      fragment     = """
                     #include <lua.h>
                     int main(int argc, char *argv[]) {
                       return !(LUA_VERSION_NUM > 500);
                     }
                   """,
      uselib       = 'LUA',
      okmsg        = "ok",
    )

#Commi


    ##################################################
    # Check for LIBAV  Library and Headers
    ##################################################

    if ctx.options.avdir:
      avlib = glob.glob(os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.avdir, "lib")))))
      avdir = glob.glob(os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.avdir, "include")))))
    else:
      avlib = ["/usr/lib", "/usr/lib/x86_64-linux-gnu/"]
      avdir = "/usr/include"

    ctx.check_cxx(
      lib          = 'avcodec',
      uselib_store = 'AV',
      mandatory    = True,
      libpath      = avlib,
      errmsg       = "LIBAV_CODEC Library not found.",
      okmsg        = "ok"
    ) 
    ctx.check_cxx(
      header_name  = 'libavcodec/avcodec.h',
      uselib_store = 'AV',
      mandatory    = True,
      includes     = avdir,
      defines      = "__STDC_CONSTANT_MACROS",
      uselib       = 'AV',
      okmsg        = "ok"
    ) 
    

    ctx.check_cxx(
      lib          = 'avformat',
      uselib_store = 'AV',
      mandatory    = True,
      libpath      = avlib,
      errmsg       = "LIBAV_FORMAT Library not found.",
      okmsg        = "ok"
    ) 
    ctx.check_cxx(
      header_name  = 'libavformat/avformat.h',
      uselib_store = 'AV',
      mandatory    = True,
      includes     = avdir,
      defines      = "__STDC_CONSTANT_MACROS",
      uselib       = 'AV',
      okmsg        = "ok"
    ) 
    

    ctx.check_cxx(
      lib          = 'avdevice',
      uselib_store = 'AV',
      mandatory    = True,
      libpath      = avlib,
      errmsg       = "LIBAV_DEVICE Library not found.",
      okmsg        = "ok"
    ) 
    ctx.check_cxx(
      header_name  = 'libavdevice/avdevice.h',
      uselib_store = 'AV',
      mandatory    = True,
      includes     = avdir,
      defines      = "__STDC_CONSTANT_MACROS",
      uselib       = 'AV',
      okmsg        = "ok"
    ) 


    ctx.check_cxx(
      lib          = 'swscale',
      uselib_store = 'AV',
      mandatory    = True,
      libpath      = avlib,
      errmsg       = "LIBAV_DEVICE Library not found.",
      okmsg        = "ok"
    ) 
    ctx.check_cxx(
      header_name  = 'libswscale/swscale.h',
      uselib_store = 'AV',
      mandatory    = True,
      includes     = avdir,
      defines      = "__STDC_CONSTANT_MACROS",
      uselib       = 'AV',
      okmsg        = "ok"
    ) 
    

    


    ##################################################
    # Check for SDL Library and Headers
    ##################################################
    if ctx.options.sdldir:
      sdllib = glob.glob(os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.sdldir, "lib")))))
      sdldir = glob.glob(os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.sdldir, "include")))))
    else:
      sdllib = "/usr/lib"
      sdldir = "/usr/include"

    ctx.check_cxx(
      lib          = 'SDL',
      uselib_store = 'SDL',
      mandatory    = False,
      libpath      = sdllib,
      errmsg       = "SDL Library not found. Use --sdl option or set $SDL.",
      okmsg        = "ok"
    ) 
    ctx.check_cxx(
      header_name  = 'SDL/SDL.h',
      uselib_store = 'SDL',
      mandatory    = False,
      includes     = sdldir,
      uselib       = 'SDL',
      okmsg        = "ok"
    ) 
    
    ctx.check_cxx(
      msg          = "Checking for SDL 1.2 or higher",
      mandatory    = False,
      execute      = True,
      fragment     = """
                     #include <SDL/SDL_version.h>
                     int main(int argc, char *argv[]) {
                       return SDL_VERSION_ATLEAST(1, 2, 0);
                     }
                   """,
      uselib       = 'SDL',
      okmsg        = "ok",
    )

    ##################################################
    # Check for mpeg3 Library and Headers
    ##################################################
    if ctx.options.sdldir:
      mpeg3lib = glob.glob(os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.mpeg3dir, "lib")))))
      mpeg3dir = glob.glob(os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.mpeg3dir, "include")))))
    else:
      mpeg3lib = "/usr/lib"
      mpeg3dir = "/usr/include"

    ctx.check_cxx(
      lib          = 'mpeg3',
      uselib_store = 'MPEG3',
      mandatory    = False,
      libpath      = mpeg3lib,
      errmsg       = "mpeg3 Library not found. Use --mpeg3 option or set $MPEG3.",
      okmsg        = "ok"
    ) 
    ctx.check_cxx(
      header_name  = 'libmpeg3.h',
      uselib_store = 'MPEG3',
      mandatory    = False,
      includes     = mpeg3dir,
      uselib       = 'MPEG3',
      okmsg        = "ok"
    ) 
    
    '''
    ##################################################
    # Check for SystemC Verification Library
    ##################################################
    # TODO: It has to be checked if we realy need them
    ctx.check_cxx(
      lib          = 'scv',
      uselib_store = 'SCV',
      mandatory    = True,
      libpath      = glob.glob(os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(options.scvdir, "lib-*"))))),
      errmsg     = "SystemC Verification Library not found. Use --scv option or set $SCV."
    ) 
    ctx.check_cxx(
      header_name  = 'scv.h',
      uselib_store = 'SCV',
      mandatory    = True,
      includes     = os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(options.scvdir, "include")))),
      uselib       = 'SYSTEMC SCV'
    ) 
    ctx.check_cxx(
      msg          = "Checking for SCV Library 2.2 or higher",
      mandatory    = True,
      execute      = True,
      fragment     = """
                   #include <scv.h>
                   int main(int argc, char *argv[]) {
                     return !(SC_VERSION > 2001999);
                   }
      """,
      uselib       = 'SYSTEMC SCV',
    )
    '''

    ##################################################
    # Check for GreenSocs GreenSockets Header
    ##################################################
    if ctx.options.greensocsdir:
      gs_inc      = [os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.greensocsdir))))]
    else:
      gs_inc      = []

    ctx.check_cxx(
      header_name   = "greensocket/initiator/single_socket.h",
      uselib_store  = 'GREENSOCS',
      mandatory     = True,
      includes      = gs_inc,
#                       ctx.env['INCLUDES_BOOST']],
      uselib        = 'BOOST SYSTEMC TLM',
      okmsg        = "ok",
    )
    ctx.check_cxx(
      header_name   = "greensocket/target/single_socket.h",
      uselib_store  = 'GREENSOCS',
      mandatory     = True,
      includes      = gs_inc,
      uselib        = 'GREENSOCS BOOST SYSTEMC TLM',
      okmsg        = "ok",
    )

    ##################################################
    # Check for GreenSocs GreenControl
    ##################################################
    ctx.check_cxx(
      header_name   = "greencontrol/config.h",
      uselib_store  = 'GREENSOCS',
      mandatory     = True,
      includes      = gs_inc,
      uselib        = 'GREENSOCS BOOST SYSTEMC TLM',
      okmsg        = "ok",
    )

    ##################################################
    # Check for GreenSocs GreenReg Library
    ##################################################
    if ctx.options.greensocsdir:
      grreg_inc      = [os.path.abspath(os.path.expanduser(os.path.expandvars(os.path.join(ctx.options.greensocsdir, "greenreg"))))]
    else:
      grreg_inc      = []

    ctx.check_cxx(
      lib          = 'greenreg',
      uselib_store = 'GREENSOCS',
      mandatory    = True,
      libpath      = grreg_inc,
      okmsg        = "ok",
    ) 

    ##################################################
    # Check for GreenSocs GreenReg Header
    ##################################################
    ctx.check_cxx(
      header_name   = "greenreg.h",
      uselib_store  = 'GREENSOCS',
      mandatory     = True,
      includes      = grreg_inc,
      uselib        = 'GREENSOCS BOOST SYSTEMC TLM',
      okmsg        = "ok",
    )

    ##################################################
    # Check for AMBAKit
    ##################################################
    if ctx.options.ambadir:
      amba_inc = [os.path.abspath(os.path.expanduser(os.path.expandvars(ctx.options.ambadir))),
                  os.path.join(os.path.abspath(os.path.expanduser(os.path.expandvars(ctx.options.ambadir))), "dependencies", "AMBA-PV", "include")]
    else:
      amba_inc = []

    ctx.check_cxx(
      header_name   = "amba.h",
      uselib_store  = 'AMBA',
      mandatory     = True,
      includes      = amba_inc,
      uselib        = 'BOOST SYSTEMC TLM GREENSOCS',
      okmsg        = "ok",
      errmsg        = 'AMBAKit not found please give the location with --amba=',
    )

    ##################################################
    # Check for AMBAKit Headers
    ##################################################
    ctx.check_cxx(
      msg          = "Checking for AMBAKit Version > 1.0.5",
      mandatory    = True,
      execute      = True,
      fragment     = """
                     #include <amba.h>
                     int main(int argc, char *argv[]) {
                       return !((AMBA_TLM_VERSION_MAJOR >= 1) && (AMBA_TLM_VERSION_MINOR >= 0) && (AMBA_TLM_VERSION_REVISION >= 6));
                     }
                     """,
      uselib       = 'BOOST SYSTEMC TLM GREENSOCS AMBA',
      okmsg        = "ok",
    )
   
    ##################################################
    # Check for AMBAKit extended GreenSocs from contrib
    ##################################################
    ctx.check_cxx(
      header_name   = "greenreg_ambasockets.h",
      uselib_store  = 'GREENSOCS',
      mandatory     = True,
      includes      = [os.path.join(ctx.path.abspath(),'contrib', 'grambasockets'), os.path.join(ctx.path.abspath(), 'common'), os.path.join(ctx.path.abspath(), 'include')],
      uselib        = 'GREENSOCS BOOST SYSTEMC TLM AMBA',
      okmsg         = "ok",
      msg           = "Check compatibility of AMBAKit and GreenReg"
    )
    ##################################################
    # Check for RTL Adapters. If not present deactivate RTL (Co-)Simulations
    ##################################################
    if not os.path.isdir("adapters"):
      ctx.env["MODELSIM"] = False
    
    ##################################################
    # Extend GreenSocs with TLM extensionPool.h
    ##################################################
    """
    if ctx.options.tlmdir:
        tlmPath = os.path.normpath(os.path.abspath(os.path.expanduser(os.path.expandvars(ctx.options.tlmdir))))
    if tlmPath.endswith('include'):
        tlmPath = os.path.join(tlmPath, '..')
    tlmPath = os.path.join(tlmPath, "unit_test", "tlm", "multi_sockets", "include")
    ctx.check_cxx(
      header_name   = [ "tlm.h",
                        "tlm_utils/multi_passthrough_initiator_socket.h",
                        "tlm_utils/multi_passthrough_target_socket.h",
                        "simpleAddressMap.h",
                        "extensionPool.h"
                      ],
      uselib_store  = 'GREENSOCS',
      msg           = "Checking for extensionPool.h from TLM",
      mandatory     = True,
      includes      = [tlmPath],
      uselib        = 'SYSTEMC TLM GREENSOCS',
      okmsg        = "ok",
    )
    """

    sr_libdir = os.path.join(ctx.path.abspath(), 'lib')
    sr_incdir = os.path.join(ctx.path.abspath(), 'include')
    if os.path.isdir(sr_libdir) and os.path.isdir(sr_incdir):
        ##################################################
        # Check for SoCRocket Libs if lib dir is present
        ##################################################
        for lib in os.listdir(sr_libdir):
            if lib.startswith("lib") and not lib in ['libcommon.a', 'libutils.a']:
                name = lib[3:-2]
                ctx.check_cxx(
                    lib          = name,
                    uselib_store = 'SOCROCKET',
                    mandatory    = True,
                    libpath      = sr_libdir,
                ) 
                ctx.check_cxx(
                    header_name  = '%s.h' % (name),
                    uselib_store = 'SOCROCKET',
                    mandatory    = False,
                    includes     = sr_incdir,
                    uselib       = 'SOCROCKET SYSTEMC TLM AMBA BOOST GREENSOCS',
                ) 
        ctx.check_cxx(
            lib          = 'utils',
            uselib_store = 'SOCROCKET',
            mandatory    = True,
            libpath      = sr_libdir,
        ) 
        ctx.check_cxx(
            header_name  = 'ahbdevice.h',
            uselib_store = 'SOCROCKET',
            mandatory    = False,
            includes     = sr_incdir,
            uselib       = 'SOCROCKET SYSTEMC TLM AMBA BOOST GREENSOCS',
        ) 
        ctx.check_cxx(
            lib          = 'common',
            uselib_store = 'SOCROCKET',
            mandatory    = True,
            libpath      = sr_libdir,
        ) 
        ctx.check_cxx(
            header_name  = 'common.h',
            uselib_store = 'SOCROCKET',
            mandatory    = False,
            includes     = sr_incdir,
            uselib       = 'SOCROCKET SYSTEMC TLM AMBA BOOST GREENSOCS',
        ) 
    
    ##################################################
    # SPARC compiler search
    ##################################################
    ctx.setenv('sparc')
    ctx.load('compiler_c')
    #sparc_env = ctx.env.copy()
    # Check if the compiler is present
    crosscc = crossxx = crossar = path = None
    
    try:
        path = os.path.abspath(os.path.expanduser(getattr(ctx.options,'sparc_cross')))
        crosscc = [ctx.find_program(ctx.options.sparc_prefix + 'gcc', path_list=[os.path.join(path, 'bin')])]
    except AttributeError as e:
        # If the path was not specified look in the search PATH
        crosscc = [ctx.find_program(ctx.options.sparc_prefix + 'gcc')]

    try:
        path = os.path.abspath(os.path.expanduser(getattr(ctx.options,'sparc_cross')))
        crossxx = [ctx.find_program(ctx.options.sparc_prefix + 'g++', path_list=[os.path.join(path, 'bin')])]
    except AttributeError as e:
        # If the path was not specified look in the search PATH
        crossxx = [ctx.find_program(ctx.options.sparc_prefix + 'g++')]

    try:
        path = os.path.abspath(os.path.expanduser(getattr(ctx.options,'sparc_cross')))
        crossar = [ctx.find_program(ctx.options.sparc_prefix + 'ar', path_list=[os.path.join(path, 'bin')])]
    except AttributeError as e:
        # If the path was not specified look in the search PATH
        crossar = [ctx.find_program(ctx.options.sparc_prefix + 'ar')]

    ctx.env['CC'] = ctx.env['LINK_CC'] = crosscc
    ctx.env['CXX'] = ctx.env['LINK_CXX'] = crossxx
    ctx.env['AR'] = crossar
    #ctx.set_env_name('sparc', sparc_env)
    sparcFlags = ['-Wall', '-static', '-O3']
    ctx.env.append_unique('LINKFLAGS', sparcFlags)
    ctx.env['SHLIB_MARKER'] = ""
    if  '-Wl,-Bdynamic' in ctx.env['LINKFLAGS']:
      ctx.env['LINKFLAGS'].remove('-Wl,-Bdynamic')
    ctx.env.append_unique('CFLAGS', sparcFlags)
    if  '-Wl,-Bdynamic' in ctx.env['CFLAGS']:
      ctx.env['CFLAGS'].remove('-Wl,-Bdynamic')
    ctx.env.append_unique('CCFLAGS', sparcFlags)
    if  '-Wl,-Bdynamic' in ctx.env['CCFLAGS']:
      ctx.env['CCFLAGS'].remove('-Wl,-Bdynamic')
    #ctx.env.append_unique('CXXFLAGS', sparcFlags)
    
    if ctx.env['CFLAGS']:
        ctx.check_cc(cflags=ctx.env['CFLAGS'], mandatory=True, msg='Checking for C compilation flags')
    if ctx.env['CCFLAGS'] and ctx.env['CCFLAGS'] != ctx.env['CFLAGS']:
        ctx.check_cc(cflags=ctx.env['CCFLAGS'], mandatory=True, msg='Checking for C compilation flags')
    #if ctx.env['CXXFLAGS']:
    #    ctx.check_cxx(cxxflags=ctx.env['CXXFLAGS'], mandatory=True, msg='Checking for C++ compilation flags')
    if ctx.env['LINKFLAGS']:
        ctx.check_cc(linkflags=ctx.env['LINKFLAGS'], mandatory=True, msg='Checking for link flags')
    
def options(ctx): 
    from os import environ
    configuration_options = ctx.get_option_group("--download")
  
    ctx.load('python', option_group=configuration_options)
    ctx.load('compiler_c', option_group=configuration_options)
    ctx.load('compiler_cxx', option_group=configuration_options)
    ctx.load('boost', option_group=configuration_options)
    ctx.load('waf_unit_test', option_group=configuration_options)
    ctx.load('common', option_group=configuration_options, tooldir='tools/waf')
    ctx.load('modelsim', option_group=configuration_options, tooldir='tools/waf')
    ctx.load('systools', option_group=configuration_options, tooldir='tools/waf')
  
    #ctx.add_option('--onlytests', action='store_true', default=True, help='Exec unit tests only', dest='only_tests')
    # Specify SystemC and TLM options
    sysc = ctx.add_option_group("SystemC configuration Options")
    ctx.add_option('--systemc', type='string', help='SystemC installation directory', dest='systemcdir', default=environ.get("SYSTEMC"))
    ctx.add_option('--tlm', type='string', help='TLM installation directory', dest='tlmdir', default=environ.get("TLM"))
    ctx.add_option('--trapgen', type='string', help='TRAP libraries and headers installation directory', dest='trapdir', default=environ.get("TRAP"))
  
    gso = ctx.add_option_group("GreenSoCs Configuration Options")
    gso.add_option("--greensocs", dest="greensocsdir", help="Basedir of your GreenSoCs instalation", default=environ.get("GREENSOCS"))
    gso.add_option("--lua", type='string', dest="luadir", help="Basedir of your Lua installation", default=environ.get("LUA"))
    gso.add_option("--sdl", type='string', dest="sdldir", help="Basedir of your SDL installation", default=environ.get("SDL"))
    gso.add_option("--mpeg3", type='string', dest="mpeg3dir", help="Basedir of your mpeg3 installation", default=environ.get("mpeg3"))
    gso.add_option('--libav', type='string', dest='avdir', help='Load given configuration of the template', default=environ.get("avdir"))
    gso.add_option("--amba", type='string', dest="ambadir", help="Basedir of your AMBAKit distribution", default=environ.get("AMBA"))
    gso.add_option("--grlib", type='string', dest="grlibdir", help="Basedir of your grlib distribution", default=environ.get("GRLIB_HOME"))
    gso.add_option("--grlib_tech", type='string', dest="grlibtech", help="Basedir of your modelsim grlib work libraries", default=environ.get("GRLIB_TECH"))

    trap = ctx.add_option_group("TrapGen Configuration Options")
    # Specify libELF library path
    trap.add_option('--with-elf', type='string', help='libELF installation directory', dest='elfdir', default=environ.get("ELF"))
    trap.add_option('--static', default=False, action="store_true", help='Triggers a static build, with no dependences from any dynamic library (LEON3)', dest='static_build')
    trap.add_option('--sparc-cross', default=None, help='Explicit path to the cross compiler. If not given it will searched on the path', dest='sparc_cross')
    trap.add_option('--sparc-prefix', default='sparc-elf-', type='string', help='Defines the sparc compiler prefix', dest='sparc_prefix')
    # Specify if OS emulation support should be compiled inside processor models
    trap.add_option('-T', '--disable-tools', default=True, action="store_false", help='Disables support for support tools (debuger, os-emulator, etc.) (switch)', dest='enable_tools')
    # Specify if instruction history has to be kept
    trap.add_option('-s', '--enable-history', default=False, action='store_true', help='Enables the history of executed instructions', dest='enable_history')
    prof = ctx.add_option_group("TrapGen Configuration Options")
    # Specify support for the profilers: gprof, vprof
    prof.add_option('-D', '--nodebug', default=True, action='store_false', help='Disables debugging support for the targets', dest='debug')
    #prof.add_option('-D', '--debug', default=True, action='store_true', help='Enables debugging support for the targets', dest='debug')
    prof.add_option('-G', '--gcov', default=False, action='store_true', help='Enables profiling with gcov profiler', dest='enable_gcov')
    prof.add_option('-P', '--gprof', default=False, action='store_true', help='Enables profiling with gprof profiler', dest='enable_gprof')
    prof.add_option('-V', '--vprof', default=False, action='store_true', help='Enables profiling with vprof profiler', dest='enable_vprof')
    prof.add_option('--with-vprof', type='string', help='vprof installation folder', dest='vprofdir')
    prof.add_option('--with-papi', type='string', help='papi installation folder', dest='papidir')
    # Custom Options
    prof.add_option('--tsim-comp', default=False, action='store_true', help='Defines the TSIM_COMPATIBILITY directive', dest='define_tsim_compatibility')

    ctx.add_option("--verbosity", dest="verbosity", help="Defines the verbosity for the build", default=environ.get("VERBOSITY",'4'))

    conf = ctx.add_option_group("'./waf generate' Options")
    conf.add_option('-t', '--template', default=None, type='string', help='Defines a template to generate a new platform', dest='template')
    conf.add_option('-l', '--load', default=None, type='string', help='Load given configuration of the template', dest='configuration')

