// vim : set fileencoding=utf-8 expandtab noai ts=4 sw=4 :
/// @addtogroup platforms
/// @{
/// @file sc_main.cpp
/// Top level file (sc_main) for system test. THIS FILE IS AUTOMATIC GENERATED.
/// CHANGES MIGHT GET LOST!!
///
/// @date 2010-2014
/// @copyright All rights reserved.
///            Any reproduction, use, distribution or disclosure of this
///            program, without the express, prior written consent of the 
///            authors is strictly prohibited.
/// @author Thomas Schuster
///
#include "core/common/gs_config.h"
#include "core/common/systemc.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <mcheck.h>
#include "core/common/amba.h"
#include "core/common/trapgen/debugger/GDBStub.hpp"
#include "core/common/trapgen/elfloader/execLoader.hpp"
#include "core/common/trapgen/osEmulator/osEmulator.hpp"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

#include "core/common/json_parser.h"
#include "core/common/paramprinter.h"
#include "core/common/verbose.h"
#include "core/common/powermonitor.h"
#include "core/models/mmu_cache/lib/leon3_mmu_cache.h"
#include "core/models/ahbin/ahbin.h"
#include "core/models/memory/memory.h"
#include "core/models/apbctrl/apbctrl.h"
#include "core/models/ahbmem/ahbmem.h"
#include "core/models/mctrl/mctrl.h"
#include "core/models/mmu_cache/lib/defines.h"
#include "core/models/gptimer/gptimer.h"
#include "core/models/apbuart/apbuart.h"
#include "core/models/apbuart/tcpio.h"
#include "core/models/apbuart/nullio.h"
#include "core/models/irqmp/irqmp.h"
#include "core/models/ahbctrl/ahbctrl.h"
#include "core/models/ahbprof/ahbprof.h"

#ifdef HAVE_SOCWIRE
#include "models/socwire/AHB2Socwire.h"
#endif
#ifdef HAVE_GRETH
#include "models/greth/greth/greth.h"
#include "vphy/tapdev.h"
#include "vphy/loopback.h"
#endif

#ifdef HAVE_PYSC
#include "pysc/pysc.h"
#endif

//#include "vphy/trafgen.h"

using namespace std;
using namespace sc_core;
#ifdef HAVE_SOCWIRE
using namespace socw;
#endif

namespace trap {
  extern int exitValue;
};

void stopSimFunction( int sig ){
  v::warn << "main" << "Simulation interrupted by user" << std::endl;
  sc_stop();
  wait(SC_ZERO_TIME);
}

boost::filesystem::path find_top_path(char *start) {

  #if (BOOST_VERSION < 104600)
    boost::filesystem::path path = boost::filesystem::path(start).parent_path();
  #else
    boost::filesystem::path path = boost::filesystem::absolute(boost::filesystem::path(start).parent_path());
  #endif

  boost::filesystem::path waf("waf");
  while(!boost::filesystem::exists(path/waf) && !path.empty()) {
    path = path.parent_path();
  }
  return path;

}

string greth_script_path;
void grethVPHYHook(char* dev_name)
{
  stringstream command;
  command << greth_script_path.c_str() << " " << dev_name << " &"; // Do not wait
  v::info << "GREth Post Script Path: " << command.str() << v::endl;
  string command_str = command.str();
  const char* command_c = command_str.c_str();

  int res = std::system(command_c);
  if(res<0)
  {
    v::error << "GREth Post Script Path: " << command << v::endl;
  }
  v::info << "GREth Post Script PID: " << res << v::endl;

  return;
}

