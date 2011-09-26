#ifndef HYBRIDSIM_TRANSACTION_H
#define HYBRIDSIM_TRANSACTION_H

namespace HybridSim
{

	enum TransactionType
	{
		DATA_READ,
		DATA_WRITE,
		RETURN_DATA
	};

	class Transaction
	{
		public:
			//fields
			TransactionType transactionType;
			uint64_t address;
			void *data;

			//functions
			Transaction(TransactionType transType, uint64_t addr, void *data)
			{
				transactionType = transType;
				address = addr;
				data = data;
			}

			Transaction() {}

			//void print();
	};
}

#endif
