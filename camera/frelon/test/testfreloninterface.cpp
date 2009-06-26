#include "FrelonInterface.h"
#include "PoolThreadMgr.h"
#include "CtSaving.h"
#include "Data.h"

#include <iostream>

using namespace lima;
using namespace std;

class TestFrameCallback : public HwFrameCallback
{
public:
	TestFrameCallback(Frelon::Interface& hw_inter, CtSaving& buffer_save,
			  Cond& acq_finished) 
		: m_hw_inter(hw_inter), m_buffer_save(buffer_save), 
		  m_acq_finished(acq_finished) {}
protected:
	virtual bool newFrameReady(const HwFrameInfoType& frame_info);
private:
	Frelon::Interface& m_hw_inter;
	CtSaving& m_buffer_save;
	Cond& m_acq_finished;
};

bool TestFrameCallback::newFrameReady(const HwFrameInfoType& frame_info)
{
	int nb_acq_frames = m_hw_inter.getNbAcquiredFrames();
	HwInterface::Status status;
	m_hw_inter.getStatus(status);

	cout << "In callback:" << endl
	     << "  frame_info=" << frame_info << endl
	     << "  nb_acq_frames=" << nb_acq_frames << endl
	     << "  status=" << status << endl;

	Data aNewData = Data();
	aNewData.frameNumber = frame_info.acq_frame_nb;
	const Size &aSize = frame_info.frame_dim->getSize();
	aNewData.width = aSize.getWidth();
	aNewData.height = aSize.getHeight();
	aNewData.type = Data::UINT16;
	
	Buffer *aNewBuffer = new Buffer();
	aNewBuffer->owner = Buffer::MAPPED;
	aNewBuffer->data = (void*)frame_info.frame_ptr;
	aNewData.setBuffer(aNewBuffer);
	aNewBuffer->unref();

	m_buffer_save.frameReady(aNewData);

	HwSyncCtrlObj *hw_sync;
	m_hw_inter.getHwCtrlObj(hw_sync);
	int nb_frames;
	hw_sync->getNbFrames(nb_frames);
	if (frame_info.acq_frame_nb == nb_frames - 1)
		m_acq_finished.signal();

	return true;
}

void print_status(Frelon::Interface& hw_inter)
{
	HwInterface::Status status;

	hw_inter.getStatus(status);
	cout << "status=" << status << endl;
}

void test_frelon_hw_inter(bool do_reset)
{
	Espia::Dev dev(0);
	Espia::Acq acq(dev);
	Espia::BufferMgr buffer_cb_mgr(acq);
	Espia::SerialLine ser_line(dev);
	Frelon::Camera cam(ser_line);
	BufferCtrlMgr buffer_mgr(buffer_cb_mgr);

	cout << "Creating the Hw Interface ... " << endl;
	Frelon::Interface hw_inter(acq, buffer_mgr, cam);
	cout << " Done!" << endl;

	CtControl aControl(NULL);
	CtSaving buffer_save(aControl);
	CtSaving::Parameters saving_par;
	saving_par.directory = ".";
	saving_par.prefix = "img";
	saving_par.suffix = ".edf";
	saving_par.nextNumber = 0;
	saving_par.savingMode = CtSaving::AutoFrame;
	saving_par.overwritePolicy = CtSaving::Overwrite;
	saving_par.framesPerFile = 1;
	buffer_save.setParameters(saving_par);

	Cond acq_finished;
	TestFrameCallback cb(hw_inter, buffer_save, acq_finished);

	HwDetInfoCtrlObj *hw_det_info;
	hw_inter.getHwCtrlObj(hw_det_info);

	HwBufferCtrlObj *hw_buffer;
	hw_inter.getHwCtrlObj(hw_buffer);

	HwSyncCtrlObj *hw_sync;
	hw_inter.getHwCtrlObj(hw_sync);

	HwBinCtrlObj *hw_bin;
	hw_inter.getHwCtrlObj(hw_bin);

	if (do_reset) {
		cout << "Reseting the hardware ... " << endl;
		hw_inter.reset(HwInterface::HardReset);
		cout << "  Done!" << endl;
	}

	Size size;
	hw_det_info->getMaxImageSize(size);
	ImageType image_type;
	hw_det_info->getCurrImageType(image_type);
	FrameDim frame_dim(size, image_type);

	Bin bin(1);
	hw_bin->setBin(bin);

	FrameDim effect_frame_dim = frame_dim / bin;
	hw_buffer->setFrameDim(effect_frame_dim);
	hw_buffer->setNbBuffers(10);
	hw_buffer->registerFrameCallback(cb);

	print_status(hw_inter);
	hw_inter.startAcq();
	acq_finished.wait();
	print_status(hw_inter);
	hw_inter.stopAcq();
	print_status(hw_inter);

	hw_sync->setExpTime(5);

	print_status(hw_inter);
	hw_inter.startAcq();
	acq_finished.wait();
	print_status(hw_inter);
	hw_inter.stopAcq();
	print_status(hw_inter);

	hw_sync->setExpTime(2);
	hw_sync->setNbFrames(3);

	print_status(hw_inter);
	hw_inter.startAcq();
	acq_finished.wait();
	print_status(hw_inter);
	hw_inter.stopAcq();
	print_status(hw_inter);

	bool raised_exception = false;
	try {
		bin = Bin(16, 16);
		hw_bin->setBin(bin);
	} catch (Exception) {
		raised_exception = true;
	}
	if (raised_exception)
		cout << "Exception was raised OK!" << endl;
	else
		throw LIMA_HW_EXC(Error, "Did not raise bad bin exception");

	bin = Bin(2, 2);
	hw_bin->setBin(bin);
	effect_frame_dim = frame_dim / bin;
	hw_buffer->setFrameDim(effect_frame_dim);
	hw_buffer->setNbBuffers(10);

	print_status(hw_inter);
	hw_inter.startAcq();
	acq_finished.wait();
	PoolThreadMgr::get().wait();
	print_status(hw_inter);
	hw_inter.stopAcq();
	print_status(hw_inter);
}

int main(int argc, char *argv[])
{
	try {
		bool do_reset = (argc > 1) && (string(argv[1]) == "reset");
		test_frelon_hw_inter(do_reset);
	} catch (Exception e) {
		cerr << "LIMA Exception:" << e << endl;
	}

	return 0;
}