int sc_main(int argc, char** argv) {
    boost::program_options::options_description desc("Options");
    desc.add_options()
      ("help", "Shows this message.")
      ("jsonconfig,j", boost::program_options::value<std::string>(), "The main configuration file. Usual config.json.")
#ifdef HAVE_PYSC
      ("pythonscript,p", boost::program_options::value<std::string>(), "The main python script file. Usual sc_main.py.")
#endif
      ("option,o", boost::program_options::value<std::vector<std::string> >(), "Additional configuration options.")
      ("argument,a", boost::program_options::value<std::vector<std::string> >(), "Arguments to the software running inside the simulation.")
#ifdef HAVE_GRETH
      ("greth,g", boost::program_options::value<std::vector<std::string> >(), "Initial Options for GREth-Core.")
#endif
      ("listoptions,l", "Show a list of all avaliable options")
      ("interactiv,i", "Start simulation in interactiv mode")
      ("listoptionsfiltered,f", boost::program_options::value<std::string>(), "Show a list of avaliable options containing a keyword")
      ("listgsconfig,c", "Show a list of all avaliable gs_config options")
      ("listgsconfigfiltered,g", boost::program_options::value<std::string>(), "Show a list of avaliable options containing a keyword")
      ("saveoptions,s", boost::program_options::value<std::string>(), "Save options to json file. Default: saved_config.json");

    boost::program_options::positional_options_description p;
    p.add("argument", -1);

    boost::program_options::variables_map vm;
    try {
      boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
      boost::program_options::notify(vm);
    } catch(boost::program_options::unknown_option o) {
      #if (BOOST_VERSION > 104600)
      std::cout << "Comand line argument '" << o.get_option_name() << "' unknown. Please use -a,[argument] to add it to the software arguments or correct it." << std::endl;
      #endif
      exit(1);
    }

    if(vm.count("help")) {
        std::cout << std::endl << "SoCRocket -- LEON3 Multi-Processor Platform" << std::endl;
        std::cout << std::endl << "Usage: " << argv[0] << " [options]" << std::endl;
        std::cout << std::endl << desc << std::endl;
        return 0;
    }

    clock_t cstart, cend;
    std::string prom_app;

    bool paramlist = false, paramlistfiltered = false, configlist = false, configlistfiltered = false;//, saveoptions = false;

    gs::ctr::GC_Core       core;
    gs::cnf::ConfigDatabase cnfdatabase("ConfigDatabase");
    gs::cnf::ConfigPlugin configPlugin(&cnfdatabase);

    json_parser* jsonreader = new json_parser();

    if(vm.count("jsonconfig")) {
        setenv("JSONCONFIG", vm["jsonconfig"].as<std::string>().c_str(), true);
    }

    // Find *.json
    // - First search on the comandline
    // - Then search environment variable
    // - Then search in current dir
    // - and finaly in the application directory
    // - searches in the source directory
    // Print an error if it is not found!
    boost::filesystem::path topdir = find_top_path(argv[0]);
    boost::filesystem::path appdir = (boost::filesystem::path(argv[0]).parent_path());
    boost::filesystem::path srcdir = (topdir / boost::filesystem::path("build") / boost::filesystem::path(__FILE__).parent_path());
    boost::filesystem::path json("leon3mp.json");
    char *json_env = std::getenv("JSONCONFIG");
    if(json_env) {
        json = boost::filesystem::path(json_env);
    } else if(boost::filesystem::exists(json)) {
        json = json;
    } else if(boost::filesystem::exists(appdir / json)) {
        json = appdir / json;
    } else if(boost::filesystem::exists(srcdir / json)) {
        json = srcdir / json;
    }
    if(boost::filesystem::exists(boost::filesystem::path(json))) {
        v::info << "main" << "Open Configuration " << json << v::endl;
        jsonreader->config(json.string().c_str());
    } else {
        v::warn << "main" << "No *.json found. Please put it in the current work directory, application directory or put the path to the file in the JSONCONFIG environment variable" << v::endl;
    }

    gs::cnf::cnf_api *mApi = gs::cnf::GCnf_Api::getApiInstance(NULL);
    if(vm.count("option")) {
        std::vector<std::string> vec = vm["option"].as< std::vector<std::string> >();
        for(std::vector<std::string>::iterator iter = vec.begin(); iter!=vec.end(); iter++) {
           std::string parname;
           std::string parvalue;

           // *** Check right format (parname=value)
           // of no space
           if(iter->find_first_of("=") == std::string::npos) {
               v::warn << "main" << "Option value in command line option has no '='. Type '--help' for help. " << *iter;
           }
           // if not space before equal sign
           if(iter->find_first_of(" ") < iter->find_first_of("=")) {
               v::warn << "main" << "Option value in command line option may not contain a space before '='. " << *iter;
           }

           // Parse parameter name
           parname = iter->substr(0,iter->find_first_of("="));
           // Parse parameter value
           parvalue = iter->substr(iter->find_first_of("=")+1);

           // Set parameter
           mApi->setInitValue(parname, parvalue);
        }
    }


    if(vm.count("listoptions")) {
       paramlist = true;
    }

    if(vm.count("listgsconfig")) {
       configlist = true;
    }
    //if(vm.count("saveoptions")) {
    //   saveoptions = true;
    //}

	std::string optionssearchkey = "";
	if(vm.count("listoptionsfiltered")) {
        optionssearchkey = vm["listoptionsfiltered"].as<std::string>();
	    paramlistfiltered = true;
    }

	std::string configssearchkey = "";
	if(vm.count("listgsconfigfiltered")) {
        configssearchkey = vm["listgsconfigfiltered"].as<std::string>();
	    configlistfiltered= true;
    }

#ifdef HAVE_PYSC
    // Initialize Python
    std::string pythonscript = "";
    if(vm.count("pythonscript")) {
      pythonscript = vm["pythonscript"].as<std::string>();
    }

    PythonModule python("python_interpreter", pythonscript.c_str(), argc, argv);
    python.start_of_initialization();
#endif  // HAVE_PYSC
    // Build GreenControl Configuration Namespace
    // ==========================================
    gs::gs_param_array p_conf("conf");
    gs::gs_param_array p_system("system", p_conf);

    // Decide whether LT or AT
    gs::gs_param<bool> p_system_at("at", false, p_system);
    gs::gs_param<unsigned int> p_system_ncpu("ncpu", 1, p_system);
    gs::gs_param<unsigned int> p_system_clock("clk", 20.0, p_system);
    gs::gs_param<std::string> p_system_osemu("osemu", "", p_system);
    gs::gs_param<std::string> p_system_log("log", "", p_system);

    gs::gs_param_array p_report("report", p_conf);
    gs::gs_param<bool> p_report_timing("timing", true, p_report);
    gs::gs_param<bool> p_report_power("power", true, p_report);
/*
    if(!((std::string)p_system_log).empty()) {
        v::logApplication((char *)((std::string)p_system_log).c_str());
    }
*/
    amba::amba_layer_ids ambaLayer;
    if(p_system_at) {
        ambaLayer = amba::amba_AT;
    } else {
        ambaLayer = amba::amba_LT;
    }

    // *** CREATE MODULES

    // AHBCtrl
    // =======
    // Always enabled.
    // Needed for basic platform
    gs::gs_param_array p_ahbctrl("ahbctrl", p_conf);
    gs::gs_param<unsigned int> p_ahbctrl_ioaddr("ioaddr", 0xFFF, p_ahbctrl);
    gs::gs_param<unsigned int> p_ahbctrl_iomask("iomask", 0xFFF, p_ahbctrl);
    gs::gs_param<unsigned int> p_ahbctrl_cfgaddr("cfgaddr", 0xFF0, p_ahbctrl);
    gs::gs_param<unsigned int> p_ahbctrl_cfgmask("cfgmask", 0xFF0, p_ahbctrl);
    gs::gs_param<bool> p_ahbctrl_rrobin("rrobin", false, p_ahbctrl);
    gs::gs_param<unsigned int> p_ahbctrl_defmast("defmast", 0u, p_ahbctrl);
    gs::gs_param<bool> p_ahbctrl_ioen("ioen", true, p_ahbctrl);
    gs::gs_param<bool> p_ahbctrl_fixbrst("fixbrst", false, p_ahbctrl);
    gs::gs_param<bool> p_ahbctrl_split("split", false, p_ahbctrl);
    gs::gs_param<bool> p_ahbctrl_fpnpen("fpnpen", true, p_ahbctrl);
    gs::gs_param<bool> p_ahbctrl_mcheck("mcheck", true, p_ahbctrl);

    AHBCtrl ahbctrl("ahbctrl",
		    p_ahbctrl_ioaddr,                // The MSB address of the I/O area
		    p_ahbctrl_iomask,                // The I/O area address mask
		    p_ahbctrl_cfgaddr,               // The MSB address of the configuration area
		    p_ahbctrl_cfgmask,               // The address mask for the configuration area
		    p_ahbctrl_rrobin,                // 1 - round robin, 0 - fixed priority arbitration (only AT)
		    p_ahbctrl_split,                 // Enable support for AHB SPLIT response (only AT)
		    p_ahbctrl_defmast,               // Default AHB master
		    p_ahbctrl_ioen,                  // AHB I/O area enable
		    p_ahbctrl_fixbrst,               // Enable support for fixed-length bursts (disabled)
		    p_ahbctrl_fpnpen,                // Enable full decoding of PnP configuration records
		    p_ahbctrl_mcheck,                // Check if there are any intersections between core memory regions
        p_report_power,                  // Enable/disable power monitoring
		    ambaLayer
    );

    // Set clock
    ahbctrl.set_clk(p_system_clock, SC_NS);

    // AHBSlave - APBCtrl
    // ==================

    gs::gs_param_array p_apbctrl("apbctrl", p_conf);
    gs::gs_param<unsigned int> p_apbctrl_haddr("haddr", 0x800, p_apbctrl);
    gs::gs_param<unsigned int> p_apbctrl_hmask("hmask", 0xFFF, p_apbctrl);
    gs::gs_param<unsigned int> p_apbctrl_index("hindex", 2u, p_apbctrl);
    gs::gs_param<bool> p_apbctrl_check("mcheck", true, p_apbctrl);

    APBCtrl apbctrl("apbctrl",
        p_apbctrl_haddr,    // The 12 bit MSB address of the AHB area.
        p_apbctrl_hmask,    // The 12 bit AHB area address mask
        p_apbctrl_check,    // Check for intersections in the memory map
        p_apbctrl_index,    // AHB bus index
        p_report_power,     // Power Monitoring on/off
		    ambaLayer           // TLM abstraction layer
    );

    // Connect to AHB and clock
    ahbctrl.ahbOUT(apbctrl.ahb);
    apbctrl.set_clk(p_system_clock, SC_NS);

    // APBSlave - IRQMP
    // ================
    // Needed for basic platform.
    // Always enabled

    gs::gs_param_array p_irqmp("irqmp", p_conf);
    gs::gs_param<unsigned int> p_irqmp_addr("addr", 0x1F0, p_irqmp);
    gs::gs_param<unsigned int> p_irqmp_mask("mask", 0xFFF, p_irqmp);
    gs::gs_param<unsigned int> p_irqmp_index("index", 2, p_irqmp);
    gs::gs_param<unsigned int> p_irqmp_eirq("eirq", 4, p_irqmp);

    Irqmp irqmp("irqmp",
                p_irqmp_addr,  // paddr
                p_irqmp_mask,  // pmask
                p_system_ncpu,  // ncpu
                p_irqmp_eirq,  // eirq
                p_irqmp_index, // pindex
                p_report_power // power monitoring
    );

    // Connect to APB and clock
    apbctrl.apb(irqmp.apb_slv);
    irqmp.set_clk(p_system_clock,SC_NS);

    // AHBSlave - MCtrl, ArrayMemory
    // =============================
    gs::gs_param_array p_mctrl("mctrl", p_conf);
    gs::gs_param_array p_mctrl_apb("apb", p_mctrl);
    gs::gs_param_array p_mctrl_prom("prom", p_mctrl);
    gs::gs_param_array p_mctrl_io("io", p_mctrl);
    gs::gs_param_array p_mctrl_ram("ram", p_mctrl);
    gs::gs_param_array p_mctrl_ram_sram("sram", p_mctrl_ram);
    gs::gs_param_array p_mctrl_ram_sdram("sdram", p_mctrl_ram);
    gs::gs_param<unsigned int> p_mctrl_apb_addr("addr", 0x000u, p_mctrl_apb);
    gs::gs_param<unsigned int> p_mctrl_apb_mask("mask", 0xFFF, p_mctrl_apb);
    gs::gs_param<unsigned int> p_mctrl_apb_index("index", 0u, p_mctrl_apb);
    gs::gs_param<unsigned int> p_mctrl_prom_addr("addr", 0x000u, p_mctrl_prom);
    gs::gs_param<unsigned int> p_mctrl_prom_mask("mask", 0xE00, p_mctrl_prom);
    gs::gs_param<unsigned int> p_mctrl_prom_asel("asel", 28, p_mctrl_prom);
    gs::gs_param<unsigned int> p_mctrl_prom_banks("banks", 2, p_mctrl_prom);
    gs::gs_param<unsigned int> p_mctrl_prom_bsize("bsize", 256, p_mctrl_prom);
    gs::gs_param<unsigned int> p_mctrl_prom_width("width", 32, p_mctrl_prom);
    gs::gs_param<unsigned int> p_mctrl_io_addr("addr", 0x200, p_mctrl_io);
    gs::gs_param<unsigned int> p_mctrl_io_mask("mask", 0xE00, p_mctrl_io);
    gs::gs_param<unsigned int> p_mctrl_io_banks("banks", 1, p_mctrl_io);
    gs::gs_param<unsigned int> p_mctrl_io_bsize("bsize", 512, p_mctrl_io);
    gs::gs_param<unsigned int> p_mctrl_io_width("width", 32, p_mctrl_io);
    gs::gs_param<unsigned int> p_mctrl_ram_addr("addr", 0x400, p_mctrl_ram);
    gs::gs_param<unsigned int> p_mctrl_ram_mask("mask", 0xC00, p_mctrl_ram);
    gs::gs_param<bool> p_mctrl_ram_wprot("wprot", false, p_mctrl_ram);
    gs::gs_param<unsigned int> p_mctrl_ram_asel("asel", 29, p_mctrl_ram);
    gs::gs_param<unsigned int> p_mctrl_ram_sram_banks("banks", 4, p_mctrl_ram_sram);
    gs::gs_param<unsigned int> p_mctrl_ram_sram_bsize("bsize", 128, p_mctrl_ram_sram);
    gs::gs_param<unsigned int> p_mctrl_ram_sram_width("width", 32, p_mctrl_ram_sram);
    gs::gs_param<unsigned int> p_mctrl_ram_sdram_banks("banks", 2, p_mctrl_ram_sdram);
    gs::gs_param<unsigned int> p_mctrl_ram_sdram_bsize("bsize", 256, p_mctrl_ram_sdram);
    gs::gs_param<unsigned int> p_mctrl_ram_sdram_width("width", 32, p_mctrl_ram_sdram);
    gs::gs_param<unsigned int> p_mctrl_ram_sdram_cols("cols", 16, p_mctrl_ram_sdram);
    gs::gs_param<unsigned int> p_mctrl_index("index", 0u, p_mctrl);
    gs::gs_param<bool> p_mctrl_ram8("ram8", true, p_mctrl);
    gs::gs_param<bool> p_mctrl_ram16("ram16", true, p_mctrl);
    gs::gs_param<bool> p_mctrl_sden("sden", true, p_mctrl);
    gs::gs_param<bool> p_mctrl_sepbus("sepbus", false, p_mctrl);
    gs::gs_param<unsigned int> p_mctrl_sdbits("sdbits", 32, p_mctrl);
    gs::gs_param<unsigned int> p_mctrl_mobile("mobile", 0u, p_mctrl);
    Mctrl mctrl( "mctrl",
        p_mctrl_prom_asel,
        p_mctrl_ram_asel,
        p_mctrl_prom_addr,
        p_mctrl_prom_mask,
        p_mctrl_io_addr,
        p_mctrl_io_mask,
        p_mctrl_ram_addr,
        p_mctrl_ram_mask,
        p_mctrl_apb_addr,
        p_mctrl_apb_mask,
        p_mctrl_ram_wprot,
        p_mctrl_ram_sram_banks,
        p_mctrl_ram8,
        p_mctrl_ram16,
        p_mctrl_sepbus,
        p_mctrl_sdbits,
        p_mctrl_mobile,
        p_mctrl_sden,
        p_mctrl_index,
        p_mctrl_apb_index,
        p_report_power,
        ambaLayer
    );

    // Connecting AHB Slave
    ahbctrl.ahbOUT(mctrl.ahb);
    // Connecting APB Slave
    apbctrl.apb(mctrl.apb);
    // Set clock
    mctrl.set_clk(p_system_clock, SC_NS);

    // CREATE MEMORIES
    // ===============

    // ROM instantiation
    Memory rom( "rom",
                     MEMDevice::ROM,
                     p_mctrl_prom_banks,
                     p_mctrl_prom_bsize * 1024 * 1024,
                     p_mctrl_prom_width,
                     0,
                     BaseMemory::MAP,
                     p_report_power
    );

    // Connect to memory controller and clock
    mctrl.mem(rom.bus);
    rom.set_clk(p_system_clock, SC_NS);

    // ELF loader from leon (Trap-Gen)
    gs::gs_param<std::string> p_mctrl_prom_elf("elf", "", p_mctrl_prom);
    if(!((std::string)p_mctrl_prom_elf).empty()) {
      if(boost::filesystem::exists(boost::filesystem::path((std::string)p_mctrl_prom_elf))) {
        uint8_t *execData;
        v::info << "rom" << "Loading Prom with " << p_mctrl_prom_elf << v::endl;
        ExecLoader prom_loader(p_mctrl_prom_elf);
        execData = prom_loader.getProgData();

        for(unsigned int i = 0; i < prom_loader.getProgDim(); i++) {
          rom.write_dbg(prom_loader.getDataStart() + i - ((((unsigned int)p_mctrl_prom_addr)&((unsigned int)p_mctrl_prom_mask))<<20), execData[i]);
        }
      } else {
        v::warn << "rom" << "File " << p_mctrl_prom_elf << " does not exist!" << v::endl;
        exit(1);
      }
    }

    // IO memory instantiation
    Memory io( "io",
               MEMDevice::IO,
               p_mctrl_prom_banks,
               p_mctrl_prom_bsize * 1024 * 1024,
               p_mctrl_prom_width,
               0,
               BaseMemory::MAP,
               p_report_power
    );

    // Connect to memory controller and clock
    mctrl.mem(io.bus);
    io.set_clk(p_system_clock, SC_NS);

    // ELF loader from leon (Trap-Gen)
    gs::gs_param<std::string> p_mctrl_io_elf("elf", "", p_mctrl_io);

    if(!((std::string)p_mctrl_io_elf).empty()) {
      if(boost::filesystem::exists(boost::filesystem::path((std::string)p_mctrl_io_elf))) {
        uint8_t *execData;
        v::info << "io" << "Loading IO with " << p_mctrl_io_elf << v::endl;
        ExecLoader loader(p_mctrl_io_elf);
        execData = loader.getProgData();

        for(unsigned int i = 0; i < loader.getProgDim(); i++) {
          io.write_dbg(loader.getDataStart() + i - ((((unsigned int)p_mctrl_io_addr)&((unsigned int)p_mctrl_io_mask))<<20), execData[i]);
        }
      } else {
        v::warn << "io" << "File " << p_mctrl_io_elf << " does not exist!" << v::endl;
        exit(1);
      }
    }

    // SRAM instantiation
    Memory sram( "sram",
                 MEMDevice::SRAM,
                 p_mctrl_ram_sram_banks,
                 p_mctrl_ram_sram_bsize * 1024 * 1024,
                 p_mctrl_ram_sram_width,
                 0,
                 BaseMemory::MAP,
                 p_report_power
    );

    // Connect to memory controller and clock
    mctrl.mem(sram.bus);
    sram.set_clk(p_system_clock, SC_NS);

    // ELF loader from leon (Trap-Gen)
    gs::gs_param<std::string> p_mctrl_ram_sram_elf("elf", "", p_mctrl_ram_sram);

    if(!((std::string)p_mctrl_ram_sram_elf).empty()) {
      if(boost::filesystem::exists(boost::filesystem::path((std::string)p_mctrl_ram_sram_elf))) {
        uint8_t *execData;
        v::info << "sram" << "Loading SRam with " << p_mctrl_ram_sram_elf << v::endl;
        ExecLoader loader(p_mctrl_ram_sram_elf);
        execData = loader.getProgData();

        for(unsigned int i = 0; i < loader.getProgDim(); i++) {
          sram.write_dbg(loader.getDataStart() + i - ((((unsigned int)p_mctrl_ram_addr)&((unsigned int)p_mctrl_ram_mask))<<20), execData[i]);
        }
      } else {
        v::warn << "sram" << "File " << p_mctrl_ram_sram_elf << " does not exist!" << v::endl;
        exit(1);
      }
    }

    // SDRAM instantiation
    Memory sdram( "sdram",
                       MEMDevice::SDRAM,
                       p_mctrl_ram_sdram_banks,
                       p_mctrl_ram_sdram_bsize * 1024 * 1024,
                       p_mctrl_ram_sdram_width,
                       p_mctrl_ram_sdram_cols,
                       BaseMemory::ARRAY,
                       p_report_power
    );

    // Connect to memory controller and clock
    mctrl.mem(sdram.bus);
    sdram.set_clk(p_system_clock, SC_NS);

    // ELF loader from leon (Trap-Gen)
    gs::gs_param<std::string> p_mctrl_ram_sdram_elf("elf", "", p_mctrl_ram_sdram);

    if(!((std::string)p_mctrl_ram_sdram_elf).empty()) {
      if(boost::filesystem::exists(boost::filesystem::path((std::string)p_mctrl_ram_sdram_elf))) {
        uint8_t *execData;
        v::info << "sdram" << "Loading SDRam with " << p_mctrl_ram_sdram_elf << v::endl;
        ExecLoader loader(p_mctrl_ram_sdram_elf);
        execData = loader.getProgData();

        for(unsigned int i = 0; i < loader.getProgDim(); i++) {
          sdram.write_dbg(loader.getDataStart() + i - ((((unsigned int)p_mctrl_ram_addr)&((unsigned int)p_mctrl_ram_mask))<<20), execData[i]);
        }
      } else {
        v::warn << "sdram" << "File " << p_mctrl_ram_sdram_elf << " does not exist!" << v::endl;
        exit(1);
      }
    }


    //leon3.ENTRY_POINT   = 0;
    //leon3.PROGRAM_LIMIT = 0;
    //leon3.PROGRAM_START = 0;

    // AHBSlave - AHBMem
    // =================
    gs::gs_param_array p_ahbmem("ahbmem", p_conf);
    gs::gs_param<bool> p_ahbmem_en("en", true, p_ahbmem);
    gs::gs_param<unsigned int> p_ahbmem_addr("addr", 0xA00, p_ahbmem);
    gs::gs_param<unsigned int> p_ahbmem_mask("mask", 0xFFF, p_ahbmem);
    gs::gs_param<unsigned int> p_ahbmem_index("index", 1, p_ahbmem);
    gs::gs_param<bool> p_ahbmem_cacheable("cacheable", 1, p_ahbmem);
    gs::gs_param<unsigned int> p_ahbmem_waitstates("waitstates", 0u, p_ahbmem);
    gs::gs_param<std::string> p_ahbmem_elf("elf", "", p_ahbmem);

    if(p_ahbmem_en) {

      AHBMem *ahbmem = new AHBMem("ahbmem",
                                  p_ahbmem_addr,
                                  p_ahbmem_mask,
                                  ambaLayer,
                                  p_ahbmem_index,
                                  p_ahbmem_cacheable,
                                  p_ahbmem_waitstates,
                                  p_report_power

      );

      // Connect to ahbctrl and clock
      ahbctrl.ahbOUT(ahbmem->ahb);
      ahbmem->set_clk(p_system_clock, SC_NS);

      // ELF loader from leon (Trap-Gen)
      if(!((std::string)p_ahbmem_elf).empty()) {
        if(boost::filesystem::exists(boost::filesystem::path((std::string)p_ahbmem_elf))) {
          uint8_t *execData;
          v::info << "ahbmem" << "Loading AHBMem with " << p_ahbmem_elf << v::endl;
          ExecLoader prom_loader(p_ahbmem_elf);
          execData = prom_loader.getProgData();

          for(unsigned int i = 0; i < prom_loader.getProgDim(); i++) {
            ahbmem->writeByteDBG(prom_loader.getDataStart() + i - ((((unsigned int)p_ahbmem_addr)&((unsigned int)p_ahbmem_mask))<<20), execData[i]);
          }
        } else {
          v::warn << "ahbmem" << "File " << p_ahbmem_elf << " does not exist!" << v::endl;
          exit(1);
        }
      }
    }


    // AHBMaster - ahbin (input_device)
    // ================================
    gs::gs_param_array p_ahbin("ahbin", p_conf);
    gs::gs_param<bool> p_ahbin_en("en", false, p_ahbin);
    gs::gs_param<unsigned int> p_ahbin_index("index", 1, p_ahbin);
    gs::gs_param<unsigned int> p_ahbin_irq("irq", 5, p_ahbin);
    gs::gs_param<unsigned int> p_ahbin_framesize("framesize", 128, p_ahbin);
    gs::gs_param<unsigned int> p_ahbin_frameaddr("frameaddr", 0xA00, p_ahbin);
    gs::gs_param<unsigned int> p_ahbin_interval("interval", 1, p_ahbin);
    if(p_ahbin_en) {
        AHBIn *ahbin = new AHBIn("ahbin",
          p_ahbin_index,
          p_ahbin_irq,
          p_ahbin_framesize,
          p_ahbin_frameaddr,
          sc_core::sc_time(p_ahbin_interval, SC_MS),
          p_report_power,
          ambaLayer
      );

      // Connect sensor to bus
      ahbin->ahb(ahbctrl.ahbIN);
      ahbin->set_clk(p_system_clock, SC_NS);

      // Connect interrupt out
      signalkit::connect(irqmp.irq_in, ahbin->irq, p_ahbin_irq);
    }

    // CREATE LEON3 Processor
    // ===================================================
    // Always enabled.
    // Needed for basic platform.
    gs::gs_param_array p_mmu_cache("mmu_cache", p_conf);
    gs::gs_param_array p_mmu_cache_ic("ic", p_mmu_cache);
    gs::gs_param<bool> p_mmu_cache_ic_en("en", true, p_mmu_cache_ic);
    gs::gs_param<int> p_mmu_cache_ic_repl("repl", 1, p_mmu_cache_ic);
    gs::gs_param<int> p_mmu_cache_ic_sets("sets", 4, p_mmu_cache_ic);
    gs::gs_param<int> p_mmu_cache_ic_linesize("linesize", 8, p_mmu_cache_ic);
    gs::gs_param<int> p_mmu_cache_ic_setsize("setsize", 8, p_mmu_cache_ic);
    gs::gs_param<bool> p_mmu_cache_ic_setlock("setlock", 1, p_mmu_cache_ic);
    gs::gs_param_array p_mmu_cache_dc("dc", p_mmu_cache);
    gs::gs_param<bool> p_mmu_cache_dc_en("en", true, p_mmu_cache_dc);
    gs::gs_param<int> p_mmu_cache_dc_repl("repl", 1, p_mmu_cache_dc);
    gs::gs_param<int> p_mmu_cache_dc_sets("sets", 2, p_mmu_cache_dc);
    gs::gs_param<int> p_mmu_cache_dc_linesize("linesize", 4, p_mmu_cache_dc);
    gs::gs_param<int> p_mmu_cache_dc_setsize("setsize", 8, p_mmu_cache_dc);
    gs::gs_param<bool> p_mmu_cache_dc_setlock("setlock", 1, p_mmu_cache_dc);
    gs::gs_param<bool> p_mmu_cache_dc_snoop("snoop", 1, p_mmu_cache_dc);
    gs::gs_param_array p_mmu_cache_ilram("ilram", p_mmu_cache);
    gs::gs_param<bool> p_mmu_cache_ilram_en("en", false, p_mmu_cache_ilram);
    gs::gs_param<unsigned int> p_mmu_cache_ilram_size("size", 0u, p_mmu_cache_ilram);
    gs::gs_param<unsigned int> p_mmu_cache_ilram_start("start", 0u, p_mmu_cache_ilram);
    gs::gs_param_array p_mmu_cache_dlram("dlram", p_mmu_cache);
    gs::gs_param<bool> p_mmu_cache_dlram_en("en", false, p_mmu_cache_dlram);
    gs::gs_param<unsigned int> p_mmu_cache_dlram_size("size", 0u, p_mmu_cache_dlram);
    gs::gs_param<unsigned int> p_mmu_cache_dlram_start("start", 0u, p_mmu_cache_dlram);
    gs::gs_param<unsigned int> p_mmu_cache_cached("cached", 0u, p_mmu_cache);
    gs::gs_param<unsigned int> p_mmu_cache_index("index", 0u, p_mmu_cache);
    gs::gs_param_array p_mmu_cache_mmu("mmu", p_mmu_cache);
    gs::gs_param<bool> p_mmu_cache_mmu_en("en", true, p_mmu_cache_mmu);
    gs::gs_param<unsigned int> p_mmu_cache_mmu_itlb_num("itlb_num", 8, p_mmu_cache_mmu);
    gs::gs_param<unsigned int> p_mmu_cache_mmu_dtlb_num("dtlb_num", 8, p_mmu_cache_mmu);
    gs::gs_param<unsigned int> p_mmu_cache_mmu_tlb_type("tlb_type", 0u, p_mmu_cache_mmu);
    gs::gs_param<unsigned int> p_mmu_cache_mmu_tlb_rep("tlb_rep", 1, p_mmu_cache_mmu);
    gs::gs_param<unsigned int> p_mmu_cache_mmu_mmupgsz("mmupgsz", 0u, p_mmu_cache_mmu);

    gs::gs_param<std::string> p_proc_history("history", "", p_system);

    gs::gs_param_array p_gdb("gdb", p_conf);
    gs::gs_param<bool> p_gdb_en("en", false, p_gdb);
    gs::gs_param<int> p_gdb_port("port", 1500, p_gdb);
    gs::gs_param<int> p_gdb_proc("proc", 0, p_gdb);
    for(uint32_t i=0; i< p_system_ncpu; i++) {
      // AHBMaster - MMU_CACHE
      // =====================
      // Always enabled.
      // Needed for basic platform.
      leon3_mmu_cache *leon3 = new leon3_mmu_cache(
              sc_core::sc_gen_unique_name("leon3_mmu_cache", false), // name of sysc module
              p_mmu_cache_ic_en,         //  int icen = 1 (icache enabled)
              p_mmu_cache_ic_repl,       //  int irepl = 0 (icache LRU replacement)
              p_mmu_cache_ic_sets,       //  int isets = 4 (4 instruction cache sets)
              p_mmu_cache_ic_linesize,   //  int ilinesize = 4 (4 words per icache line)
              p_mmu_cache_ic_setsize,    //  int isetsize = 16 (16kB per icache set)
              p_mmu_cache_ic_setlock,    //  int isetlock = 1 (icache locking enabled)
              p_mmu_cache_dc_en,         //  int dcen = 1 (dcache enabled)
              p_mmu_cache_dc_repl,       //  int drepl = 2 (dcache random replacement)
              p_mmu_cache_dc_sets,       //  int dsets = 2 (2 data cache sets)
              p_mmu_cache_dc_linesize,   //  int dlinesize = 4 (4 word per dcache line)
              p_mmu_cache_dc_setsize,    //  int dsetsize = 1 (1kB per dcache set)
              p_mmu_cache_dc_setlock,    //  int dsetlock = 1 (dcache locking enabled)
              p_mmu_cache_dc_snoop,      //  int dsnoop = 1 (dcache snooping enabled)
              p_mmu_cache_ilram_en,      //  int ilram = 0 (instr. localram disable)
              p_mmu_cache_ilram_size,    //  int ilramsize = 0 (1kB ilram size)
              p_mmu_cache_ilram_start,   //  int ilramstart = 8e (0x8e000000 default ilram start address)
              p_mmu_cache_dlram_en,      //  int dlram = 0 (data localram disable)
              p_mmu_cache_dlram_size,    //  int dlramsize = 0 (1kB dlram size)
              p_mmu_cache_dlram_start,   //  int dlramstart = 8f (0x8f000000 default dlram start address)
              p_mmu_cache_cached,        //  int cached = 0xffff (fixed cacheability mask)
              p_mmu_cache_mmu_en,        //  int mmu_en = 0 (mmu not present)
              p_mmu_cache_mmu_itlb_num,  //  int itlb_num = 8 (8 itlbs - not present)
              p_mmu_cache_mmu_dtlb_num,  //  int dtlb_num = 8 (8 dtlbs - not present)
              p_mmu_cache_mmu_tlb_type,  //  int tlb_type = 0 (split tlb mode - not present)
              p_mmu_cache_mmu_tlb_rep,   //  int tlb_rep = 1 (random replacement)
              p_mmu_cache_mmu_mmupgsz,   //  int mmupgsz = 0 (4kB mmu page size)>
              p_mmu_cache_index + i,     // Id of the AHB master
              p_report_power,            // Power Monitor,
              ambaLayer                  // TLM abstraction layer
      );

      // Connecting AHB Master
      leon3->ahb(ahbctrl.ahbIN);
      // Set clock
      leon3->set_clk(p_system_clock, SC_NS);
      connect(leon3->snoop, ahbctrl.snoop);

      // History logging
      std::string history = p_proc_history;
      if(!history.empty()) {
        leon3->g_history = history;
      }

      connect(irqmp.irq_req, leon3->cpu.IRQ_port.irq_signal, i);
      connect(leon3->cpu.irqAck.initSignal, irqmp.irq_ack, i);
      connect(leon3->cpu.irqAck.run, irqmp.cpu_rst, i);
      connect(leon3->cpu.irqAck.status, irqmp.cpu_stat, i);

      // GDBStubs
      if(p_gdb_en) {
        leon3->g_gdb = p_gdb_port;
      }
      // OS Emulator
      // ===========
      // is activating the leon traps to map basic io functions to the host system
      // set_brk, open, read, ...
      if(!((std::string)p_system_osemu).empty()) {
        leon3->g_osemu = p_system_osemu;
      }
      if(vm.count("argument")) {
        leon3->g_args = vm["arguments"].as<std::vector<std::string> >();
      }
    }

    // APBSlave - GPTimer
    // ==================
    gs::gs_param_array p_gptimer("gptimer", p_conf);
    gs::gs_param<bool> p_gptimer_en("en", true, p_gptimer);
    gs::gs_param<unsigned int> p_gptimer_ntimers("ntimers", 2, p_gptimer);
    gs::gs_param<unsigned int> p_gptimer_pindex("pindex", 3, p_gptimer);
    gs::gs_param<unsigned int> p_gptimer_paddr("paddr", 0x3, p_gptimer);
    gs::gs_param<unsigned int> p_gptimer_pmask("pmask", 0xfff, p_gptimer);
    gs::gs_param<unsigned int> p_gptimer_pirq("pirq", 8, p_gptimer);
    gs::gs_param<bool> p_gptimer_sepirq("sepirq", true, p_gptimer);
    gs::gs_param<unsigned int> p_gptimer_sbits("sbits", 16, p_gptimer);
    gs::gs_param<unsigned int> p_gptimer_nbits("nbits", 32, p_gptimer);
    gs::gs_param<unsigned int> p_gptimer_wdog("wdog", 1u, p_gptimer);

    if(p_gptimer_en) {
      GPTimer *gptimer = new GPTimer("gptimer",
        p_gptimer_ntimers,  // ntimers
        p_gptimer_pindex,   // index
        p_gptimer_paddr,    // paddr
        p_gptimer_pmask,    // pmask
        p_gptimer_pirq,     // pirq
        p_gptimer_sepirq,   // sepirq
        p_gptimer_sbits,    // sbits
        p_gptimer_nbits,    // nbits
        p_gptimer_wdog,     // wdog
        p_report_power      // powmon
      );

      // Connect to apb and clock
      apbctrl.apb(gptimer->bus);
      gptimer->set_clk(p_system_clock,SC_NS);

      // Connecting Interrupts
      for(int i=0; i < 8; i++) {
        signalkit::connect(irqmp.irq_in, gptimer->irq, p_gptimer_pirq + i);
      }

    }

    // APBSlave - APBUart
    // ==================
    gs::gs_param_array p_apbuart("apbuart", p_conf);
    gs::gs_param<bool> p_apbuart_en("en", false, p_apbuart);
    gs::gs_param<unsigned int> p_apbuart_index("index", 1, p_apbuart);
    gs::gs_param<unsigned int> p_apbuart_addr("addr", 0x001, p_apbuart);
    gs::gs_param<unsigned int> p_apbuart_mask("mask", 0xFFF, p_apbuart);
    gs::gs_param<unsigned int> p_apbuart_irq("irq", 2, p_apbuart);
    gs::gs_param<unsigned int> p_apbuart_type("type", 1, p_apbuart);
    gs::gs_param<unsigned int> p_apbuart_port("port", 2000, p_apbuart);
    int port = (unsigned int)p_apbuart_port;
    io_if *uart_io = NULL;
    if(p_apbuart_en) {
      switch(p_apbuart_type) {
        case 1:
          uart_io = new TcpIo(port);
          break;
        default:
          uart_io = new NullIO();
          break;
      }

      APBUART *apbuart = new APBUART(sc_core::sc_gen_unique_name("apbuart", true), uart_io,
        p_apbuart_index,           // index
        p_apbuart_addr,            // paddr
        p_apbuart_mask,            // pmask
        p_apbuart_irq,             // pirq
        p_report_power   // powmon
      );

      // Connecting APB Slave
      apbctrl.apb(apbuart->bus);
      // Connecting Interrupts
      signalkit::connect(irqmp.irq_in, apbuart->irq, p_apbuart_irq);
      // Set clock
      apbuart->set_clk(p_system_clock,SC_NS);
      // ******************************************
    }

    // AHBSlave - AHBProf
    // ==================
    gs::gs_param_array p_ahbprof("ahbprof", p_conf);
    gs::gs_param<bool> p_ahbprof_en("en", true, p_ahbprof);
    gs::gs_param<unsigned int> p_ahbprof_addr("addr", 0x900, p_ahbprof);
    gs::gs_param<unsigned int> p_ahbprof_mask("mask", 0xFFF, p_ahbprof);
    gs::gs_param<unsigned int> p_ahbprof_index("index", 6, p_ahbprof);
    if(p_ahbprof_en) {
      AHBProf *ahbprof = new AHBProf("ahbprof",
        p_ahbprof_index,  // index
        p_ahbprof_addr,   // paddr
        p_ahbprof_mask,   // pmask
        ambaLayer
      );

      // Connecting APB Slave
      ahbctrl.ahbOUT(ahbprof->ahb);
      ahbprof->set_clk(p_system_clock,SC_NS);
    }
#ifdef HAVE_SOCWIRE
    // CREATE AHB2Socwire bridge
    // =========================
    gs::gs_param_array p_socwire("socwire", p_conf);
    gs::gs_param<bool> p_socwire_en("en", false, p_socwire);
    gs::gs_param_array p_socwire_apb("apb", p_socwire);
    gs::gs_param<unsigned int> p_socwire_apb_addr("addr", 0x010, p_socwire_apb);
    gs::gs_param<unsigned int> p_socwire_apb_mask("mask", 0xFFF, p_socwire_apb);
    gs::gs_param<unsigned int> p_socwire_apb_index("index", 3, p_socwire_apb);
    gs::gs_param<unsigned int> p_socwire_apb_irq("irq", 10, p_socwire_apb);
    gs::gs_param_array p_socwire_ahb("ahb", p_socwire);
    gs::gs_param<unsigned int> p_socwire_ahb_index("index", 1, p_socwire_ahb);
    if(p_socwire_en) {
      AHB2Socwire *ahb2socwire = new AHB2Socwire("ahb2socwire",
        p_socwire_apb_addr,  // paddr
        p_socwire_apb_mask,  // pmask
        p_socwire_apb_index, // pindex
        p_socwire_apb_irq,   // pirq
        p_socwire_ahb_index, // hindex
        ambaLayer            // abstraction
      );

      // Connecting AHB Master
      ahb2socwire->ahb(ahbctrl.ahbIN);

      // Connecting APB Slave
      apbctrl.apb(ahb2socwire->apb);

      // Connecting Interrupts
      connect(irqmp.irq_in, ahb2socwire->irq, p_socwire_apb_irq);

      // Connect socwire ports as loopback
      ahb2socwire->socwire.master_socket(ahb2socwire->socwire.slave_socket);
    }
#endif
#ifdef HAVE_GRETH
  // ===========================================================
    // GREth Media Access Controller with EDCL support (AHBMaster)
    // ===========================================================
  GREthConfiguration *greth_config = new GREthConfiguration();
  // Load default configuration
    gs::gs_param_array p_greth("greth", p_conf);
    gs::gs_param<bool> p_greth_en("en", false, p_greth);
    gs::gs_param<uint8_t> p_greth_hindex("hindex",greth_config->hindex, p_greth);
    gs::gs_param<uint8_t> p_greth_pindex("pindex",greth_config->pindex, p_greth);
    gs::gs_param<uint16_t> p_greth_paddr("paddr",greth_config->paddr, p_greth);
    gs::gs_param<uint16_t> p_greth_pmask("pmask",greth_config->pmask, p_greth);
    gs::gs_param<uint32_t> p_greth_pirq("pirq",greth_config->pirq, p_greth);
    gs::gs_param<uint32_t> p_greth_memtech("memtech",greth_config->memtech, p_greth);
    gs::gs_param<uint16_t> p_greth_ifg_gap("ifg_gap",greth_config->ifg_gap, p_greth);
    gs::gs_param<uint16_t> p_greth_attempt_limit("attempt_limit",greth_config->attempt_limit, p_greth);
    gs::gs_param<uint16_t> p_greth_backoff_limit("backoff_limit",greth_config->backoff_limit, p_greth);
    gs::gs_param<uint16_t> p_greth_slot_time("slot_time",greth_config->slot_time, p_greth);
    gs::gs_param<uint16_t> p_greth_mdcscaler("mdcscaler",greth_config->mdcscaler, p_greth);
    gs::gs_param<bool> p_greth_enable_mdio("enable_mdio",greth_config->enable_mdio, p_greth);
    gs::gs_param<uint8_t> p_greth_fifosize("fifosize",greth_config->fifosize, p_greth);
    gs::gs_param<uint8_t> p_greth_nsync("nsync",greth_config->nsync, p_greth);
    gs::gs_param<uint8_t> p_greth_edcl("edcl",greth_config->edcl, p_greth);
    gs::gs_param<uint8_t> p_greth_edclbufsz("edclbufsz",greth_config->edclbufsz, p_greth);
    gs::gs_param<uint32_t> p_greth_macaddrh("macaddrh",greth_config->macaddrh, p_greth);
    gs::gs_param<uint32_t> p_greth_macaddrl("macaddrl",greth_config->macaddrl, p_greth);
    gs::gs_param<uint16_t> p_greth_ipaddrh("ipaddrh",greth_config->ipaddrh, p_greth);
    gs::gs_param<uint16_t> p_greth_ipaddrl("ipaddrl",greth_config->ipaddrl, p_greth);
    gs::gs_param<uint8_t> p_greth_phyrstadr("phyrstadr",greth_config->phyrstadr, p_greth);
    gs::gs_param<bool> p_greth_rmii("rmii",greth_config->rmii, p_greth);
    gs::gs_param<bool> p_greth_oepol("oepol",greth_config->oepol, p_greth);
    gs::gs_param<bool> p_greth_mdint_pol("mdint_pol",greth_config->mdint_pol, p_greth);
    gs::gs_param<bool> p_greth_enable_mdint("enable_mdint",greth_config->enable_mdint, p_greth);
    gs::gs_param<bool> p_greth_multicast("multicast",greth_config->multicast, p_greth);
    gs::gs_param<uint8_t> p_greth_ramdebug("ramdebug",greth_config->ramdebug, p_greth);
    gs::gs_param<uint8_t> p_greth_ehindex("ehindex",greth_config->ehindex, p_greth);
    gs::gs_param<bool> p_greth_edclsepahb("edclsepahb",greth_config->edclsepahb, p_greth);
    gs::gs_param<uint8_t> p_greth_mdiohold("mdiohold",greth_config->mdiohold, p_greth);
    gs::gs_param<uint16_t> p_greth_maxsize("maxsize",greth_config->maxsize, p_greth);
    gs::gs_param<int> p_greth_vphy_ctrl("vphy_ctrl",1 , p_greth);

    // Set custom configuration adaptions
    greth_config->hindex = p_greth_hindex;
    greth_config->pindex = p_greth_pindex;
    greth_config->paddr = p_greth_paddr;
    greth_config->pmask = p_greth_pmask;
    greth_config->pirq = p_greth_pirq;
    greth_config->memtech = p_greth_memtech;
    greth_config->ifg_gap = p_greth_ifg_gap;
    greth_config->attempt_limit = p_greth_attempt_limit;
    greth_config->backoff_limit = p_greth_backoff_limit;
    greth_config->slot_time = p_greth_slot_time;
    greth_config->mdcscaler = p_greth_mdcscaler;
    greth_config->enable_mdio = p_greth_enable_mdio;
    greth_config->fifosize = p_greth_fifosize;
    greth_config->nsync = p_greth_nsync;
    greth_config->edcl = p_greth_edcl;
    greth_config->edclbufsz = p_greth_edclbufsz;
    greth_config->macaddrh = p_greth_macaddrh;
    greth_config->macaddrl = p_greth_macaddrl;
    greth_config->ipaddrh = p_greth_ipaddrh;
    greth_config->ipaddrl = p_greth_ipaddrl;
    greth_config->phyrstadr = p_greth_phyrstadr;
    greth_config->rmii = p_greth_rmii;
    greth_config->oepol = p_greth_oepol;
    greth_config->mdint_pol = p_greth_mdint_pol;
    greth_config->enable_mdint = p_greth_enable_mdint;
    greth_config->multicast = p_greth_multicast;
    greth_config->ramdebug = p_greth_ramdebug;
    greth_config->ehindex = p_greth_ehindex;
    greth_config->edclsepahb = p_greth_edclsepahb;
    greth_config->mdiohold = p_greth_mdiohold;
    greth_config->maxsize = p_greth_maxsize;
    // Optional params for tap offset and post script
    if(vm.count("greth")) {
        std::vector<std::string> vec = vm["greth"].as< std::vector<std::string> >();
        for(std::vector<std::string>::iterator iter = vec.begin(); iter!=vec.end(); iter++) {
           std::string parname;
           std::string parvalue;

           // *** Check right format (parname=value)
           if(iter->find_first_of("=") == std::string::npos) {
               v::warn << "main" << "Option value in command line greth has no '='. Type '--help' for help. " << *iter;
           }
           // if not space before equal sign
           if(iter->find_first_of(" ") < iter->find_first_of("=")) {
               v::warn << "main" << "Option value in command line option may not contain a space before '='. " << *iter;
           }

           // Parse parameter name
           parname = iter->substr(0,iter->find_first_of("="));
           // Parse parameter value
           parvalue = iter->substr(iter->find_first_of("=")+1);

           // Set parameter
           if(parname.compare("tap.offset") == 0)
           {
               cout << "GRETH OTIONS:" << parname << " - " << parvalue << endl;
               TAP::deviceno = atoi(parvalue.c_str());
           }
           if(parname.compare("post.script") == 0)
           {
               cout << "GRETH OTIONS:" << parname << " - " << parvalue << endl;
               greth_script_path = parvalue.c_str();
           }
        }
    }

    GREth *greth;
    if(p_greth_en)  {
        MII *vphyLink;
        switch(p_greth_vphy_ctrl) {
        case 0:
          vphyLink = new Loopback();
          break;
        case 1:
        vphyLink = new Tap();
        break;
        //case 2:
        //vphyLink = new TrafGen();
        //break;
        default:
        v::info << "GREth.init" << "No VPHY defined -> choosing TAP as default." << v::endl;
        vphyLink = new Tap();
        break;
        }

      // Initialize the GREth with a virtual PHY
      // ==========================
      greth = new GREth("GREth",
            greth_config,   // CONFIG
            vphyLink,     // VPHY Controller
            ambaLayer,      // LT or AT (AT not supported at time)
            grethVPHYHook);   // Callback function, to init the TAP


      greth->set_clk(40, SC_NS);

      // Connect TLM buses
      // ==========================
      greth->ahb(ahbctrl.ahbIN);
      apbctrl.apb(greth->apb);

      // Connect IRQ
      // ==========================
      connect(irqmp.irq_in, greth->irq);
    }
  // GREth done. ==========================
#endif  // HAVE_GRETH
    // * Param Listing **************************
    paramprinter printer;
    if(paramlist) {
    	printer.printParams();
    	exit(0);
    }

    if(configlist){
      printer.printConfigs();
    	exit(0);
    }

    if(paramlistfiltered ){
      printer.printParams(optionssearchkey);
    	exit(0);
    }

    if(configlistfiltered ){
      printer.printConfigs(configssearchkey);
    	exit(0);
    }

    // ******************************************

    signalkit::signal_out<bool, Irqmp> irqmp_rst;
    connect(irqmp_rst, irqmp.rst);
    irqmp_rst.write(0);
    irqmp_rst.write(1);

    // * Power Monitor **************************
    //if(p_report_power) {
    //    new powermonitor("pow_mon");
    //}

    (void) signal(SIGINT, stopSimFunction);
    (void) signal(SIGTERM, stopSimFunction);
    (void) signal(10, stopSimFunction);

#ifdef HAVE_PYSC
    python.end_of_initialization();
#endif
    cstart = cend = clock();
    cstart = clock();
    //mtrace();
    sc_core::sc_start();
    //muntrace();
    cend = clock();

    v::info << "Summary" << "Start: " << dec << cstart << v::endl;
    v::info << "Summary" << "End:   " << dec << cend << v::endl;
    v::info << "Summary" << "Delta: " << dec << setprecision(4) << ((double)(cend - cstart) / (double)CLOCKS_PER_SEC * 1000) << "ms" << v::endl;

#ifdef HAVE_PYSC
    python.start_of_evaluation();
    python.end_of_evaluation();
#endif

    std::cout << "End of sc_main" << std::endl << std::flush;
    return trap::exitValue;
}
/// @}