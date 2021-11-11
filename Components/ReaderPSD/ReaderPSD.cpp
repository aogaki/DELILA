// -*- C++ -*-
/*!
 * @file
 * @brief
 * @date
 * @author
 *
 */

#include "ReaderPSD.h"

using DAQMW::FatalType::DATAPATH_DISCONNECTED;
using DAQMW::FatalType::OUTPORT_ERROR;
using DAQMW::FatalType::USER_DEFINED_ERROR1;

// Module specification
// Change following items to suit your component's spec.
static const char* reader_spec[] =
  {
   "implementation_id", "ReaderPSD",
   "type_name",         "ReaderPSD",
   "description",       "ReaderPSD component",
   "version",           "1.0",
   "vendor",            "Kazuo Nakayoshi, KEK",
   "category",          "example",
   "activity_type",     "DataFlowComponent",
   "max_instance",      "1",
   "language",          "C++",
   "lang_type",         "compile",
   ""
  };

ReaderPSD::ReaderPSD(RTC::Manager* manager)
  : DAQMW::DaqComponentBase(manager),
    m_OutPort("reader_out", m_out_data),
    m_recv_byte_size(0),
    m_out_status(BUF_SUCCESS),

    m_debug(false)
{
  // Registration: InPort/OutPort/Service

  // Set OutPort buffers
  registerOutPort("reader_out", m_OutPort);

  init_command_port();
  init_state_table();
  set_comp_name("READER");

  fData = new unsigned char[1024 * 1024 * 16];

  fConfigFile = "/DAQ/PSD.conf";
}

ReaderPSD::~ReaderPSD()
{
}

RTC::ReturnCode_t ReaderPSD::onInitialize()
{
  if (m_debug) {
    std::cerr << "ReaderPSD::onInitialize()" << std::endl;
  }

  return RTC::RTC_OK;
}

RTC::ReturnCode_t ReaderPSD::onExecute(RTC::UniqueId ec_id)
{
  daq_do();

  return RTC::RTC_OK;
}

int ReaderPSD::daq_dummy()
{
  return 0;
}

int ReaderPSD::daq_configure()
{
  std::cerr << "*** ReaderPSD::configure" << std::endl;

  ::NVList* paramList;
  paramList = m_daq_service0.getCompParams();
  parse_params(paramList);

  fDigitizer.reset(new TPSD);
  fDigitizer->LoadParameters(fConfigFile);
  fDigitizer->OpenDigitizers();
  fDigitizer->InitDigitizers();
  fDigitizer->UseFineTS();
  fDigitizer->AllocateMemory();

  return 0;
}

int ReaderPSD::parse_params(::NVList* list)
{
  std::cerr << "param list length:" << (*list).length() << std::endl;

  int len = (*list).length();
  for (int i = 0; i < len; i+=2) {
    std::string sname  = (std::string)(*list)[i].value;
    std::string svalue = (std::string)(*list)[i+1].value;

    std::cerr << "sname: " << sname << "  ";
    std::cerr << "value: " << svalue << std::endl;

    if(sname == "ConfigFile") {
      fConfigFile = svalue;
    } else if (sname == "StartModNo") {
      fStartModNo = std::stoi(svalue);
    }

  }

  return 0;
}

int ReaderPSD::daq_unconfigure()
{
  std::cerr << "*** ReaderPSD::unconfigure" << std::endl;
     
  fDigitizer->FreeMemory();
  fDigitizer->CloseDigitizers();

  return 0;
}

int ReaderPSD::daq_start()
{
  std::cerr << "*** ReaderPSD::start" << std::endl;

  m_out_status = BUF_SUCCESS;

  // usleep(1000);
   fDigitizer->Start();
   
  return 0;
}

int ReaderPSD::daq_stop()
{
  std::cerr << "*** ReaderPSD::stop" << std::endl;

  fDigitizer->Stop();
 
  return 0;
}

int ReaderPSD::daq_pause()
{
  std::cerr << "*** ReaderPSD::pause" << std::endl;

  return 0;
}

int ReaderPSD::daq_resume()
{
  std::cerr << "*** ReaderPSD::resume" << std::endl;

  return 0;
}

