#ifndef FDSIM_H
#define FDSIM_H
//FlashDIMM.h
//Header for flash flash dimm system wrapper

namespace FDSim
{
	// SimObj.h stuff
        class SimObj
        {
        public:
                uint64_t currentClockCycle;

                void step();
                virtual void update()=0;
        };

	// FlashTransaction.h stuff
        enum TransactionType
        {
                DATA_READ,
                DATA_WRITE,
                RETURN_DATA
        };

        class FlashTransaction
        {
        public:
                //fields
                TransactionType transactionType;
                uint64_t address;
                void *data;
                uint64_t timeAdded;
                uint64_t timeReturned;


                //functions
                FlashTransaction(TransactionType transType, uint64_t addr, void *data);
                FlashTransaction();

                void print();
        };


	// Callbacks.h stuff
	template <typename ReturnT, typename Param1T, typename Param2T,
			    typename Param3T>
	class CallbackBase
	{
	    public:
		virtual ~CallbackBase() = 0;
		virtual ReturnT operator()(Param1T, Param2T, Param3T) = 0;
	};

	template <typename Return, typename Param1T, typename Param2T, typename Param3T>
	FDSim::CallbackBase<Return,Param1T,Param2T,Param3T>::~CallbackBase(){}

	template <typename ConsumerT, typename ReturnT,
				typename Param1T, typename Param2T, typename Param3T >
	class Callback: public CallbackBase<ReturnT,Param1T,Param2T,Param3T>
	{
	    private:
		typedef ReturnT (ConsumerT::*PtrMember)(Param1T,Param2T,Param3T);

	    public:
		Callback( ConsumerT* const object, PtrMember member) :
			object(object), member(member) {
		}

		Callback( const Callback<ConsumerT,ReturnT,Param1T,Param2T,Param3T>& e ) :
			object(e.object), member(e.member) {
		}

		ReturnT operator()(Param1T param1, Param2T param2, Param3T param3) {
		    return (const_cast<ConsumerT*>(object)->*member)
							    (param1,param2,param3);
		}

	    private:

		ConsumerT* const object;
		const PtrMember  member;
	};


	// FlashDIMM.h stuff
	typedef CallbackBase<void,uint,uint64_t,uint64_t> Callback_t;
	class FlashDIMM : public SimObj{
		public:
			FlashDIMM(uint id, string dev, string sys, string pwd, string trc);
			void update(void);
			bool add(FlashTransaction &trans);
			void RegisterCallbacks(Callback_t *readDone, Callback_t *writeDone);

	};
}
#endif

