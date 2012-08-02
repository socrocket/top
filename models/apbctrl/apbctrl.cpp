// ********************************************************************
// Copyright 2010, Institute of Computer and Network Engineering,
//                 TU-Braunschweig
// All rights reserved
// Any reproduction, use, distribution or disclosure of this program,
// without the express, prior written consent of the authors is
// strictly prohibited.
//
// University of Technology Braunschweig
// Institute of Computer and Network Engineering
// Hans-Sommer-Str. 66
// 38118 Braunschweig, Germany
//
// ESA SPECIAL LICENSE
//
// This program may be freely used, copied, modified, and redistributed
// by the European Space Agency for the Agency's own requirements.
//
// The program is provided "as is", there is no warranty that
// the program is correct or suitable for any purpose,
// neither implicit nor explicit. The program and the information in it
// contained do not necessarily reflect the policy of the
// European Space Agency or of TU-Braunschweig.
// ********************************************************************
// Title:      apbctrl.cpp
//
// ScssId:
//
// Origin:     HW-SW SystemC Co-Simulation SoC Validation Platform
//
// Purpose:    Implementation of the AHB APB Bridge
//
// Method:
//
// Modified on $Date$
//          at $Revision$
//          by $Author$
//
// Principal:  European Space Agency
// Author:     VLSI working group @ IDA @ TUBS
// Maintainer: Thomas Schuster
// Reviewed:
// ********************************************************************

#include "apbctrl.h"
#include "verbose.h"
#include "vendian.h"


/// Constructor of class APBCtrl
APBCtrl::APBCtrl(sc_core::sc_module_name nm, // SystemC name
		 uint32_t haddr_,            // The MSB address of the AHB area. Sets the 12 MSBs in the AHB address
                 uint32_t hmask_,            // The 12bit AHB area address mask
		 bool mcheck,                // Check if there are any intersections between APB slave memory regions
		 uint32_t hindex,            // AHB bus index
		 bool pow_mon,               // Enable power monitoring
		 amba::amba_layer_ids ambaLayer) :
  AHBSlave<>(nm,
             hindex,
             0x01, // vendor_id: Gaisler
             0x006, // device_id: APBCTRL (p. 92 GRIP)
             0,
             0,
             ambaLayer,
             BAR(AHBDevice::AHBMEM, hmask_, 0, 0, haddr_)),
       
  apb("apb", amba::amba_APB, amba::amba_LT, false),
 
  m_AcceptPEQ("AcceptPEQ"),
  m_TransactionPEQ("TransactionPEQ"),
  m_pnpbase(0xFF000), 
  m_haddr(haddr_),
  m_hmask(hmask_),
  m_mcheck(mcheck),
  m_pow_mon(pow_mon),
  m_ambaLayer(ambaLayer),
  m_total_transactions("total_transactions", 0ull, m_performance_counters),
  m_right_transactions("successful_transactions", 0ull, m_performance_counters),
  sta_power_norm("power.apbctrl.sta_power_norm", 0.0, true), // Normalized static power input
  dyn_power_norm("power.apbctrl.dyn_power_norm", 0.0, true), // Normalized dyn power input (activation indep.)
  dyn_read_energy_norm("power.apbctrl.dyn_read_energy_norm", 0.0, true), // Normalized read energy input
  dyn_write_energy_norm("power.apbctrl.dyn_write_energy_norm", 0.0, true), // Normalized write energy input
  power("power"),
  sta_power("sta_power", 0.0, power), // Static power output
  dyn_power("dyn_power", 0.0, power), // Dynamic power output
  dyn_read_energy("dyn_read_energy", 0.0, power), // Energy per read access
  dyn_write_energy("dyn_write_energy", 0.0, power), // Energy per write access
  dyn_reads("dyn_reads", 0ull, power), // Read access counter for power computation
  dyn_writes("dyn_writes", 0ull, power) // Write access counter for power computation

 {

    // Assert generics are withing allowed ranges
    assert(haddr_ <= 0xfff);
    assert(hmask_ <= 0xfff);

    // Register power monitor
    PM::registerIP(this, "apbctrl", m_pow_mon);
    PM::send_idle(this, "idle", sc_time_stamp(), m_pow_mon);

    v::info << name() << " ***********************************************************************" << v::endl;
    v::info << name() << " * Created APBCTRL with following parameters: " << v::endl;
    v::info << name() << " * ------------------------------------------ " << v::endl;
    v::info << name() << " * haddr/hmask: " << hex << haddr_ << "/" << hmask_ << v::endl;
    v::info << name() << " * mcheck: " << mcheck << v::endl;
    v::info << name() << " * hindex: " << hindex << v::endl;
    v::info << name() << " * pow_mon: " << pow_mon << v::endl;
    v::info << name() << " * ambaLayer (LT = 8 / AT = 4): " << ambaLayer << v::endl;
    v::info << name() << " ***********************************************************************" << v::endl;
}

// Do reset
void APBCtrl::dorst() {

}

// Destructor
APBCtrl::~APBCtrl() {

}

