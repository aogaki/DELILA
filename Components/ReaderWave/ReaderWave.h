// -*- C++ -*-
/*!
 * @file
 * @brief
 * @date
 * @author
 *
 */

#ifndef READER_H
#define READER_H

#include <deque>
#include <memory>
#include <string>

#include "../../TDigiTES/include/TDigiTes.hpp"
#include "../../TDigiTES/include/TPHA.hpp"
#include "../../TDigiTES/include/TPHAData.hpp"
#include "../../TDigiTES/include/TPSD.hpp"
#include "../../TDigiTES/include/TPSDData.hpp"
#include "../include/TDataContainer.hpp"
#include "DaqComponentBase.h"

using namespace RTC;

class ReaderWave : public DAQMW::DaqComponentBase
{
 public:
  ReaderWave(RTC::Manager *manager);
  ~ReaderWave();

  // The initialize action (on CREATED->ALIVE transition)
  // former rtc_init_entry()
  virtual RTC::ReturnCode_t onInitialize();

  // The execution action that is invoked periodically
  // former rtc_active_do()
  virtual RTC::ReturnCode_t onExecute(RTC::UniqueId ec_id);

 private:
  TimedOctetSeq m_out_data;
  OutPort<TimedOctetSeq> m_OutPort;

 private:
  int daq_dummy();
  int daq_configure();
  int daq_unconfigure();
  int daq_start();
  int daq_run();
  int daq_stop();
  int daq_pause();
  int daq_resume();

  int parse_params(::NVList *list);
  int read_data_from_detectors();
  int set_data();
  int write_OutPort();

  static const int SEND_BUFFER_SIZE = 0;
  unsigned char m_data[SEND_BUFFER_SIZE];
  unsigned int m_recv_byte_size;

  BufferStatus m_out_status;
  bool m_debug;

  // Digitizer
  std::unique_ptr<TPSD> fDigitizer;
  unsigned char *fData;
  std::deque<PSDData_t> fQue;
  std::string fConfigFile;
  int fStartModNo = 0;

  TDataContainer fDataContainer;
};

extern "C" {
void ReaderWaveInit(RTC::Manager *manager);
};

#endif  // READER_H
