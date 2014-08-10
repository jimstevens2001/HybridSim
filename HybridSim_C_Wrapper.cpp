
#include "HybridSystem.h"

using namespace std;

namespace HybridSim {

// Extra functions for C interface (used by Python front end)
struct HybridSim_C_Wrapper
{
	public:

	HybridSystem *hs;

	void (*readDone)(uint, uint64_t, uint64_t);
	void (*writeDone)(uint, uint64_t, uint64_t);
	void (*notifyCB)(uint, uint64_t, uint64_t);

	HybridSim_C_Wrapper(uint id, char *ini)
	{
		// Create a HybridSystem object.
		// Note ini is implicitly transformed to C++ string type.
		hs = getMemorySystemInstance(id, ini);

		// Register callbacks to receive messages from HybridSystem instance.
		typedef CallbackBase<void,uint,uint64_t,uint64_t> Callback_t;
		Callback_t *read_cb = new Callback<HybridSim_C_Wrapper, void, uint, uint64_t, uint64_t>(this, &HybridSim_C_Wrapper::read_complete);
		Callback_t *write_cb = new Callback<HybridSim_C_Wrapper, void, uint, uint64_t, uint64_t>(this, &HybridSim_C_Wrapper::write_complete);
		hs->RegisterCallbacks(read_cb, write_cb);

		// Init external C callback pointers.
		readDone = NULL;
		writeDone = NULL;

		// Set up notify callback in the same way.
		Callback_t *notify_cb = new Callback<HybridSim_C_Wrapper, void, uint, uint64_t, uint64_t>(this, &HybridSim_C_Wrapper::notify_callback);
		hs->RegisterNotifyCallback(notify_cb);
		notifyCB = NULL;
	}

	~HybridSim_C_Wrapper()
	{
		delete hs;
	}

	void RegisterCallbacks(void (*readDone)(uint, uint64_t, uint64_t), void (*writeDone)(uint, uint64_t, uint64_t))
	{
		cout << "Registering C callbacks in HybridSim_C_Wrapper::RegisterCallbacks\n";
		//cout << "Address is " << (uint64_t)readDone << ", " << (uint64_t)writeDone << "\n";
		this->readDone = readDone;
		this->writeDone = writeDone;
	}

	void RegisterNotifyCallback(void (*notifyCB)(uint, uint64_t, uint64_t))
	{
		cout << "Registering C notify callback in HybridSim_C_Wrapper::RegisterNotifyCallback\n";
		this->notifyCB = notifyCB;
	}

	void read_complete(uint id, uint64_t address, uint64_t cycle)
	{
		if (readDone != NULL)
		{
			//cout << "Calling HybridSim C read callback!\n";
			readDone(id, address, cycle);
		}
	}

	void write_complete(uint id, uint64_t address, uint64_t cycle)
	{
		if (writeDone != NULL)
		{
			//cout << "Calling HybridSim C write callback!\n";
			writeDone(id, address, cycle);
		}
	}

	void notify_callback(uint operation, uint64_t address, uint64_t cycle)
	{
		if (notifyCB != NULL)
		{
			//cout << "Calling HybridSim C notify callback!\n";
			notifyCB(operation, address, cycle);
		}
	}
};

typedef struct HybridSim_C_Wrapper HybridSim_C_Wrapper_t;

extern "C"
{
	HybridSim_C_Wrapper_t *HybridSim_C_getMemorySystemInstance(uint id, char *ini)
	{
		HybridSim_C_Wrapper_t *hsc = new HybridSim_C_Wrapper(id, ini);

		return hsc;
	}

	void HybridSim_C_RegisterCallbacks(HybridSim_C_Wrapper_t *hsc, void (*readDone)(uint, uint64_t, uint64_t), void (*writeDone)(uint, uint64_t, uint64_t))
	{
		cout << "Registering C callbacks in HybridSim_C_RegisterCallbacks\n";
		hsc->RegisterCallbacks(readDone, writeDone);
	}

	bool HybridSim_C_addTransaction(HybridSim_C_Wrapper_t *hsc, bool isWrite, uint64_t addr)
	{
		//cout << "C interface... addr=" << addr << " isWrite=" << isWrite << "\n";
		return hsc->hs->addTransaction(isWrite, addr);
	}

	bool HybridSim_C_WillAcceptTransaction(HybridSim_C_Wrapper_t *hsc)
	{
		return hsc->hs->WillAcceptTransaction();
	}

	void HybridSim_C_update(HybridSim_C_Wrapper_t *hsc)
	{
		hsc->hs->update();
	}

	void HybridSim_C_mmio(HybridSim_C_Wrapper_t *hsc, uint64_t operation, uint64_t address)
	{
		hsc->hs->mmio(operation, address);
	}

	void HybridSim_C_syncAll(HybridSim_C_Wrapper_t *hsc)
	{
		hsc->hs->syncAll();
	}

	void HybridSim_C_reportPower(HybridSim_C_Wrapper_t *hsc)
	{
		hsc->hs->reportPower();
	}

	void HybridSim_C_printLogfile(HybridSim_C_Wrapper_t *hsc)
	{
		hsc->hs->printLogfile();
	}

	void HybridSim_C_RegisterNotifyCallback(HybridSim_C_Wrapper_t *hsc, void (*notify)(uint, uint64_t, uint64_t))
	{
		hsc->RegisterNotifyCallback(notify);
	}

	void HybridSim_C_ConfigureNotify(HybridSim_C_Wrapper_t *hsc, uint operation, bool enable)
	{
		hsc->hs->ConfigureNotify(operation, enable);
	}

	void HybridSim_C_delete(HybridSim_C_Wrapper_t *hsc)
	{
		delete hsc;
	}

}

} // Namespace HybridSim