/// Helper function for creating slave map decoder entries
void APBCtrl::setAddressMap(const uint32_t binding, const uint32_t pindex, const uint32_t paddr, const uint32_t pmask) {

  // Create slave map entry from slave ID and address range descriptor (slave_info_t)
  slave_info_t tmp;

  tmp.pindex = pindex;
  tmp.paddr = paddr;
  tmp.pmask  = pmask;

  slave_map.insert(std::pair<uint32_t, slave_info_t>(binding, tmp));
}

/// Find slave index by address
int APBCtrl::get_index(const uint32_t address) {

  // Use 12 bit segment address for decoding
  uint32_t addr = (address >> 8) & 0xfff;

  for (it = slave_map.begin(); it != slave_map.end(); it++) {

    slave_info_t info = it->second;

    if (((addr ^ info.paddr) & info.pmask) == 0) {

      // APB: Device == BAR)
      m_right_transactions++;
      return(it->first);

    }

  }

  // no slave found
  return -1;
}

// Returns a PNP register from the APB configuration area (upper 4kb of address space)
uint32_t APBCtrl::getPNPReg(const uint32_t address) {

  // Calculate address offset in configuration area
  uint32_t addr = address - (get_bar_addr(0) + m_pnpbase);
  // Calculate index of the device in mSlaves pointer array (8 byte per device)
  uint32_t device = (addr >> 2) >> 1;
  // Calculate offset within device information
  uint32_t offset = (addr >> 2) & 0x1;

  if((device < 16) && (mSlaves[device] != NULL)) {
      m_right_transactions++;
      uint32_t result = mSlaves[device][offset];
      #ifdef LITTLE_ENDIAN_BO
      swap_Endianess(result);
      #endif
      return result;
  } else {
    v::debug << name() << "Access to not existing PNP Register!" << v::endl;
      return 0;
  }

}

// Functional part of the model (decoding logic)
uint32_t APBCtrl::exec_func(tlm::tlm_generic_payload & ahb_gp, sc_time &delay, bool debug) {

  m_total_transactions++;
  transport_statistics(ahb_gp);
  
  // Extract data pointer from payload
  uint8_t *data = ahb_gp.get_data_ptr();
  // Extract address from payload
  uint32_t addr = ahb_gp.get_address();
  // Extract length from payload
  uint32_t length = ahb_gp.get_data_length();

  // Is this an access to the configuration area
  // The configuration area is always in the upper 0xFF000 area
  if(((addr ^ m_pnpbase) & m_pnpbase) == 0) {
    // Configuration area is read only

    if(ahb_gp.get_command() == tlm::TLM_READ_COMMAND) {

      //addr = addr - ((m_pnpbase) & (m_pnpbase));
      for(uint32_t i = 0; i < length; i++) {

        //uint32_t word = (addr + i) >> 2;
        uint32_t byte = (addr + i) & 0x3;
        uint32_t reg = getPNPReg(addr + i);
        
        data[i] = ((uint8_t *)&reg)[byte];

        delay += clock_cycle;
      }

      ahb_gp.set_response_status(tlm::TLM_OK_RESPONSE);
   
    } else {
    
      v::error << name() << " Forbidden write to APBCTRL configuration area (PNP)!" << v::endl;
      ahb_gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);

    }

    return ahb_gp.get_data_length();

  }

  // Find slave by address / returns slave index or -1 for not mapped
  int index = get_index(addr);  

  // For valid slave index
  if(index >= 0) {

    // -- For Debug only --
    uint32_t a = 0;
    socket_t *other_socket = apb.get_other_side(index, a);
    sc_core::sc_object *obj = other_socket->get_parent();

    v::debug << name() << "Forwarding request to APB slave:" << obj->name()
             << "@0x" << hex << v::setfill('0') << v::setw(8)
             << (ahb_gp.get_address() & 0x000fffff) << endl;
    // --------------------

    // Take APB transaction from pool
    payload_t *apb_gp = apb.get_transaction();

    // Build APB transaction from incoming AHB transaction:
   // ----------------------------------------------------
    apb_gp->set_command(ahb_gp.get_command());
    // Substract the base address of the bridge
    apb_gp->set_address(ahb_gp.get_address() & 0x000fffff);
    apb_gp->set_data_length(ahb_gp.get_data_length());
    apb_gp->set_byte_enable_ptr(ahb_gp.get_byte_enable_ptr());
    apb_gp->set_data_ptr(ahb_gp.get_data_ptr());

    if (!debug) {

      // Power event start
      PM::send(this,"apb_trans", 1, sc_time_stamp(), (unsigned int)apb_gp->get_data_ptr(), m_pow_mon);

      // Forward request to the selected slave
      apb[index]->b_transport(*apb_gp, delay);

      // Add delay for APB setup cycle
      delay += clock_cycle;

      // Power event end
      PM::send(this,"apb_trans", 0, sc_time_stamp()+delay, (unsigned int)apb_gp->get_data_ptr(), m_pow_mon);

    } else {

      apb[index]->transport_dbg(*apb_gp);

    }

    // Copy back response message
    ahb_gp.set_response_status(apb_gp->get_response_status());

    // Release transaction
    apb.release_transaction(apb_gp);

  } else {

    v::error << name() << "Access to unmapped APB address space at address " << v::uint32 << addr << endl;
    ahb_gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
  }

  return ahb_gp.get_data_length();

}

