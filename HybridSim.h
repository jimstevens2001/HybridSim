#ifndef HYBRIDSIM_H
#define HYBRIDSIM_H

//#include <FlashDIMMSim/FlashDIMM.h>

#include "config.h"

namespace HybridSim
{
	class HybridSystem: public SimulatorObject
	{
		public:
		HybridSystem(uint id);
		void update();
		bool addTransaction(Transaction &trans);
		void RegisterCallbacks(
			    DRAMSim::TransactionCompleteCB *readDone,
			    DRAMSim::TransactionCompleteCB *writeDone,
			    void (*reportPower)(double bgpower, double burstpower, double refreshpower, double actprepower));


	};

}

#endif