int ReaderPSD::read_data_from_detectors()
{
  int received_data_size = 0;
  /// write your logic here

  constexpr auto maxSize = 2000000; // < 2MB(2 * 1024 * 1024)
  
  constexpr auto sizeMod = sizeof(PSDData::ModNumber);
  constexpr auto sizeCh = sizeof(PSDData::ChNumber);
  constexpr auto sizeTS = sizeof(PSDData::TimeStamp);
  constexpr auto sizeEne = sizeof(PSDData::ChargeLong);
  constexpr auto sizeRL = sizeof(PSDData::RecordLength);
  
  fDigitizer->ReadEvents();
  auto data = fDigitizer->GetData();
  // std::cout << data->size() << std::endl;
  if(data->size() > 0) {
    const auto oneHitSize = sizeMod + sizeCh + sizeTS + sizeEne + sizeRL
      + (sizeof(*(PSDData::Trace1)) * data->at(0)->RecordLength);
    
    const auto nData = data->size();
    auto index = 0;
    for(auto i = 0; i < nData; i++) {
      if(received_data_size + oneHitSize > maxSize) break;

      unsigned char mod = data->at(i)->ModNumber + fStartModNo;
      memcpy(&fData[index], &(mod), sizeMod);
      index += sizeMod;
      received_data_size += sizeMod;

      memcpy(&fData[index], &(data->at(i)->ChNumber), sizeCh);
      index += sizeCh;
      received_data_size += sizeCh;

      memcpy(&fData[index], &(data->at(i)->TimeStamp), sizeTS);
      index += sizeTS;
      received_data_size += sizeTS;

      memcpy(&fData[index], &(data->at(i)->ChargeLong), sizeEne);
      index += sizeEne;
      received_data_size += sizeEne;

      memcpy(&fData[index], &(data->at(i)->RecordLength), sizeRL);
      index += sizeRL;
      received_data_size += sizeRL;
      
      const auto sizeTrace = sizeof(*(PSDData::Trace1)) * data->at(i)->RecordLength;
      memcpy(&fData[index], data->at(i)->Trace1, sizeTrace);
      index += sizeTrace;
      received_data_size += sizeTrace;    
    }
  }
  
  return received_data_size;
}

int ReaderPSD::set_data(unsigned int data_byte_size)
{
  unsigned char header[8];
  unsigned char footer[8];

  set_header(&header[0], data_byte_size);
  set_footer(&footer[0]);

  ///set OutPort buffer length
  m_out_data.data.length(data_byte_size + HEADER_BYTE_SIZE + FOOTER_BYTE_SIZE);
  memcpy(&(m_out_data.data[0]), &header[0], HEADER_BYTE_SIZE);
  memcpy(&(m_out_data.data[HEADER_BYTE_SIZE]), &fData[0], data_byte_size);
  memcpy(&(m_out_data.data[HEADER_BYTE_SIZE + data_byte_size]), &footer[0],
	 FOOTER_BYTE_SIZE);

  return 0;
}

int ReaderPSD::write_OutPort()
{
  ////////////////// send data from OutPort  //////////////////
  bool ret = m_OutPort.write();

  //////////////////// check write status /////////////////////
  if (ret == false) {  // TIMEOUT or FATAL
    m_out_status  = check_outPort_status(m_OutPort);
    if (m_out_status == BUF_FATAL) {   // Fatal error
      fatal_error_report(OUTPORT_ERROR);
    }
    if (m_out_status == BUF_TIMEOUT) { // Timeout
      return -1;
    }
  }
  else {
    m_out_status = BUF_SUCCESS; // successfully done
  }

  return 0;
}

int ReaderPSD::daq_run()
{
  if (m_debug) {
    std::cerr << "*** ReaderPSD::run" << std::endl;
  }

  if (check_trans_lock()) {  // check if stop command has come
    set_trans_unlock();    // transit to CONFIGURED state
    return 0;
  }

  // for(auto i = 0; i < 2; i++) fDigitizer->SendSWTrigger();
  
  if (m_out_status == BUF_SUCCESS) {   // previous OutPort.write() successfully done
    m_recv_byte_size = read_data_from_detectors();
    // std::cout << m_recv_byte_size << std::endl;
    if (m_recv_byte_size > 0) {
      set_data(m_recv_byte_size); // set data to OutPort Buffer
    } else {
      return 0;
    }
  }

  if (write_OutPort() < 0) {
    ;     // Timeout. do nothing.
  }
  else if(m_recv_byte_size > 0) {    // OutPort write successfully done
    inc_sequence_num();                    // increase sequence num.
    inc_total_data_size(m_recv_byte_size);  // increase total data byte size
  }

  return 0;
}

extern "C"
{
  void ReaderPSDInit(RTC::Manager* manager)
  {
    RTC::Properties profile(reader_spec);
    manager->registerFactory(profile,
			     RTC::Create<ReaderPSD>,
			     RTC::Delete<ReaderPSD>);
  }
};