/// Set up slave map and collect plug & play information
void APBCtrl::start_of_simulation() {

  // Get number of bindings at master socket (number of connected slaves)
  uint32_t num_of_bindings = apb.size();

  // max. 16 APB slaves allowed
  assert(num_of_bindings<=16);

  v::info << name() << "******************************************************************************* " << v::endl;
  v::info << name() << "* APB DECODER INITIALIZATION " << v::endl;
  v::info << name() << "* -------------------------- " << v::endl;

  // iterate the registered slaves
  for (uint32_t i = 0; i < num_of_bindings; i++) {

    uint32_t a = 0;

    // get pointer to socket of slave i
    socket_t *other_socket = apb.get_other_side(i, a);

    // get parent object containing slave socket i
    sc_core::sc_object *obj = other_socket->get_parent();

    // valid slaves implement the APBDevice interface
    APBDevice *slave = dynamic_cast<APBDevice *> (obj);

    v::info << name() << "* Slave name: " << obj->name() << v::endl;

    // slave is valid (implements APBDevice)
    if(slave) {

      // Get pointer to device information
      const uint32_t *deviceinfo = slave->get_device_info();

      // Get slave id (pindex)
      const uint32_t sbusid = slave->get_busid();

      // Map device information into PNP region
      mSlaves[sbusid] = deviceinfo;

      // check 'type'filed of bar[i] (must be != 0)
      if (slave->get_type()) {

  	// get base address and mask from BAR
        uint32_t addr = slave->get_base();
        uint32_t mask = slave->get_mask();

	v::info << name() << "* BAR with MSB addr: " << hex << addr << " and mask: " << mask << v::endl;

	// insert slave region into memory map
        setAddressMap(i, sbusid, addr, mask);

      }

    } else {

       v::warn << name() << "Slave bound to socket 'apb' is not a valid APBDevice." << v::endl;
       assert(0);

    }
  }

  // End of decoder initialization
  v::info << name() << "******************************************************************************* " << v::endl;

  // Check memory map for overlaps
  if(m_mcheck) {
      checkMemMap();
  }
}

// Print execution statistic at end of simulation
void APBCtrl::end_of_simulation() {
    v::report << name() << " ********************************************" << v::endl;
    v::report << name() << " * APBCtrl Statistic:" << v::endl;
    v::report << name() << " * ------------------" << v::endl;
    v::report << name() << " * Successful Transactions: " << m_right_transactions << v::endl;
    v::report << name() << " * Total Transactions:      " << m_total_transactions << v::endl;
    v::report << name() << " *  " << v::endl;
    v::report << name() << " * AHB Slave interface reports: " << v::endl;
    print_transport_statistics(name());
    v::report << name() << " ******************************************** " << v::endl;
}

sc_core::sc_time APBCtrl::get_clock() {

  return clock_cycle;

}

struct apb_check_slave_type {
    uint32_t start;
    uint32_t end;
    uint32_t index;
};

// Check the memory map for overlaps
void APBCtrl::checkMemMap() {
   map<uint32_t, apb_check_slave_type> slaves;
   typedef map<uint32_t, apb_check_slave_type>::iterator iter_t;
   struct apb_check_slave_type last;
   last.start = 0;
   last.end = 0;
   last.index = ~0;

   for(slave_iter iter = slave_map.begin(); iter!=slave_map.end(); iter++) {
       uint32_t start_addr = (iter->second.paddr & iter->second.pmask) << 12;
       uint32_t size = ((~iter->second.pmask & 0xFFF) + 1) << 12;
       struct apb_check_slave_type obj;
       obj.start = start_addr;
       obj.end = start_addr + size -1;
       obj.index = iter->second.pindex;
       slaves.insert(make_pair(start_addr, obj));
   }
   for(iter_t iter=slaves.begin(); iter != slaves.end(); iter++) {
      // First Slave need it in last to start 
      if(last.index!=~0u) {
          // All other elements
          // See if the last element is begining and end befor the current
          if(last.start>=iter->second.start || last.end >= iter->second.start) {
              uint32_t a = 0;
              socket_t *other_socket = apb.get_other_side(last.index, a);
              sc_core::sc_object *obj = other_socket->get_parent();

              other_socket = apb.get_other_side(iter->second.index, a);
              sc_core::sc_object *obj2 = other_socket->get_parent();

              v::error << name() << "Overlap in AHB memory mapping." << v::endl;
              v::error << name() << obj->name() << " " << v::uint32 << last.start << " - " << v::uint32 << last.end << endl;
              v::error << name() << obj2->name() << " " << v::uint32 << iter->second.start << " - " << v::uint32 << iter->second.end << endl;
          }
      }
      last = iter->second;
  }
}

